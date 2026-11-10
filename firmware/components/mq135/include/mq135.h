#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Operating state of the MQ-135 sensor
 */
typedef enum {
    MQ135_STATE_UNINITIALIZED = 0, /**< Sensor is not initialized */
    MQ135_STATE_PREHEATING,        /**< Sensor heater is preheating */
    MQ135_STATE_READY,             /**< Sensor is ready for measurements */
    MQ135_STATE_ERROR              /**< Sensor error state */
} mq135_state_t;

/**
 * @brief Configuration structure for the MQ-135 sensor
 */
typedef struct {
    adc_unit_t adc_unit;          /**< ADC unit (e.g., ADC_UNIT_1) */
    adc_channel_t adc_channel;    /**< ADC channel connected to the sensor analog output */
    gpio_num_t heater_pin;        /**< GPIO pin controlling the heater (GPIO_NUM_NC if always powered) */
    float rl_kohm;                /**< Load resistance RL [kOhm] */
    float r0_kohm;                /**< Base resistance R0 in clean air [kOhm] */
    uint32_t preheat_time_ms;     /**< Required heater preheat time [ms] */
} mq135_config_t;

/**
 * @brief Measurement data structure for the MQ-135 sensor
 */
typedef struct {
    int raw_adc;                  /**< Raw ADC reading */
    float voltage_mv;             /**< Sensor output voltage [mV] */
    float rs_kohm;                /**< Current sensor resistance Rs [kOhm] */
    float corrected_rs_kohm;      /**< Temperature and humidity compensated resistance Rs [kOhm] */
    float ratio_rs_r0;            /**< Resistance ratio Rs/R0 */
    float ppm_co2;                /**< Estimated equivalent CO2 / air pollutant concentration [ppm] */
    uint16_t aqi;                 /**< Estimated Air Quality Index (AQI) */
} mq135_data_t;

/**
 * @brief Main descriptor structure representing an MQ-135 sensor instance
 */
typedef struct {
    mq135_config_t config;                /**< Hardware and calibration configuration */
    mq135_state_t state;                  /**< Current operating state */
    mq135_data_t last_data;               /**< Most recent measurement data */
    adc_oneshot_unit_handle_t adc_handle; /**< ADC oneshot unit handle */
    bool initialized;                     /**< Initialization status flag */
} mq135_sensor_t;

/**
 * @brief Initialize the MQ-135 sensor (facade: configures ADC, heater GPIO, and initial state)
 *
 * @param[in,out] sensor Pointer to the MQ-135 sensor descriptor
 * @param[in] config Pointer to the configuration structure
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mq135_init(mq135_sensor_t *sensor, const mq135_config_t *config);

/**
 * @brief Deinitialize the MQ-135 sensor and release hardware resources
 *
 * @param[in,out] sensor Pointer to the MQ-135 sensor descriptor
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mq135_deinit(mq135_sensor_t *sensor);

/**
 * @brief Calibrate the MQ-135 sensor in clean air (determines base resistance R0)
 *
 * @param[in,out] sensor Pointer to the MQ-135 sensor descriptor
 * @param[in] samples Number of samples to average during calibration
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mq135_calibrate(mq135_sensor_t *sensor, uint16_t samples);

/**
 * @brief Retrieve processed measurement data from the MQ-135 sensor without environmental compensation
 *
 * @param[in,out] sensor Pointer to the MQ-135 sensor descriptor
 * @param[out] out_data Pointer to the structure where measurement results will be stored
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mq135_get_data(mq135_sensor_t *sensor, mq135_data_t *out_data);

/**
 * @brief Retrieve measurement data from the MQ-135 sensor with temperature and humidity compensation (e.g., from BME280)
 *
 * @param[in,out] sensor Pointer to the MQ-135 sensor descriptor
 * @param[in] temperature_c Ambient temperature [°C]
 * @param[in] humidity_rh Relative humidity [% RH]
 * @param[out] out_data Pointer to the structure where measurement results will be stored
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mq135_get_compensated_data(mq135_sensor_t *sensor, float temperature_c, float humidity_rh, mq135_data_t *out_data);

/**
 * @brief Enable or disable the MQ-135 sensor heater
 *
 * @param[in,out] sensor Pointer to the MQ-135 sensor descriptor
 * @param[in] enable true to enable the heater, false to disable
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mq135_set_heater(mq135_sensor_t *sensor, bool enable);

/**
 * @brief Check whether the MQ-135 sensor is ready to take measurements
 *
 * @param[in] sensor Pointer to the MQ-135 sensor descriptor
 * @return true if the sensor is initialized and preheated, false otherwise
 */
bool mq135_is_ready(const mq135_sensor_t *sensor);

#ifdef __cplusplus
}
#endif
