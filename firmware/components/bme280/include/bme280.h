#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Default I2C addresses for the BME280 sensor */
#define BME280_I2C_ADDR_PRIMARY   0x76
#define BME280_I2C_ADDR_SECONDARY 0x77

/**
 * @brief Operating mode of the BME280 sensor
 */
typedef enum {
    BME280_MODE_SLEEP  = 0x00, /**< Sleep mode (no measurements performed) */
    BME280_MODE_FORCED = 0x01, /**< Forced mode (single on-demand measurement) */
    BME280_MODE_NORMAL = 0x03  /**< Normal mode (continuous periodic measurements) */
} bme280_mode_t;

/**
 * @brief Oversampling setting for BME280 measurements
 */
typedef enum {
    BME280_OVERSAMPLING_SKIP = 0x00,
    BME280_OVERSAMPLING_X1   = 0x01,
    BME280_OVERSAMPLING_X2   = 0x02,
    BME280_OVERSAMPLING_X4   = 0x03,
    BME280_OVERSAMPLING_X8   = 0x04,
    BME280_OVERSAMPLING_X16  = 0x05
} bme280_oversampling_t;

/**
 * @brief IIR filter coefficient for the BME280 sensor
 */
typedef enum {
    BME280_FILTER_OFF = 0x00,
    BME280_FILTER_2   = 0x01,
    BME280_FILTER_4   = 0x02,
    BME280_FILTER_8   = 0x03,
    BME280_FILTER_16  = 0x04
} bme280_filter_t;

/**
 * @brief Factory calibration coefficients stored in the BME280 NVM
 */
typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;
    uint8_t  dig_H1;
    int16_t  dig_H2;
    uint8_t  dig_H3;
    int16_t  dig_H4;
    int16_t  dig_H5;
    int8_t   dig_H6;
    int32_t  t_fine; /**< Intermediate fine temperature value used for pressure and humidity compensation */
} bme280_calib_data_t;

/**
 * @brief Configuration structure for the BME280 sensor
 */
typedef struct {
    i2c_port_t i2c_port;             /**< I2C port number (e.g., I2C_NUM_0) */
    uint8_t i2c_addr;                /**< I2C device address (0x76 or 0x77) */
    gpio_num_t sda_pin;              /**< GPIO pin for the SDA data line */
    gpio_num_t scl_pin;              /**< GPIO pin for the SCL clock line */
    uint32_t i2c_freq_hz;            /**< I2C bus clock frequency [Hz] */
    bme280_mode_t mode;              /**< Sensor operating mode */
    bme280_oversampling_t osr_temp;  /**< Temperature oversampling */
    bme280_oversampling_t osr_press; /**< Pressure oversampling */
    bme280_oversampling_t osr_hum;   /**< Humidity oversampling */
    bme280_filter_t filter;          /**< Low-pass IIR filter setting */
} bme280_config_t;

/**
 * @brief Measurement data structure for the BME280 sensor
 */
typedef struct {
    float temperature_c; /**< Ambient temperature [°C] */
    float humidity_rh;   /**< Relative humidity [% RH] */
    float pressure_hpa;  /**< Atmospheric pressure [hPa] */
    float dew_point_c;   /**< Calculated dew point [°C] */
} bme280_data_t;

/**
 * @brief Main descriptor structure representing a BME280 sensor instance
 */
typedef struct {
    bme280_config_t config;    /**< I2C bus and measurement configuration */
    bme280_calib_data_t calib; /**< Device-specific factory calibration coefficients */
    bme280_data_t last_data;   /**< Most recent measurement data */
    uint8_t chip_id;           /**< Chip identifier read from sensor (0x60 for BME280) */
    bool initialized;          /**< Initialization status flag */
} bme280_sensor_t;

/**
 * @brief Initialize the BME280 sensor (facade: configures I2C, verifies chip ID,
 *        reads factory calibration data, and writes measurement registers)
 *
 * @param[in,out] sensor Pointer to the BME280 sensor descriptor
 * @param[in] config Pointer to the configuration structure
 * @return esp_err_t ESP_OK on success
 */
esp_err_t bme280_init(bme280_sensor_t *sensor, const bme280_config_t *config);

/**
 * @brief Deinitialize the BME280 sensor and release bus resources
 *
 * @param[in,out] sensor Pointer to the BME280 sensor descriptor
 * @return esp_err_t ESP_OK on success
 */
esp_err_t bme280_deinit(bme280_sensor_t *sensor);

/**
 * @brief Retrieve compensated environmental data from the BME280 sensor
 *        (encapsulates raw I2C register read, FORCED mode trigger, and Bosch compensation formulas)
 *
 * @param[in,out] sensor Pointer to the BME280 sensor descriptor
 * @param[out] out_data Pointer to the structure where measurement results will be stored
 * @return esp_err_t ESP_OK on success
 */
esp_err_t bme280_get_data(bme280_sensor_t *sensor, bme280_data_t *out_data);

/**
 * @brief Set the operating mode of the BME280 sensor (SLEEP, FORCED, or NORMAL)
 *
 * @param[in,out] sensor Pointer to the BME280 sensor descriptor
 * @param[in] mode Desired operating mode
 * @return esp_err_t ESP_OK on success
 */
esp_err_t bme280_set_mode(bme280_sensor_t *sensor, bme280_mode_t mode);

/**
 * @brief Perform a soft reset of the BME280 sensor
 *
 * @param[in,out] sensor Pointer to the BME280 sensor descriptor
 * @return esp_err_t ESP_OK on success
 */
esp_err_t bme280_reset(bme280_sensor_t *sensor);

/**
 * @brief Check whether the BME280 sensor is initialized and ready for reading
 *
 * @param[in] sensor Pointer to the BME280 sensor descriptor
 * @return true if the sensor is ready, false otherwise
 */
bool bme280_is_ready(const bme280_sensor_t *sensor);

#ifdef __cplusplus
}
#endif
