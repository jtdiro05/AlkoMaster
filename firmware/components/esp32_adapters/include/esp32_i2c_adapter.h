#pragma once

/*
 * Generic ESP-IDF I2C register-access adapter.
 *
 * Knows NOTHING about any particular sensor. It only offers functions with a
 * common "register read/write + delay" contract, so they can be assigned to
 * any sensor driver's bus interface whose function-pointer types match, e.g.:
 *
 *   bme280_bus_t bus = {
 *       .handle   = dev_handle,       // i2c_master_dev_handle_t -> void *
 *       .read     = esp32_i2c_read,   // matches bme280_read_fn
 *       .write    = esp32_i2c_write,  // matches bme280_write_fn
 *       .delay_ms = esp32_delay_ms,   // matches bme280_delay_ms_fn
 *   };
 *
 * Handle: `void *handle` MUST be an i2c_master_dev_handle_t created with
 * i2c_master_bus_add_device(). In the ESP-IDF i2c_master driver the slave
 * address is bound to the device handle at creation time, so `dev_addr` is
 * only used for validation/diagnostics and must match that address.
 *
 * Return values: 0 on success, negative value on error (see ESP32_I2C_ERR_*).
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP32_I2C_OK              0
#define ESP32_I2C_ERR_INVALID_ARG (-1) /**< NULL handle/buffer or len == 0 for read */
#define ESP32_I2C_ERR_TIMEOUT     (-2) /**< ESP_ERR_TIMEOUT */
#define ESP32_I2C_ERR_NACK        (-3) /**< ESP_ERR_NOT_FOUND / ESP_FAIL (no ACK) */
#define ESP32_I2C_ERR_BUS         (-4) /**< Any other esp_err_t or memory allocation failure */

/** Stack buffer threshold for esp32_i2c_write() (payloads above this use heap fallback) */
#define ESP32_I2C_STACK_WRITE_LEN 32

/** Default transaction timeout [ms] */
#define ESP32_I2C_TIMEOUT_MS      100

/**
 * @brief Read @p len bytes starting at @p reg_addr (write reg, repeated START, read)
 * @param handle   i2c_master_dev_handle_t cast to void *
 * @param dev_addr 7-bit address (informational; bound to the handle)
 */
int esp32_i2c_read(void *handle, uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, size_t len);

/**
 * @brief Write @p len bytes starting at @p reg_addr in a single transaction
 * @param handle   i2c_master_dev_handle_t cast to void *
 * @param dev_addr 7-bit address (informational; bound to the handle)
 */
int esp32_i2c_write(void *handle, uint8_t dev_addr, uint8_t reg_addr, const uint8_t *data, size_t len);

/**
 * @brief Blocking delay (FreeRTOS), at least one tick for ms > 0
 */
void esp32_delay_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif
