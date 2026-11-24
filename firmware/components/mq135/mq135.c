/*
 * Platform-independent MQ-135 driver.
 * All ADC access is delegated through sensor->adc (Dependency Inversion).
 */
#include "mq135.h"

#include <math.h>

/* MQ-135 CO2 sensitivity curve parameters: ppm = A * (Rs/R0)^B */
#define MQ135_CO2_PARA 116.6020682f
#define MQ135_CO2_PARB (-2.769034857f)

static float mq135_correction_factor(float t_c, float rh)
{
    /* Standard empirical T/RH correction polynomial for MQ-135 (reference: 20 °C, 33 %RH) */
    float cf = 0.00035f * t_c * t_c - 0.02718f * t_c + 1.39538f - (0.0018f * t_c - 0.00333f) * (rh - 33.0f);
    return (cf > 0.1f) ? cf : 0.1f;
}

static uint16_t mq135_ppm_to_aqi(float ppm)
{
    if (ppm <= 400.0f)  { return 25; }
    if (ppm <= 1000.0f) { return (uint16_t)(25.0f + (ppm - 400.0f) * (75.0f / 600.0f)); }
    if (ppm <= 2000.0f) { return (uint16_t)(100.0f + (ppm - 1000.0f) * (100.0f / 1000.0f)); }
    if (ppm <= 5000.0f) { return (uint16_t)(200.0f + (ppm - 2000.0f) * (100.0f / 3000.0f)); }
    return 400;
}

static mq135_err_t mq135_measure_rs(const mq135_sensor_t *sensor, int *out_raw, float *out_mv, float *out_rs)
{
    int mv = 0;
    if (sensor->adc.read_mv(sensor->adc.handle, &mv) != 0) {
        return MQ135_ERR_ADC;
    }

    int raw = 0;
    if (sensor->adc.read_raw != NULL) {
        if (sensor->adc.read_raw(sensor->adc.handle, &raw) != 0) {
            return MQ135_ERR_ADC;
        }
    }

    float v_mv = (float)mv;
    if (v_mv <= 0.0f || v_mv >= sensor->config.vcc_mv) {
        return MQ135_ERR_OUT_OF_RANGE;
    }

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
    return MQ135_OK;
}

mq135_err_t mq135_init(mq135_sensor_t *sensor, const mq135_adc_t *adc, const mq135_config_t *config)
{
    if (sensor == NULL || adc == NULL || config == NULL || adc->read_mv == NULL) {
        return MQ135_ERR_NULL;
    }
    if (config->rl_kohm <= 0.0f || config->r0_kohm < 0.0f) {
        return MQ135_ERR_INVALID_ARG;
    }

    *sensor = (mq135_sensor_t){ 0 };
    sensor->adc = *adc;
    sensor->config = *config;

    if (sensor->config.vcc_mv <= 0.0f) {
        sensor->config.vcc_mv = MQ135_DEFAULT_VCC_MV;
    }
    if (sensor->config.clean_air_ratio <= 0.0f) {
        sensor->config.clean_air_ratio = MQ135_DEFAULT_CLEAN_AIR_RATIO;
    }

    sensor->state = MQ135_STATE_READY;
    sensor->initialized = true;
    return MQ135_OK;
}

mq135_err_t mq135_deinit(mq135_sensor_t *sensor)
{
    if (sensor == NULL) {
        return MQ135_ERR_NULL;
    }
    if (!sensor->initialized) {
        return MQ135_ERR_INVALID_STATE;
    }

    sensor->state = MQ135_STATE_UNINITIALIZED;
    sensor->initialized = false;
    return MQ135_OK;
}

mq135_err_t mq135_calibrate(mq135_sensor_t *sensor, uint16_t samples)
{
    if (sensor == NULL) {
        return MQ135_ERR_NULL;
    }
    if (!sensor->initialized) {
        return MQ135_ERR_INVALID_STATE;
    }
    if (samples == 0) {
        return MQ135_ERR_INVALID_ARG;
    }

    float rs_sum = 0.0f;
    for (uint16_t i = 0; i < samples; i++) {
        float rs = 0.0f;
        mq135_err_t err = mq135_measure_rs(sensor, NULL, NULL, &rs);
        if (err != MQ135_OK) {
            sensor->state = MQ135_STATE_ERROR;
            return err;
        }
        rs_sum += rs;
    }

    float rs_avg = rs_sum / (float)samples;
    sensor->config.r0_kohm = rs_avg / sensor->config.clean_air_ratio;
    sensor->state = MQ135_STATE_READY;
    return MQ135_OK;
}

mq135_err_t mq135_get_data(mq135_sensor_t *sensor, mq135_data_t *out_data)
{
    return mq135_get_compensated_data(sensor, 20.0f, 33.0f, out_data);
}

mq135_err_t mq135_get_compensated_data(mq135_sensor_t *sensor,
                                       float temperature_c,
                                       float humidity_rh,
                                       mq135_data_t *out_data)
{
    if (sensor == NULL || out_data == NULL) {
        return MQ135_ERR_NULL;
    }
    if (!sensor->initialized || sensor->state != MQ135_STATE_READY) {
        return MQ135_ERR_INVALID_STATE;
    }

    int raw = 0;
    float v_mv = 0.0f;
    float rs = 0.0f;
    mq135_err_t err = mq135_measure_rs(sensor, &raw, &v_mv, &rs);
    if (err != MQ135_OK) {
        return err;
    }

    float corr_rs = rs / mq135_correction_factor(temperature_c, humidity_rh);

    mq135_data_t data = {
        .raw_adc = raw,
        .voltage_mv = v_mv,
        .rs_kohm = rs,
        .corrected_rs_kohm = corr_rs,
        .ratio_rs_r0 = 0.0f,
        .ppm_co2 = 0.0f,
        .aqi = 0,
    };

    if (sensor->config.r0_kohm > 0.0f) {
        data.ratio_rs_r0 = corr_rs / sensor->config.r0_kohm;
        if (data.ratio_rs_r0 > 0.0f) {
            data.ppm_co2 = MQ135_CO2_PARA * powf(data.ratio_rs_r0, MQ135_CO2_PARB);
            data.aqi = mq135_ppm_to_aqi(data.ppm_co2);
        }
    }

    sensor->last_data = data;
    *out_data = data;
    return MQ135_OK;
}

bool mq135_is_ready(const mq135_sensor_t *sensor)
{
    return sensor != NULL && sensor->initialized && (sensor->state == MQ135_STATE_READY);
}
