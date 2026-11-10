#include "bme280.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "bme280";

esp_err_t bme280_init(bme280_sensor_t *sensor, const bme280_config_t *config)
{
    if (sensor == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(sensor, 0, sizeof(bme280_sensor_t));
    sensor->config = *config;
    sensor->chip_id = 0x60;
    sensor->initialized = true;

    ESP_LOGI(TAG, "BME280 sensor initialized (I2C addr: 0x%02X)", config->i2c_addr);
    return ESP_OK;
}

esp_err_t bme280_deinit(bme280_sensor_t *sensor)
{
    if (sensor == NULL || !sensor->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    sensor->initialized = false;
    ESP_LOGI(TAG, "BME280 sensor deinitialized");
    return ESP_OK;
}

esp_err_t bme280_get_data(bme280_sensor_t *sensor, bme280_data_t *out_data)
{
    if (sensor == NULL || out_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!sensor->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    *out_data = sensor->last_data;
    return ESP_OK;
}

esp_err_t bme280_set_mode(bme280_sensor_t *sensor, bme280_mode_t mode)
{
    if (sensor == NULL || !sensor->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    sensor->config.mode = mode;
    ESP_LOGI(TAG, "BME280 operating mode set to: 0x%02X", mode);
    return ESP_OK;
}

esp_err_t bme280_reset(bme280_sensor_t *sensor)
{
    if (sensor == NULL || !sensor->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Resetting BME280 sensor...");
    return ESP_OK;
}

bool bme280_is_ready(const bme280_sensor_t *sensor)
{
    if (sensor == NULL) {
        return false;
    }
    return sensor->initialized;
}
