#include <stdbool.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#include "epaper.h"
#include "epaper_spi_adapter.h"
#include "dht22.h"
#include "mq3.h"
#include "esp32_dht_adapter.h"
#include "esp32_adc_adapter.h"
#include "esp32_gpio_adapter.h"

static const char *TAG = "main";

/* Default ESP32-S3 SPI and control pin assignment for SSD1680 e-Paper */
#define EPAPER_SPI_HOST     SPI2_HOST
#define EPAPER_PIN_MOSI     GPIO_NUM_11
#define EPAPER_PIN_SCLK     GPIO_NUM_12
#define EPAPER_PIN_CS       GPIO_NUM_10
#define EPAPER_PIN_DC       GPIO_NUM_18
#define EPAPER_PIN_RST      GPIO_NUM_8
#define EPAPER_PIN_BUSY     GPIO_NUM_3

/* 2.9" SSD1680 panel resolution (128x296) */
#define EPAPER_PANEL_WIDTH  EPAPER_2IN9_WIDTH
#define EPAPER_PANEL_HEIGHT EPAPER_2IN9_HEIGHT

/* DHT22 single-wire DATA pin configuration */
#define DHT22_PIN_DATA      GPIO_NUM_6

/* MQ-3 analog input configuration (ADC_UNIT_1, ADC_CHANNEL_3 -> GPIO 4 on ESP32-S3) */
#define MQ3_ADC_UNIT        ADC_UNIT_1
#define MQ3_ADC_CHANNEL     ADC_CHANNEL_3
#define MQ3_VCC_MV          3300.0f
#define MQ3_RL_KOHM         4.7f    /* Waveshare MQ-3 onboard load resistor RL = 4.7 kOhm (SMD "472") */
#define MQ3_DEFAULT_R0_KOHM 1.0f
#define MQ3_CALIB_SAMPLES   20u

static void build_labeled_value(char *out_buf, size_t out_size,
                                const char *prefix, float value, const char *suffix)
{
    char num_str[16];
    float_to_str_1dec(value, num_str, sizeof(num_str));

    out_buf[0] = '\0';
    strncat(out_buf, prefix, out_size - 1);
    strncat(out_buf, num_str, out_size - strlen(out_buf) - 1);
    strncat(out_buf, suffix, out_size - strlen(out_buf) - 1);
}

void app_main(void)
{
    static epaper_esp32_spi_ctx_t spi_ctx;
    static epaper_spi_adapter_t spi_adapter;
    static epaper_display_t display;

    static esp32_dht_ctx_t dht_ctx;
    static dht22_dev_t dht_dev;

    static esp32_adc_ctx_t mq3_adc_ctx;
    static mq3_sensor_t mq3_dev;

    /* 1. Initialize SSD1680 e-Paper display */
    const epaper_esp32_spi_config_t spi_cfg = {
        .spi_host = EPAPER_SPI_HOST,
        .mosi_pin = EPAPER_PIN_MOSI,
        .sclk_pin = EPAPER_PIN_SCLK,
        .cs_pin = EPAPER_PIN_CS,
        .dc_pin = EPAPER_PIN_DC,
        .rst_pin = EPAPER_PIN_RST,
        .busy_pin = EPAPER_PIN_BUSY,
        .clock_speed_hz = SSD1680_SPI_DEFAULT_CLOCK_HZ,
        .max_transfer_sz = 4096,
        .init_spi_bus = true,
    };
    ESP_ERROR_CHECK(epaper_spi_esp32_create_adapter(&spi_ctx, &spi_cfg, &spi_adapter));

    const epaper_panel_config_t panel_cfg = {
        .width = EPAPER_PANEL_WIDTH,
        .height = EPAPER_PANEL_HEIGHT,
        .data_entry_mode = SSD1680_DATA_ENTRY_X_INC_Y_INC,
        .border_waveform = 0x05,
        .busy_timeout_ms = 5000,
    };
    ESP_LOGI(TAG, "Initializing SSD1680 e-Paper display...");
    ESP_ERROR_CHECK(epaper_init(&display, &panel_cfg, &spi_adapter));

    /* 2. Initialize DHT22 temperature & humidity sensor */
    ESP_LOGI(TAG, "Initializing DHT22 sensor on GPIO %d...", (int)DHT22_PIN_DATA);
    ESP_ERROR_CHECK(esp32_dht_init(&dht_ctx, DHT22_PIN_DATA));

    const dht22_bus_t dht_bus = {
        .handle = &dht_ctx,
        .read_raw = esp32_dht_read_raw,
        .get_time_ms = esp32_get_time_ms,
    };
    dht22_err_t dht_err = dht22_init(&dht_dev, &dht_bus);
    if (dht_err != DHT22_OK) {
        ESP_LOGE(TAG, "DHT22 init failed: %d", (int)dht_err);
    }

    /* 3. Initialize MQ-3 alcohol sensor and ADC adapter */
    ESP_LOGI(TAG, "Initializing MQ-3 ADC adapter (unit=%d, ch=%d)...",
             (int)MQ3_ADC_UNIT, (int)MQ3_ADC_CHANNEL);
    const esp32_adc_config_t mq3_adc_cfg = {
        .unit_handle = NULL,
        .unit_id = MQ3_ADC_UNIT,
        .channel = MQ3_ADC_CHANNEL,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(esp32_adc_init(&mq3_adc_ctx, &mq3_adc_cfg));

    const mq3_adc_t mq3_adc_if = {
        .handle = &mq3_adc_ctx,
        .read_raw = esp32_adc_read_raw,
        .read_mv = esp32_adc_read_mv,
    };
    const mq3_config_t mq3_cfg = {
        .vcc_mv = MQ3_VCC_MV,
        .rl_kohm = MQ3_RL_KOHM,
        .r0_kohm = MQ3_DEFAULT_R0_KOHM,
        .clean_air_ratio = MQ3_DEFAULT_CLEAN_AIR_RATIO,
        .preheat_time_ms = 0,
    };
    mq3_err_t mq3_err = mq3_init(&mq3_dev, &mq3_adc_if, &mq3_cfg);
    if (mq3_err == MQ3_OK) {
        /* Try initial clean-air calibration; keep fallback R0 if voltage is out of range */
        mq3_err_t cal_err = mq3_calibrate(&mq3_dev, MQ3_CALIB_SAMPLES);
        if (cal_err == MQ3_OK) {
            ESP_LOGI(TAG, "MQ-3 calibrated in clean air (R0 = %.2f kOhm)", mq3_dev.config.r0_kohm);
        } else {
            mq3_dev.state = MQ3_STATE_READY;
            ESP_LOGW(TAG, "MQ-3 auto-calibration skipped (err=%d), using default R0=%.2f kOhm",
                     (int)cal_err, mq3_dev.config.r0_kohm);
        }
    } else {
        ESP_LOGE(TAG, "MQ-3 init failed: %d", (int)mq3_err);
    }

    char line_temp[32];
    char line_hum[32];
    char line_alko[32];
    char line_mq3_mv[32];

    /* 4. Main measurement and display loop (every 5 seconds) */
    while (1) {
        /* --- Read DHT22 --- */
        dht22_data_t dht_data = { 0 };
        dht22_err_t dht_rc = dht22_get_data(&dht_dev, &dht_data);
        if (dht_rc == DHT22_OK) {
            ESP_LOGI(TAG, "DHT22 -> Temp: %.1f C, Hum: %.1f %%RH",
                     dht_data.temperature_c, dht_data.humidity_rh);
            build_labeled_value(line_temp, sizeof(line_temp), "Temp: ", dht_data.temperature_c, " C");
            build_labeled_value(line_hum, sizeof(line_hum),   "Wilg: ", dht_data.humidity_rh,   " %");
        } else {
            ESP_LOGW(TAG, "DHT22 read failed (err=%d)", (int)dht_rc);
            strcpy(line_temp, "Temp: DHT ERR");
            strcpy(line_hum,  "Wilg: DHT ERR");
        }

        /* --- Read MQ-3 --- */
        mq3_data_t mq3_data = { 0 };
        mq3_err_t mq3_rc = mq3_get_data(&mq3_dev, &mq3_data);
        if (mq3_rc == MQ3_OK) {
            ESP_LOGI(TAG, "MQ-3  -> Vout: %.1f mV (raw=%d), Rs: %.2f kOhm, Alko: %.2f mg/L (%.2f prom)",
                     mq3_data.voltage_mv, mq3_data.raw_adc, mq3_data.rs_kohm,
                     mq3_data.alcohol_mg_l, mq3_data.promille);
            build_labeled_value(line_alko,   sizeof(line_alko),   "Alko: ", mq3_data.promille,   " prom");
            build_labeled_value(line_mq3_mv, sizeof(line_mq3_mv), "MQ3:  ", mq3_data.voltage_mv, " mV");
        } else {
            ESP_LOGW(TAG, "MQ-3 read failed (err=%d)", (int)mq3_rc);
            strcpy(line_alko,   "Alko: MQ3 ERR");
            strcpy(line_mq3_mv, "MQ3:  --- mV");
        }

        /* --- Render results on e-Paper (296x128 landscape, font24 height = 24 px) --- */
        epaper_fill(&display, EPAPER_COLOR_WHITE);
        epaper_draw_string(&display, 0, 0,   "AlkoMaster", &epaper_font24, EPAPER_COLOR_BLACK);
        epaper_draw_string(&display, 0, 26,  line_temp,    &epaper_font24, EPAPER_COLOR_BLACK);
        epaper_draw_string(&display, 0, 52,  line_hum,     &epaper_font24, EPAPER_COLOR_BLACK);
        epaper_draw_string(&display, 0, 78,  line_alko,    &epaper_font24, EPAPER_COLOR_BLACK);
        epaper_draw_string(&display, 0, 104, line_mq3_mv,  &epaper_font24, EPAPER_COLOR_BLACK);

        epaper_write_framebuffer(&display, display.framebuffer, SSD1680_UPDATE_FULL_MODE_1);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
