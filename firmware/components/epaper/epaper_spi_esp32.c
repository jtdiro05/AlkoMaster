#include "epaper_spi_adapter.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "epaper_spi_esp32";

/** Maximum single SPI transaction payload size in bytes */
#define ESP32_SPI_MAX_CHUNK_SIZE 4096

/**
 * @brief Pre-transmission callback setting the D/C# line before CS# is asserted.
 *        Transaction user pointer encodes the D/C# pin level (0 = Command, 1 = Data).
 */
typedef struct {
    gpio_num_t dc_pin;
    uint8_t dc_level;
} epaper_spi_trans_ext_t;

static void IRAM_ATTR epaper_spi_pre_transfer_cb(spi_transaction_t *trans)
{
    const epaper_spi_trans_ext_t *meta = (const epaper_spi_trans_ext_t *)trans->user;
    if (meta != NULL && meta->dc_pin != GPIO_NUM_NC) {
        gpio_set_level(meta->dc_pin, meta->dc_level);
    }
}

static esp_err_t esp32_spi_adapter_init(void *ctx)
{
    epaper_esp32_spi_ctx_t *esp_ctx = (epaper_esp32_spi_ctx_t *)ctx;
    if (esp_ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (esp_ctx->initialized) {
        return ESP_OK;
    }

    const epaper_esp32_spi_config_t *cfg = &esp_ctx->config;

    /* Configure D/C# and RES# output pins */
    uint64_t out_pin_mask = 0;
    if (cfg->dc_pin != GPIO_NUM_NC) {
        out_pin_mask |= (1ULL << cfg->dc_pin);
    }
    if (cfg->rst_pin != GPIO_NUM_NC) {
        out_pin_mask |= (1ULL << cfg->rst_pin);
    }

    if (out_pin_mask != 0) {
        gpio_config_t io_conf = {
            .pin_bit_mask = out_pin_mask,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        esp_err_t err = gpio_config(&io_conf);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure output GPIOs: %s", esp_err_to_name(err));
            return err;
        }
    }

    if (cfg->dc_pin != GPIO_NUM_NC) {
        gpio_set_level(cfg->dc_pin, 0);
    }
    if (cfg->rst_pin != GPIO_NUM_NC) {
        gpio_set_level(cfg->rst_pin, 1);
    }

    /* Configure BUSY input pin (Active High on SSD1680) */
    if (cfg->busy_pin != GPIO_NUM_NC) {
        gpio_config_t busy_conf = {
            .pin_bit_mask = (1ULL << cfg->busy_pin),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        esp_err_t err = gpio_config(&busy_conf);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure BUSY GPIO: %s", esp_err_to_name(err));
            return err;
        }
    }

    /* Initialize the ESP32 SPI bus if requested */
    if (cfg->init_spi_bus) {
        spi_bus_config_t buscfg = {
            .mosi_io_num = cfg->mosi_pin,
            .miso_io_num = -1, /* 4-wire write-only mode uses unidirectional SDA */
            .sclk_io_num = cfg->sclk_pin,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = (cfg->max_transfer_sz > 0) ? cfg->max_transfer_sz : ESP32_SPI_MAX_CHUNK_SIZE,
        };

        esp_err_t err = spi_bus_initialize(cfg->spi_host, &buscfg, SPI_DMA_CH_AUTO);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(err));
            return err;
        }
        esp_ctx->bus_owned = true;
    }

    /* Attach SSD1680 device to SPI bus (Mode 0: CPOL=0, CPHA=0, MSB first per Section 6.1.2) */
    int clk_hz = (cfg->clock_speed_hz > 0) ? cfg->clock_speed_hz : SSD1680_SPI_DEFAULT_CLOCK_HZ;
    if (clk_hz > SSD1680_SPI_MAX_WRITE_HZ) {
        clk_hz = SSD1680_SPI_MAX_WRITE_HZ;
    }

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = clk_hz,
        .mode = 0,
        .spics_io_num = cfg->cs_pin,
        .queue_size = 4,
        .pre_cb = epaper_spi_pre_transfer_cb,
        .flags = SPI_DEVICE_HALFDUPLEX,
    };

    esp_err_t err = spi_bus_add_device(cfg->spi_host, &devcfg, &esp_ctx->spi_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SSD1680 SPI device: %s", esp_err_to_name(err));
        if (esp_ctx->bus_owned) {
            spi_bus_free(cfg->spi_host);
            esp_ctx->bus_owned = false;
        }
        return err;
    }

    esp_ctx->initialized = true;
    ESP_LOGI(TAG, "ESP32 SPI adapter initialized (host=%d, clk=%d Hz, CS=%d, DC=%d, RST=%d, BUSY=%d)",
             cfg->spi_host, clk_hz, cfg->cs_pin, cfg->dc_pin, cfg->rst_pin, cfg->busy_pin);
    return ESP_OK;
}

static esp_err_t esp32_spi_adapter_deinit(void *ctx)
{
    epaper_esp32_spi_ctx_t *esp_ctx = (epaper_esp32_spi_ctx_t *)ctx;
    if (esp_ctx == NULL || !esp_ctx->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (esp_ctx->spi_handle != NULL) {
        spi_bus_remove_device(esp_ctx->spi_handle);
        esp_ctx->spi_handle = NULL;
    }

    if (esp_ctx->bus_owned) {
        spi_bus_free(esp_ctx->config.spi_host);
        esp_ctx->bus_owned = false;
    }

    esp_ctx->initialized = false;
    ESP_LOGI(TAG, "ESP32 SPI adapter deinitialized");
    return ESP_OK;
}

static esp_err_t esp32_spi_adapter_write_cmd(void *ctx, uint8_t cmd)
{
    epaper_esp32_spi_ctx_t *esp_ctx = (epaper_esp32_spi_ctx_t *)ctx;
    if (esp_ctx == NULL || !esp_ctx->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    epaper_spi_trans_ext_t meta = {
        .dc_pin = esp_ctx->config.dc_pin,
        .dc_level = 0, /* D/C# LOW for command byte */
    };

    spi_transaction_t trans;
    memset(&trans, 0, sizeof(trans));
    trans.length = 8;
    trans.tx_data[0] = cmd;
    trans.flags = SPI_TRANS_USE_TXDATA;
    trans.user = &meta;

    return spi_device_polling_transmit(esp_ctx->spi_handle, &trans);
}

static esp_err_t esp32_spi_adapter_write_data(void *ctx, const uint8_t *data, size_t len)
{
    epaper_esp32_spi_ctx_t *esp_ctx = (epaper_esp32_spi_ctx_t *)ctx;
    if (esp_ctx == NULL || !esp_ctx->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    epaper_spi_trans_ext_t meta = {
        .dc_pin = esp_ctx->config.dc_pin,
        .dc_level = 1, /* D/C# HIGH for data bytes */
    };

    size_t max_chunk = (esp_ctx->config.max_transfer_sz > 0)
                           ? (size_t)esp_ctx->config.max_transfer_sz
                           : ESP32_SPI_MAX_CHUNK_SIZE;

    size_t offset = 0;
    while (offset < len) {
        size_t chunk_len = len - offset;
        if (chunk_len > max_chunk) {
            chunk_len = max_chunk;
        }

        spi_transaction_t trans;
        memset(&trans, 0, sizeof(trans));
        trans.length = chunk_len * 8;
        trans.tx_buffer = data + offset;
        trans.user = &meta;

        esp_err_t err = spi_device_polling_transmit(esp_ctx->spi_handle, &trans);
        if (err != ESP_OK) {
            return err;
        }
        offset += chunk_len;
    }

    return ESP_OK;
}

static esp_err_t esp32_spi_adapter_set_reset(void *ctx, bool level)
{
    epaper_esp32_spi_ctx_t *esp_ctx = (epaper_esp32_spi_ctx_t *)ctx;
    if (esp_ctx == NULL || !esp_ctx->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (esp_ctx->config.rst_pin == GPIO_NUM_NC) {
        return ESP_OK;
    }
    return gpio_set_level(esp_ctx->config.rst_pin, level ? 1 : 0);
}

static bool esp32_spi_adapter_is_busy(void *ctx)
{
    epaper_esp32_spi_ctx_t *esp_ctx = (epaper_esp32_spi_ctx_t *)ctx;
    if (esp_ctx == NULL || !esp_ctx->initialized || esp_ctx->config.busy_pin == GPIO_NUM_NC) {
        return false;
    }
    /* Table 5-2: When BUSY is High, operation should not be interrupted */
    return gpio_get_level(esp_ctx->config.busy_pin) != 0;
}

static void esp32_spi_adapter_delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms > 0 ? ms : 1));
}

esp_err_t epaper_spi_esp32_create_adapter(epaper_esp32_spi_ctx_t *esp_ctx,
                                          const epaper_esp32_spi_config_t *config,
                                          epaper_spi_adapter_t *out_adapter)
{
    if (esp_ctx == NULL || config == NULL || out_adapter == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(esp_ctx, 0, sizeof(epaper_esp32_spi_ctx_t));
    esp_ctx->config = *config;

    out_adapter->ctx = esp_ctx;
    out_adapter->init = esp32_spi_adapter_init;
    out_adapter->deinit = esp32_spi_adapter_deinit;
    out_adapter->write_cmd = esp32_spi_adapter_write_cmd;
    out_adapter->write_data = esp32_spi_adapter_write_data;
    out_adapter->set_reset = esp32_spi_adapter_set_reset;
    out_adapter->is_busy = esp32_spi_adapter_is_busy;
    out_adapter->delay_ms = esp32_spi_adapter_delay_ms;

    return ESP_OK;
}
