/*
 * Platform-independent MQ-3 driver.
 * All ADC access is delegated through sensor->adc (Dependency Inversion).
 */
#include "mq3.h"

#include <math.h>

/* MQ-3 alcohol sensitivity curve parameters (datasheet log-log fit: mg/L = A * (Rs/R0)^B) */
#define MQ3_CURVE_A          0.40f
#define MQ3_CURVE_B         (-1.43f)
#define MQ3_MG_L_TO_PROMILLE 2.10f

static mq3_err_t mq3_measure_rs(const mq3_sensor_t *sensor, int *out_raw, float *out_mv, float *out_rs)
{
    int mv = 0;
    if (sensor->adc.read_mv(sensor->adc.handle, &mv) != 0) {
        return MQ3_ERR_ADC;
    }

    int raw = 0;
    if (sensor->adc.read_raw != NULL) {
        if (sensor->adc.read_raw(sensor->adc.handle, &raw) != 0) {
            return MQ3_ERR_ADC;
        }
    }

    float v_mv = (float)mv;
    if (v_mv <= 0.0f || v_mv >= sensor->config.vcc_mv) {
        return MQ3_ERR_OUT_OF_RANGE;
    }

    /* Voltage divider: Rs = RL * (Vcc - Vout) / Vout */
    float rs = sensor->config.rl_kohm * ((sensor->config.vcc_mv - v_mv) / v_mv);

    if (out_raw != NULL) {
        *out_raw = raw;
    }
    if (out_mv != NULL) {
        *out_mv = v_mv;
    }
    if (out_rs != NULL) {
        *out_rs = rs;
    }
    return MQ3_OK;
}

mq3_err_t mq3_init(mq3_sensor_t *sensor, const mq3_adc_t *adc, const mq3_config_t *config)
{
    if (sensor == NULL || adc == NULL || config == NULL || adc->read_mv == NULL) {
        return MQ3_ERR_NULL;
    }
    if (config->rl_kohm <= 0.0f || config->r0_kohm < 0.0f) {
        return MQ3_ERR_INVALID_ARG;
    }

    *sensor = (mq3_sensor_t){ 0 };
    sensor->adc = *adc;
    sensor->config = *config;

    if (sensor->config.vcc_mv <= 0.0f) {
        sensor->config.vcc_mv = MQ3_DEFAULT_VCC_MV;
    }
    if (sensor->config.clean_air_ratio <= 0.0f) {
        sensor->config.clean_air_ratio = MQ3_DEFAULT_CLEAN_AIR_RATIO;
    }

    sensor->state = MQ3_STATE_READY;
    sensor->initialized = true;
    return MQ3_OK;
}

mq3_err_t mq3_deinit(mq3_sensor_t *sensor)
{
    if (sensor == NULL) {
        return MQ3_ERR_NULL;
    }
    if (!sensor->initialized) {
        return MQ3_ERR_INVALID_STATE;
    }

    sensor->state = MQ3_STATE_UNINITIALIZED;
    sensor->initialized = false;
    return MQ3_OK;
}

mq3_err_t mq3_calibrate(mq3_sensor_t *sensor, uint16_t samples)
{
    if (sensor == NULL) {
        return MQ3_ERR_NULL;
    }
    if (!sensor->initialized) {
        return MQ3_ERR_INVALID_STATE;
    }
    if (samples == 0) {
        return MQ3_ERR_INVALID_ARG;
    }

    float rs_sum = 0.0f;
    for (uint16_t i = 0; i < samples; i++) {
        float rs = 0.0f;
        mq3_err_t err = mq3_measure_rs(sensor, NULL, NULL, &rs);
        if (err != MQ3_OK) {
            sensor->state = MQ3_STATE_ERROR;
            return err;
        }
        rs_sum += rs;
    }

    float rs_avg = rs_sum / (float)samples;
    sensor->config.r0_kohm = rs_avg / sensor->config.clean_air_ratio;
    sensor->state = MQ3_STATE_READY;
    return MQ3_OK;
}

mq3_err_t mq3_get_data(mq3_sensor_t *sensor, mq3_data_t *out_data)
{
    if (sensor == NULL || out_data == NULL) {
        return MQ3_ERR_NULL;
    }
    if (!sensor->initialized || sensor->state != MQ3_STATE_READY) {
        return MQ3_ERR_INVALID_STATE;
    }

    int raw = 0;
    float v_mv = 0.0f;
    float rs = 0.0f;
    mq3_err_t err = mq3_measure_rs(sensor, &raw, &v_mv, &rs);
    if (err != MQ3_OK) {
        return err;
    }

    mq3_data_t data = {
        .raw_adc = raw,
        .voltage_mv = v_mv,
        .rs_kohm = rs,
        .ratio_rs_r0 = 0.0f,
        .alcohol_mg_l = 0.0f,
        .promille = 0.0f,
    };

    if (sensor->config.r0_kohm > 0.0f) {
        data.ratio_rs_r0 = rs / sensor->config.r0_kohm;
        /* Deadzone near clean-air baseline (>80% of clean_air_ratio) so idle reads 0.0 */
        float clean_threshold = sensor->config.clean_air_ratio * 0.80f;
        if (data.ratio_rs_r0 > 0.0f && data.ratio_rs_r0 < clean_threshold) {
            data.alcohol_mg_l = MQ3_CURVE_A * powf(data.ratio_rs_r0, MQ3_CURVE_B);
            data.promille = data.alcohol_mg_l * MQ3_MG_L_TO_PROMILLE;
        }
    }

    sensor->last_data = data;
    *out_data = data;
    return MQ3_OK;
}

bool mq3_is_ready(const mq3_sensor_t *sensor)
{
    return sensor != NULL && sensor->initialized && (sensor->state == MQ3_STATE_READY);
}
