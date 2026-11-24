/*
 * Generic ESP-IDF I2C adapter - sensor agnostic (does NOT include any sensor header).
 */
#include "esp32_i2c_adapter.h"

#include <stdlib.h>
#include <string.h>
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "esp32_i2c";

/* Map esp_err_t (ESP_OK / positive codes / ESP_FAIL) to 0 / negative adapter codes */
static int map_err(esp_err_t err, uint8_t dev_addr, uint8_t reg_addr)
{
    if (err == ESP_OK) {
        return ESP32_I2C_OK;
    }
    ESP_LOGE(TAG, "I2C error on dev 0x%02X, reg 0x%02X: %s (%s)",
             dev_addr, reg_addr, esp_err_to_name(err),
             (err == ESP_ERR_NOT_FOUND) ? "NACK: no device responded at this address" :
             (err == ESP_ERR_TIMEOUT)   ? "TIMEOUT: check pull-up resistors and bus wiring" :
             "bus error");
    switch (err) {
    case ESP_ERR_INVALID_ARG: return ESP32_I2C_ERR_INVALID_ARG;
    case ESP_ERR_TIMEOUT:     return ESP32_I2C_ERR_TIMEOUT;
    case ESP_ERR_NOT_FOUND:
    case ESP_FAIL:            return ESP32_I2C_ERR_NACK;
    default:                  return ESP32_I2C_ERR_BUS;
    }
}

int esp32_i2c_read(void *handle, uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, size_t len)
{
    if (handle == NULL || data == NULL || len == 0) {
        return ESP32_I2C_ERR_INVALID_ARG;
    }
    /* void * -> concrete ESP-IDF device handle (address already bound to it) */
    i2c_master_dev_handle_t dev = (i2c_master_dev_handle_t)handle;

    esp_err_t err = i2c_master_transmit_receive(dev, &reg_addr, 1, data, len, ESP32_I2C_TIMEOUT_MS);
    return map_err(err, dev_addr, reg_addr);
}

int esp32_i2c_write(void *handle, uint8_t dev_addr, uint8_t reg_addr, const uint8_t *data, size_t len)
{
    if (handle == NULL || (data == NULL && len > 0)) {
        return ESP32_I2C_ERR_INVALID_ARG;
    }
    i2c_master_dev_handle_t dev = (i2c_master_dev_handle_t)handle;

    /* Register address and payload must go in one transaction (single START/STOP).
     * Small payloads use a stack buffer; larger ones fall back to the heap. */
    uint8_t stack_buf[1 + ESP32_I2C_STACK_WRITE_LEN];
    uint8_t *buf = stack_buf;
    if (len > ESP32_I2C_STACK_WRITE_LEN) {
        buf = malloc(len + 1);
        if (buf == NULL) {
            return ESP32_I2C_ERR_BUS;
        }
    }

    buf[0] = reg_addr;
    if (len > 0) {
        memcpy(&buf[1], data, len);
    }

    esp_err_t err = i2c_master_transmit(dev, buf, len + 1, ESP32_I2C_TIMEOUT_MS);

    if (buf != stack_buf) {
        free(buf);
    }
    return map_err(err, dev_addr, reg_addr);
}

void esp32_delay_ms(uint32_t ms)
{
    if (ms == 0) {
        return;
    }
    TickType_t ticks = pdMS_TO_TICKS(ms);
    vTaskDelay(ticks > 0 ? ticks : 1); /* never round a non-zero delay down to 0 ticks */
}
