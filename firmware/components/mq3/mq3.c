#include "mq3.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "mq3";

esp_err_t mq3_init(mq3_sensor_t *sensor, const mq3_config_t *config)
{
    if (sensor == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(sensor, 0, sizeof(mq3_sensor_t));
    sensor->config = *config;
    sensor->state = MQ3_STATE_READY;
    sensor->initialized = true;

    ESP_LOGI(TAG, "MQ-3 sensor initialized (ADC channel: %d)", config->adc_channel);
    return ESP_OK;
}

esp_err_t mq3_deinit(mq3_sensor_t *sensor)
{
    if (sensor == NULL || !sensor->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    sensor->state = MQ3_STATE_UNINITIALIZED;
    sensor->initialized = false;

    ESP_LOGI(TAG, "MQ-3 sensor deinitialized");
    return ESP_OK;
}

esp_err_t mq3_calibrate(mq3_sensor_t *sensor, uint16_t samples)
{
    if (sensor == NULL || !sensor->initialized || samples == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Calibrating MQ-3 sensor (%u samples)...", samples);
    return ESP_OK;
}

esp_err_t mq3_get_data(mq3_sensor_t *sensor, mq3_data_t *out_data)
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

esp_err_t mq3_set_heater(mq3_sensor_t *sensor, bool enable)
{
    if (sensor == NULL || !sensor->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "MQ-3 heater: %s", enable ? "ENABLED" : "DISABLED");
    return ESP_OK;
}

bool mq3_is_ready(const mq3_sensor_t *sensor)
{
    if (sensor == NULL) {
        return false;
    }
    return sensor->initialized && (sensor->state == MQ3_STATE_READY);
}
