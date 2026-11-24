#pragma once

/*
 * Generic ESP-IDF GPIO and monotonic clock adapter.
 *
 * Knows NOTHING about any higher-level module (does NOT include button.h,
 * mq3.h, etc.). Its callback signatures match generic driver contracts, e.g.:
 *
 *   button_io_t btn_io = {
 *       .handle      = &btn_gpio_ctx,         // esp32_gpio_ctx_t * -> void *
 *       .read_level  = esp32_gpio_read_level, // matches button_read_level_fn
 *       .get_time_ms = esp32_get_time_ms,     // matches button_get_time_ms_fn
 *   };
 */

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP32_GPIO_OK              0
#define ESP32_GPIO_ERR_INVALID_ARG (-1)

/**
 * @brief Configuration for a single ESP32 GPIO pin
 */
typedef struct {
    gpio_num_t pin;  /**< GPIO pin number (e.g. GPIO_NUM_0 for the BOOT button) */
    gpio_mode_t mode;/**< Pin direction (e.g. GPIO_MODE_INPUT or GPIO_MODE_OUTPUT) */
    bool pull_up;    /**< Enable internal pull-up resistor */
    bool pull_down;  /**< Enable internal pull-down resistor */
} esp32_gpio_config_t;

/**
 * @brief Runtime context for a configured ESP32 GPIO pin (passed as `void *handle`)
 */
typedef struct {
    gpio_num_t pin;  /**< Configured GPIO pin */
    bool initialized;/**< Initialization flag */
} esp32_gpio_ctx_t;

/**
 * @brief Configure the ESP32 GPIO pin and initialize the adapter context
 */
esp_err_t esp32_gpio_init(esp32_gpio_ctx_t *ctx, const esp32_gpio_config_t *config);

/**
 * @brief Reset the GPIO pin to default state and mark context uninitialized
 */
esp_err_t esp32_gpio_deinit(esp32_gpio_ctx_t *ctx);

/**
 * @brief Read logic level from the GPIO pin
 * @param handle esp32_gpio_ctx_t * cast to void *
 * @return 0 (LOW), 1 (HIGH), or negative error code (ESP32_GPIO_ERR_INVALID_ARG)
 */
int esp32_gpio_read_level(void *handle);

/**
 * @brief Set output logic level on the GPIO pin
 * @param handle esp32_gpio_ctx_t * cast to void *
 * @param level  0 for LOW, non-zero for HIGH
 * @return 0 (ESP32_GPIO_OK) on success, negative error code on failure
 */
int esp32_gpio_write_level(void *handle, int level);

/**
 * @brief Return monotonic system uptime in milliseconds [ms] (FreeRTOS ticks)
 */
uint32_t esp32_get_time_ms(void);

#ifdef __cplusplus
}
#endif
