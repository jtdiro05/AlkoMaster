#pragma once

/*
 * Generic ESP-IDF single-wire pulse-timing adapter for DHT11/DHT22 sensors.
 *
 * Knows NOTHING about high-level sensor structures (does NOT include dht22.h).
 * Captures a 40-bit (5-byte) frame using open-drain GPIO and microsecond timing
 * inside a critical section so FreeRTOS/Wi-Fi interrupts cannot corrupt pulse
 * width measurements.
 *
 *   dht22_bus_t bus = {
 *       .handle      = &dht_ctx,           // esp32_dht_ctx_t * -> void *
 *       .read_raw    = esp32_dht_read_raw, // matches dht22_read_raw_fn
 *       .get_time_ms = esp32_get_time_ms,  // from esp32_gpio_adapter.h (optional)
 *   };
 */

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP32_DHT_OK              0
#define ESP32_DHT_ERR_INVALID_ARG (-1) /**< NULL handle/buffer or uninitialized context */
#define ESP32_DHT_ERR_TIMEOUT     (-2) /**< Sensor did not respond or pulse timed out */

/**
 * @brief Runtime context for an ESP32 DHT single-wire data pin
 */
typedef struct {
    gpio_num_t pin;   /**< Single-wire DATA pin */
    bool initialized; /**< Adapter initialization flag */
} esp32_dht_ctx_t;

/**
 * @brief Configure the GPIO pin in Open-Drain mode with internal pull-up enabled
 *
 * @param[out] ctx Pointer to the adapter context
 * @param[in]  pin GPIO pin connected to the DHT DATA line
 * @return esp_err_t ESP_OK on success
 */
esp_err_t esp32_dht_init(esp32_dht_ctx_t *ctx, gpio_num_t pin);

/**
 * @brief Reset the GPIO pin and mark the adapter context uninitialized
 */
esp_err_t esp32_dht_deinit(esp32_dht_ctx_t *ctx);

/**
 * @brief Send the start signal and capture 5 raw bytes (40 bits) from the sensor
 *
 * @param handle    esp32_dht_ctx_t * cast to void *
 * @param[out] data 5-byte buffer receiving the raw 40-bit frame
 * @return 0 (ESP32_DHT_OK) on success, negative error code on timeout
 */
int esp32_dht_read_raw(void *handle, uint8_t data[5]);

#ifdef __cplusplus
}
#endif
