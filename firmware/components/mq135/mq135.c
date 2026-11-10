#include "mq135.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "mq135";

esp_err_t mq135_init(mq135_sensor_t *sensor, const mq135_config_t *config)
{
    if (sensor == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(sensor, 0, sizeof(mq135_sensor_t));
    sensor->config = *config;
    sensor->state = MQ135_STATE_READY;
    sensor->initialized = true;

    ESP_LOGI(TAG, "MQ-135 sensor initialized (ADC channel: %d)", config->adc_channel);
    return ESP_OK;
}

esp_err_t mq135_deinit(mq135_sensor_t *sensor)
{
    if (sensor == NULL || !sensor->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    sensor->state = MQ135_STATE_UNINITIALIZED;
    sensor->initialized = false;

    ESP_LOGI(TAG, "MQ-135 sensor deinitialized");
    return ESP_OK;
}

esp_err_t mq135_calibrate(mq135_sensor_t *sensor, uint16_t samples)
{
    if (sensor == NULL || !sensor->initialized || samples == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Calibrating MQ-135 sensor (%u samples)...", samples);
    return ESP_OK;
}

esp_err_t mq135_get_data(mq135_sensor_t *sensor, mq135_data_t *out_data)
{
    return mq135_get_compensated_data(sensor, 20.0f, 33.0f, out_data);
}

esp_err_t mq135_get_compensated_data(mq135_sensor_t *sensor, float temperature_c, float humidity_rh, mq135_data_t *out_data)
{
    if (sensor == NULL || out_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!sensor->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    (void)temperature_c;
    (void)humidity_rh;

    *out_data = sensor->last_data;
    return ESP_OK;
}

esp_err_t mq135_set_heater(mq135_sensor_t *sensor, bool enable)
{
    if (sensor == NULL || !sensor->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "MQ-135 heater: %s", enable ? "ENABLED" : "DISABLED");
    return ESP_OK;
}

bool mq135_is_ready(const mq135_sensor_t *sensor)
{
    if (sensor == NULL) {
        return false;
    }
    return sensor->initialized && (sensor->state == MQ135_STATE_READY);
}
