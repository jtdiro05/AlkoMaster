/*
 * Generic ESP-IDF single-wire pulse-timing adapter for DHT sensors.
 */
#include "esp32_dht_adapter.h"

#include <string.h>
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#define DHT_START_LOW_US     1200u /**< Host start pulse low duration (>1 ms for DHT22) */
#define DHT_START_RELEASE_US   30u /**< Host release wait before sensor ACK */
#define DHT_ACK_TIMEOUT_US    120u /**< Max duration of sensor 80 us LOW/HIGH ACK phases */
#define DHT_BIT_LOW_MAX_US    100u /**< Max duration of 50 us bit-start LOW phase */
#define DHT_BIT_HIGH_MAX_US   120u /**< Max duration of 26..70 us bit-data HIGH phase */

static portMUX_TYPE s_dht_spinlock = portMUX_INITIALIZER_UNLOCKED;

/* Measure how many microseconds the pin stays at @p expected_level (returns -1 on timeout) */
static int wait_level_us(gpio_num_t pin, int expected_level, uint32_t timeout_us)
{
    uint32_t elapsed = 0;
    while (gpio_get_level(pin) == expected_level) {
        if (elapsed >= timeout_us) {
            return -1;
        }
        esp_rom_delay_us(1);
        elapsed++;
    }
    return (int)elapsed;
}

esp_err_t esp32_dht_init(esp32_dht_ctx_t *ctx, gpio_num_t pin)
{
    if (ctx == NULL || !GPIO_IS_VALID_OUTPUT_GPIO(pin)) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Idle state of the single-wire bus is HIGH */
    gpio_set_level(pin, 1);

    const gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&io_cfg);
    if (err != ESP_OK) {
        return err;
    }

    gpio_set_level(pin, 1);
    ctx->pin = pin;
    ctx->initialized = true;
    return ESP_OK;
}

esp_err_t esp32_dht_deinit(esp32_dht_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = gpio_reset_pin(ctx->pin);
    ctx->initialized = false;
    return err;
}

int esp32_dht_read_raw(void *handle, uint8_t data[5])
{
    if (handle == NULL || data == NULL) {
        return ESP32_DHT_ERR_INVALID_ARG;
    }

    const esp32_dht_ctx_t *ctx = (const esp32_dht_ctx_t *)handle;
    if (!ctx->initialized) {
        return ESP32_DHT_ERR_INVALID_ARG;
    }

    const gpio_num_t pin = ctx->pin;
    memset(data, 0, 5);

    /* 1. Host start signal: pull LOW for 1.2 ms (interrupts can remain enabled here) */
    gpio_set_level(pin, 0);
    esp_rom_delay_us(DHT_START_LOW_US);

    /* 2. Time-critical phase (~4 ms): disable interrupts and capture 40 bits */
    portENTER_CRITICAL(&s_dht_spinlock);

    /* Release line HIGH and wait for sensor to pull it LOW */
    gpio_set_level(pin, 1);
    esp_rom_delay_us(DHT_START_RELEASE_US);

    if (wait_level_us(pin, 1, DHT_ACK_TIMEOUT_US) < 0 ||
        wait_level_us(pin, 0, DHT_ACK_TIMEOUT_US) < 0 ||
        wait_level_us(pin, 1, DHT_ACK_TIMEOUT_US) < 0) {
        portEXIT_CRITICAL(&s_dht_spinlock);
        return ESP32_DHT_ERR_TIMEOUT;
    }

    /* 3. Read 40 bits: compare HIGH pulse width against the preceding ~50 us LOW pulse */
    for (int i = 0; i < 40; i++) {
        int low_us = wait_level_us(pin, 0, DHT_BIT_LOW_MAX_US);
        if (low_us < 0) {
            portEXIT_CRITICAL(&s_dht_spinlock);
            return ESP32_DHT_ERR_TIMEOUT;
        }

        int high_us = wait_level_us(pin, 1, DHT_BIT_HIGH_MAX_US);
        if (high_us < 0) {
            portEXIT_CRITICAL(&s_dht_spinlock);
            return ESP32_DHT_ERR_TIMEOUT;
        }

        data[i / 8] <<= 1;
        /* '0' is ~27 us HIGH, '1' is ~70 us HIGH (comparing against low_us ~50 us is drift-immune) */
        if (high_us > low_us) {
            data[i / 8] |= 1u;
        }
    }

    portEXIT_CRITICAL(&s_dht_spinlock);
    return ESP32_DHT_OK;
}
