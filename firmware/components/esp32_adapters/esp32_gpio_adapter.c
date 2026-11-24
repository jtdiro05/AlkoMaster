/*
 * Generic ESP-IDF GPIO and time adapter - device agnostic.
 */
#include "esp32_gpio_adapter.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

esp_err_t esp32_gpio_init(esp32_gpio_ctx_t *ctx, const esp32_gpio_config_t *config)
{
    if (ctx == NULL || config == NULL || !GPIO_IS_VALID_GPIO(config->pin)) {
        return ESP_ERR_INVALID_ARG;
    }

    const gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << config->pin),
        .mode = config->mode,
        .pull_up_en = config->pull_up ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = config->pull_down ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&io_cfg);
    if (err != ESP_OK) {
        return err;
    }

    ctx->pin = config->pin;
    ctx->initialized = true;
    return ESP_OK;
}

esp_err_t esp32_gpio_deinit(esp32_gpio_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = gpio_reset_pin(ctx->pin);
    ctx->initialized = false;
    return err;
}

int esp32_gpio_read_level(void *handle)
{
    if (handle == NULL) {
        return ESP32_GPIO_ERR_INVALID_ARG;
    }

    const esp32_gpio_ctx_t *ctx = (const esp32_gpio_ctx_t *)handle;
    if (!ctx->initialized) {
        return ESP32_GPIO_ERR_INVALID_ARG;
    }

    return gpio_get_level(ctx->pin);
}

int esp32_gpio_write_level(void *handle, int level)
{
    if (handle == NULL) {
        return ESP32_GPIO_ERR_INVALID_ARG;
    }

    const esp32_gpio_ctx_t *ctx = (const esp32_gpio_ctx_t *)handle;
    if (!ctx->initialized) {
        return ESP32_GPIO_ERR_INVALID_ARG;
    }

    esp_err_t err = gpio_set_level(ctx->pin, level ? 1 : 0);
    return (err == ESP_OK) ? ESP32_GPIO_OK : ESP32_GPIO_ERR_INVALID_ARG;
}

uint32_t esp32_get_time_ms(void)
{
    return (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
}
