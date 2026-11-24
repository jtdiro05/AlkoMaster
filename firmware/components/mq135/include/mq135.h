#pragma once

/*
 * Platform-independent MQ-135 air quality gas sensor driver (Dependency Inversion).
 *
 * The driver depends ONLY on the abstract ADC interface mq135_adc_t defined
 * below and includes only standard C headers. Any ADC implementation whose
 * functions match the mq135_adc_*_fn signatures can be plugged in, e.g. the
 * generic ESP-IDF ADC adapter (esp32_adc_adapter.h in esp32_adapters):
 *
 *   mq135_adc_t field | ESP-IDF adapter symbol | handle type behind void*
 *   ------------------+------------------------+-------------------------
 *   handle            | &adc_ctx               | esp32_adc_ctx_t *
 *   read_raw          | esp32_adc_read_raw     |
 *   read_mv           | esp32_adc_read_mv      |
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MQ135_DEFAULT_VCC_MV          3300.0f /**< Default divider supply voltage [mV] */
#define MQ135_DEFAULT_CLEAN_AIR_RATIO 3.6f    /**< Rs/R0 ratio in clean air (~400 ppm CO2, MQ-135 datasheet) */

/**
 * @brief Read raw ADC count from the channel connected to the sensor
 * @return 0 on success, negative value on error
 */
typedef int (*mq135_adc_read_raw_fn)(void *handle, int *out_raw);

/**
 * @brief Read calibrated voltage in millivolts [mV] from the ADC channel
 * @return 0 on success, negative value on error
 */
typedef int (*mq135_adc_read_mv_fn)(void *handle, int *out_mv);

/**
 * @brief ADC interface injected into the MQ-135 driver
 */
typedef struct {
    void *handle;                   /**< Opaque ADC handle passed unchanged to callbacks */
    mq135_adc_read_raw_fn read_raw; /**< Optional: raw ADC read (may be NULL) */
    mq135_adc_read_mv_fn  read_mv;  /**< Required: calibrated voltage read in [mV] */
} mq135_adc_t;

/** Platform-independent result codes of the MQ-135 driver */
typedef enum {
    MQ135_OK                =  0,
    MQ135_ERR_NULL          = -1, /**< NULL pointer argument or missing read_mv callback */
    MQ135_ERR_INVALID_ARG   = -2, /**< Invalid configuration parameter or samples == 0 */
    MQ135_ERR_INVALID_STATE = -3, /**< Driver not initialized or R0 not calibrated */
    MQ135_ERR_ADC           = -4, /**< ADC adapter returned a read error */
    MQ135_ERR_OUT_OF_RANGE  = -5  /**< Measured voltage out of divider range (<= 0 or >= vcc_mv) */
} mq135_err_t;

/** Operating state of the MQ-135 sensor */
typedef enum {
    MQ135_STATE_UNINITIALIZED = 0, /**< Sensor is not initialized */
    MQ135_STATE_PREHEATING,        /**< Sensor heater is preheating */
    MQ135_STATE_READY,             /**< Sensor is ready for measurements */
    MQ135_STATE_ERROR              /**< Sensor error state */
} mq135_state_t;

/** Configuration structure for the MQ-135 sensor (platform-agnostic) */
typedef struct {
    float vcc_mv;             /**< Circuit supply voltage Vcc [mV] (0 -> MQ135_DEFAULT_VCC_MV) */
    float rl_kohm;            /**< Load resistance RL [kOhm] (must be > 0) */
    float r0_kohm;            /**< Base resistance R0 in clean air [kOhm] (can be set by mq135_calibrate) */
    float clean_air_ratio;    /**< Rs/R0 ratio in clean air (0 -> MQ135_DEFAULT_CLEAN_AIR_RATIO) */
    uint32_t preheat_time_ms; /**< Nominal heater preheat time [ms] (informational) */
} mq135_config_t;

/** Measurement data structure for the MQ-135 sensor */
typedef struct {
    int raw_adc;             /**< Raw ADC reading (0 if read_raw callback is not provided) */
    float voltage_mv;        /**< Sensor output voltage [mV] */
    float rs_kohm;           /**< Current sensor resistance Rs [kOhm] */
    float corrected_rs_kohm; /**< Temperature and humidity compensated resistance Rs [kOhm] */
    float ratio_rs_r0;       /**< Resistance ratio Rs_corr/R0 */
    float ppm_co2;           /**< Estimated equivalent CO2 / air pollutant concentration [ppm] */
    uint16_t aqi;            /**< Estimated Air Quality Index (AQI) */
} mq135_data_t;

/** Main descriptor structure representing an MQ-135 sensor instance */
typedef struct {
    mq135_adc_t adc;        /**< Injected ADC interface */
    mq135_config_t config;  /**< Sensor circuit and calibration configuration */
    mq135_state_t state;    /**< Current operating state */
    mq135_data_t last_data; /**< Most recent measurement data */
    bool initialized;       /**< Initialization status flag */
} mq135_sensor_t;

typedef mq135_sensor_t mq135_dev_t;

/**
 * @brief Initialize the MQ-135 sensor descriptor and bind the abstract ADC interface
 */
mq135_err_t mq135_init(mq135_sensor_t *sensor, const mq135_adc_t *adc, const mq135_config_t *config);

/**
 * @brief Deinitialize the MQ-135 sensor descriptor (does not release the ADC adapter)
 */
mq135_err_t mq135_deinit(mq135_sensor_t *sensor);

/**
 * @brief Calibrate the MQ-135 sensor in clean air (measures Rs over @p samples and updates r0_kohm)
 */
mq135_err_t mq135_calibrate(mq135_sensor_t *sensor, uint16_t samples);

/**
 * @brief Retrieve processed measurement data using reference conditions (20 °C, 33 %RH)
 */
mq135_err_t mq135_get_data(mq135_sensor_t *sensor, mq135_data_t *out_data);

/**
 * @brief Retrieve measurement data with temperature and humidity compensation (e.g. from BME280)
 */
mq135_err_t mq135_get_compensated_data(mq135_sensor_t *sensor,
                                       float temperature_c,
                                       float humidity_rh,
                                       mq135_data_t *out_data);

/**
 * @brief Check whether the MQ-135 sensor is initialized and ready to take measurements
 */
bool mq135_is_ready(const mq135_sensor_t *sensor);

#ifdef __cplusplus
}
#endif
