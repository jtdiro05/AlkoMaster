#pragma once

/*
 * Platform-independent MQ-3 alcohol gas sensor driver (Dependency Inversion).
 *
 * The driver depends ONLY on the abstract ADC interface mq3_adc_t defined
 * below and includes only standard C headers. Any ADC implementation whose
 * functions match the mq3_adc_*_fn signatures can be plugged in, e.g. the
 * generic ESP-IDF ADC adapter (esp32_adc_adapter.h):
 *
 *   mq3_adc_t field | ESP-IDF adapter symbol | handle type behind void*
 *   ----------------+------------------------+-------------------------
 *   handle          | &adc_ctx               | esp32_adc_ctx_t *
 *   read_raw        | esp32_adc_read_raw     |
 *   read_mv         | esp32_adc_read_mv      |
 *
 * The adapter has no knowledge of this header - binding happens in the
 * application code (composition root, e.g. main.c).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* Default sensor constants                                                   */
/* -------------------------------------------------------------------------- */

#define MQ3_DEFAULT_VCC_MV          3300.0f /**< Default divider supply voltage [mV] */
#define MQ3_DEFAULT_CLEAN_AIR_RATIO 4.5f    /**< Rs/R0 ratio in clean air for Waveshare module (RL=4.7k, 3.3V) */

/* -------------------------------------------------------------------------- */
/* Abstract ADC interface (the "port" the driver depends on)                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief Read raw ADC count from the channel connected to the sensor
 * @param handle       Opaque ADC adapter handle
 * @param[out] out_raw Raw ADC count
 * @return 0 on success, negative value on error
 */
typedef int (*mq3_adc_read_raw_fn)(void *handle, int *out_raw);

/**
 * @brief Read calibrated voltage in millivolts [mV] from the ADC channel
 * @param handle      Opaque ADC adapter handle
 * @param[out] out_mv Measured voltage [mV]
 * @return 0 on success, negative value on error
 */
typedef int (*mq3_adc_read_mv_fn)(void *handle, int *out_mv);

/**
 * @brief ADC interface injected into the MQ-3 driver
 */
typedef struct {
    void *handle;                 /**< Opaque ADC handle passed unchanged to callbacks */
    mq3_adc_read_raw_fn read_raw; /**< Optional: raw ADC read (may be NULL) */
    mq3_adc_read_mv_fn  read_mv;  /**< Required: calibrated voltage read in [mV] */
} mq3_adc_t;

/* -------------------------------------------------------------------------- */
/* Driver types                                                               */
/* -------------------------------------------------------------------------- */

/** Platform-independent result codes of the MQ-3 driver */
typedef enum {
    MQ3_OK                =  0,
    MQ3_ERR_NULL          = -1, /**< NULL pointer argument or missing read_mv callback */
    MQ3_ERR_INVALID_ARG   = -2, /**< Invalid configuration parameter or samples == 0 */
    MQ3_ERR_INVALID_STATE = -3, /**< Driver not initialized or R0 not calibrated */
    MQ3_ERR_ADC           = -4, /**< ADC adapter returned a read error */
    MQ3_ERR_OUT_OF_RANGE  = -5  /**< Measured voltage out of divider range (<= 0 or >= vcc_mv) */
} mq3_err_t;

/** Operating state of the MQ-3 sensor */
typedef enum {
    MQ3_STATE_UNINITIALIZED = 0, /**< Sensor is not initialized */
    MQ3_STATE_PREHEATING,        /**< Sensor heater is preheating */
    MQ3_STATE_READY,             /**< Sensor is ready for measurements */
    MQ3_STATE_ERROR              /**< Sensor error state */
} mq3_state_t;

/** Configuration structure for the MQ-3 sensor (platform-agnostic) */
typedef struct {
    float vcc_mv;             /**< Circuit supply voltage Vcc [mV] (0 -> MQ3_DEFAULT_VCC_MV) */
    float rl_kohm;            /**< Load resistance RL [kOhm] (must be > 0) */
    float r0_kohm;            /**< Base resistance R0 in clean air [kOhm] (can be set by mq3_calibrate) */
    float clean_air_ratio;    /**< Rs/R0 ratio in clean air (0 -> MQ3_DEFAULT_CLEAN_AIR_RATIO) */
    uint32_t preheat_time_ms; /**< Nominal heater preheat time [ms] (informational) */
} mq3_config_t;

/** Measurement data structure for the MQ-3 sensor */
typedef struct {
    int raw_adc;        /**< Raw ADC reading (0 if read_raw callback is not provided) */
    float voltage_mv;   /**< Sensor output voltage [mV] */
    float rs_kohm;      /**< Current sensor resistance Rs [kOhm] */
    float ratio_rs_r0;  /**< Resistance ratio Rs/R0 */
    float alcohol_mg_l; /**< Estimated breath alcohol concentration [mg/L] */
    float promille;     /**< Estimated blood alcohol content [‰] */
} mq3_data_t;

/** Main descriptor structure representing an MQ-3 sensor instance */
typedef struct {
    mq3_adc_t adc;          /**< Injected ADC interface */
    mq3_config_t config;    /**< Sensor circuit and calibration configuration */
    mq3_state_t state;      /**< Current operating state */
    mq3_data_t last_data;   /**< Most recent measurement data */
    bool initialized;       /**< Initialization status flag */
} mq3_sensor_t;

typedef mq3_sensor_t mq3_dev_t;

/* -------------------------------------------------------------------------- */
/* API                                                                        */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initialize the MQ-3 sensor descriptor and bind the abstract ADC interface
 *
 * @param[out] sensor Pointer to the MQ-3 sensor descriptor
 * @param[in]  adc    Pointer to the ADC interface (copied; read_mv must be non-NULL)
 * @param[in]  config Pointer to the configuration structure (rl_kohm must be > 0)
 * @return mq3_err_t MQ3_OK on success
 */
mq3_err_t mq3_init(mq3_sensor_t *sensor, const mq3_adc_t *adc, const mq3_config_t *config);

/**
 * @brief Deinitialize the MQ-3 sensor descriptor (does not release the ADC adapter)
 *
 * @param[in,out] sensor Pointer to the MQ-3 sensor descriptor
 * @return mq3_err_t MQ3_OK on success
 */
mq3_err_t mq3_deinit(mq3_sensor_t *sensor);

/**
 * @brief Calibrate the MQ-3 sensor in clean air (measures Rs over @p samples and updates r0_kohm)
 *
 * @param[in,out] sensor  Pointer to the MQ-3 sensor descriptor
 * @param[in]     samples Number of ADC samples to average (must be > 0)
 * @return mq3_err_t MQ3_OK on success
 */
mq3_err_t mq3_calibrate(mq3_sensor_t *sensor, uint16_t samples);

/**
 * @brief Retrieve processed measurement data from the MQ-3 sensor
 *        (requests ADC reading via mq3_adc_t, computes Rs, Rs/R0, mg/L and promille)
 *
 * @param[in,out] sensor   Pointer to the MQ-3 sensor descriptor
 * @param[out]    out_data Pointer to the structure where measurement results will be stored
 * @return mq3_err_t MQ3_OK on success
 */
mq3_err_t mq3_get_data(mq3_sensor_t *sensor, mq3_data_t *out_data);

/**
 * @brief Check whether the MQ-3 sensor is initialized and ready to take measurements
 *
 * @param[in] sensor Pointer to the MQ-3 sensor descriptor
 * @return true if ready, false otherwise
 */
bool mq3_is_ready(const mq3_sensor_t *sensor);

#ifdef __cplusplus
}
#endif
