/*
 * Generic ESP-IDF ADC oneshot adapter - sensor agnostic (does NOT include any sensor header).
 */
#include "esp32_adc_adapter.h"

#include <string.h>
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

static const char *TAG = "esp32_adc";

/* Fallback full-scale voltage [mV] per attenuation when eFuse calibration is unavailable */
static int fallback_fullscale_mv(adc_atten_t atten)
{
    switch (atten) {
    case ADC_ATTEN_DB_0:   return 950;
    case ADC_ATTEN_DB_2_5: return 1250;
    case ADC_ATTEN_DB_6:   return 1750;
    case ADC_ATTEN_DB_12:
    default:               return 3100;
    }
}

static bool create_calibration(adc_unit_t unit, adc_channel_t chan, adc_atten_t atten,
                               adc_bitwidth_t bitwidth, adc_cali_handle_t *out_cali)
{
    *out_cali = NULL;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    const adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = unit,
        .chan = chan,
        .atten = atten,
        .bitwidth = bitwidth,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_cfg, out_cali) == ESP_OK) {
        return true;
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    const adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id = unit,
        .atten = atten,
        .bitwidth = bitwidth,
    };
    if (adc_cali_create_scheme_line_fitting(&cali_cfg, out_cali) == ESP_OK) {
        return true;
    }
#endif

    (void)unit;
    (void)chan;
    (void)atten;
    (void)bitwidth;
    return false;
}

static void delete_calibration(adc_cali_handle_t cali)
{
    if (cali == NULL) {
        return;
    }
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    (void)adc_cali_delete_scheme_curve_fitting(cali);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    (void)adc_cali_delete_scheme_line_fitting(cali);
#endif
}

esp_err_t esp32_adc_init(esp32_adc_ctx_t *ctx, const esp32_adc_config_t *config)
{
    if (ctx == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->unit_id = config->unit_id;
    ctx->channel = config->channel;
    ctx->atten = config->atten;
    ctx->bitwidth = config->bitwidth;

    esp_err_t err;
    if (config->unit_handle != NULL) {
        ctx->unit_handle = config->unit_handle;
        ctx->unit_owned = false;
    } else {
        const adc_oneshot_unit_init_cfg_t unit_cfg = {
            .unit_id = config->unit_id,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        err = adc_oneshot_new_unit(&unit_cfg, &ctx->unit_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "adc_oneshot_new_unit failed: %s", esp_err_to_name(err));
            return err;
        }
        ctx->unit_owned = true;
    }

    const adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = config->atten,
        .bitwidth = config->bitwidth,
    };
    err = adc_oneshot_config_channel(ctx->unit_handle, config->channel, &chan_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_config_channel failed: %s", esp_err_to_name(err));
        if (ctx->unit_owned) {
            adc_oneshot_del_unit(ctx->unit_handle);
            ctx->unit_handle = NULL;
        }
        return err;
    }

    ctx->cali_owned = create_calibration(config->unit_id, config->channel,
                                         config->atten, config->bitwidth,
                                         &ctx->cali_handle);
    ctx->initialized = true;
    return ESP_OK;
}

esp_err_t esp32_adc_deinit(esp32_adc_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (ctx->cali_owned && ctx->cali_handle != NULL) {
        delete_calibration(ctx->cali_handle);
        ctx->cali_handle = NULL;
    }

    esp_err_t err = ESP_OK;
    if (ctx->unit_owned && ctx->unit_handle != NULL) {
        err = adc_oneshot_del_unit(ctx->unit_handle);
    }
    ctx->unit_handle = NULL;
    ctx->initialized = false;
    return err;
}

int esp32_adc_read_raw(void *handle, int *out_raw)
{
    if (handle == NULL || out_raw == NULL) {
        return ESP32_ADC_ERR_INVALID_ARG;
    }

    esp32_adc_ctx_t *ctx = (esp32_adc_ctx_t *)handle;
    if (!ctx->initialized || ctx->unit_handle == NULL) {
        return ESP32_ADC_ERR_INVALID_ARG;
    }

    esp_err_t err = adc_oneshot_read(ctx->unit_handle, ctx->channel, out_raw);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_read(ch=%d) failed: %s", (int)ctx->channel, esp_err_to_name(err));
        return ESP32_ADC_ERR_READ;
    }
    return ESP32_ADC_OK;
}

int esp32_adc_read_mv(void *handle, int *out_mv)
{
    if (handle == NULL || out_mv == NULL) {
        return ESP32_ADC_ERR_INVALID_ARG;
    }

    int raw = 0;
    int rc = esp32_adc_read_raw(handle, &raw);
    if (rc != ESP32_ADC_OK) {
        return rc;
    }

    esp32_adc_ctx_t *ctx = (esp32_adc_ctx_t *)handle;
    if (ctx->cali_handle != NULL) {
        if (adc_cali_raw_to_voltage(ctx->cali_handle, raw, out_mv) != ESP_OK) {
            return ESP32_ADC_ERR_CALI;
        }
        return ESP32_ADC_OK;
    }

    /* Linear fallback (12-bit default) if eFuse calibration is not present */
    int bits = (ctx->bitwidth == ADC_BITWIDTH_DEFAULT) ? 12 : (int)ctx->bitwidth;
    int max_raw = (1 << bits) - 1;
    *out_mv = (raw * fallback_fullscale_mv(ctx->atten)) / (max_raw > 0 ? max_raw : 4095);
    return ESP32_ADC_OK;
}
