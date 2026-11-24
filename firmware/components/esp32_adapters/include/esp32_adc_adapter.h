#pragma once

/*
 * Generic ESP-IDF ADC oneshot adapter.
 *
 * Knows NOTHING about any particular sensor (does NOT include mq3.h or mq135.h).
 * Provides channel configuration, optional eFuse calibration, and generic
 * read callbacks matching sensor driver contracts:
 *
 *   mq3_adc_t adc = {
 *       .handle   = &adc_ctx,           // esp32_adc_ctx_t * -> void *
 *       .read_raw = esp32_adc_read_raw, // matches mq3_adc_read_raw_fn
 *       .read_mv  = esp32_adc_read_mv,  // matches mq3_adc_read_mv_fn
 *   };
 *
 * Multiple sensor channels on the same ADC unit (e.g. MQ-3 and MQ-135 on
 * ADC_UNIT_1) can share a single adc_oneshot_unit_handle_t by passing it in
 * esp32_adc_config_t::unit_handle.
 */

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP32_ADC_OK              0
#define ESP32_ADC_ERR_INVALID_ARG (-1) /**< NULL handle/output pointer or uninitialized context */
#define ESP32_ADC_ERR_READ        (-2) /**< adc_oneshot_read() failed */
#define ESP32_ADC_ERR_CALI        (-3) /**< Voltage conversion failed */

/**
 * @brief Configuration for an ESP32 ADC channel adapter instance
 */
typedef struct {
    adc_oneshot_unit_handle_t unit_handle; /**< Optional: existing unit handle (NULL -> adapter creates one) */
    adc_unit_t unit_id;                    /**< ADC unit (e.g. ADC_UNIT_1) */
    adc_channel_t channel;                 /**< ADC channel (e.g. ADC_CHANNEL_0) */
    adc_atten_t atten;                     /**< Attenuation (e.g. ADC_ATTEN_DB_12 for up to ~3.1-3.3 V) */
    adc_bitwidth_t bitwidth;               /**< Bitwidth (ADC_BITWIDTH_DEFAULT for 12-bit) */
} esp32_adc_config_t;

/**
 * @brief Runtime context of the ESP32 ADC adapter (passed as `void *handle` to callbacks)
 */
typedef struct {
    adc_oneshot_unit_handle_t unit_handle; /**< Active ADC oneshot unit handle */
    adc_cali_handle_t cali_handle;         /**< Hardware calibration handle (NULL if using linear fallback) */
    adc_unit_t unit_id;                    /**< ADC unit ID */
    adc_channel_t channel;                 /**< Configured ADC channel */
    adc_atten_t atten;                     /**< Configured attenuation */
    adc_bitwidth_t bitwidth;               /**< Configured bitwidth */
    bool unit_owned;                       /**< True if unit_handle was allocated by esp32_adc_init() */
    bool cali_owned;                       /**< True if cali_handle was allocated by esp32_adc_init() */
    bool initialized;                      /**< Adapter initialization flag */
} esp32_adc_ctx_t;

/**
 * @brief Initialize the ESP32 ADC unit (if not shared), channel, and eFuse calibration
 *
 * @param[out] ctx    Pointer to the persistent adapter context
 * @param[in]  config Pointer to the ADC channel configuration
 * @return esp_err_t ESP_OK on success
 */
esp_err_t esp32_adc_init(esp32_adc_ctx_t *ctx, const esp32_adc_config_t *config);

/**
 * @brief Release calibration resources (and the ADC unit if owned by this context)
 *
 * @param[in,out] ctx Pointer to the adapter context
 * @return esp_err_t ESP_OK on success
 */
esp_err_t esp32_adc_deinit(esp32_adc_ctx_t *ctx);

/**
 * @brief Read a raw ADC sample from the configured channel
 *
 * @param handle       esp32_adc_ctx_t * cast to void *
 * @param[out] out_raw Raw ADC reading
 * @return 0 (ESP32_ADC_OK) on success, negative error code on failure
 */
int esp32_adc_read_raw(void *handle, int *out_raw);

/**
 * @brief Read a calibrated voltage in millivolts [mV] from the configured channel
 *
 * @param handle      esp32_adc_ctx_t * cast to void *
 * @param[out] out_mv Voltage in millivolts [mV]
 * @return 0 (ESP32_ADC_OK) on success, negative error code on failure
 */
int esp32_adc_read_mv(void *handle, int *out_mv);

#ifdef __cplusplus
}
#endif
