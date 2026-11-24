/*
 * Platform-independent BME280 driver.
 * Only standard C headers are allowed here - all I/O goes through dev->bus.
 */
#include "bme280.h"

#define BME280_STARTUP_MS       2u   /* start-up time after power-on/reset */
#define BME280_NVM_POLL_TRIES   10
#define BME280_MEAS_EXTRA_POLLS 10
#define BME280_SKIPPED_TP       0x80000
#define BME280_SKIPPED_H        0x8000

/* ---------------------------------------------------------------- helpers */

static bool bus_is_complete(const bme280_bus_t *bus)
{
    return bus->read != NULL && bus->write != NULL && bus->delay_ms != NULL;
}

static bme280_err_t rd(const bme280_dev_t *dev, uint8_t reg, uint8_t *data, size_t len)
{
    return (dev->bus.read(dev->bus.handle, dev->dev_addr, reg, data, len) == 0) ? BME280_OK : BME280_ERR_BUS;
}

static bme280_err_t wr(const bme280_dev_t *dev, uint8_t reg, uint8_t value)
{
    return (dev->bus.write(dev->bus.handle, dev->dev_addr, reg, &value, 1) == 0) ? BME280_OK : BME280_ERR_BUS;
}

static uint8_t ctrl_meas(const bme280_config_t *c, bme280_mode_t mode)
{
    return (uint8_t)(((c->osr_temp & 7) << 5) | ((c->osr_press & 7) << 2) | (mode & 3));
}

static uint32_t osr_samples(bme280_oversampling_t o)
{
    return (o == BME280_OVERSAMPLING_SKIP) ? 0u : (1u << (o - 1));
}

/* Max measurement time, datasheet section 9.1, rounded up to ms */
static uint32_t max_meas_ms(const bme280_config_t *c)
{
    uint32_t p = osr_samples(c->osr_press), h = osr_samples(c->osr_hum);
    uint32_t us = 1250u + 2300u * osr_samples(c->osr_temp);
    if (p) { us += 2300u * p + 575u; }
    if (h) { us += 2300u * h + 575u; }
    return (us + 999u) / 1000u;
}

static bme280_err_t wait_status_clear(const bme280_dev_t *dev, uint8_t mask, int tries)
{
    for (int i = 0; i <= tries; i++) {
        uint8_t st = 0;
        bme280_err_t err = rd(dev, BME280_REG_STATUS, &st, 1);
        if (err != BME280_OK) { return err; }
        if ((st & mask) == 0) { return BME280_OK; }
        dev->bus.delay_ms(1);
    }
    return BME280_ERR_TIMEOUT;
}

static bme280_err_t soft_reset(const bme280_dev_t *dev)
{
    bme280_err_t err = wr(dev, BME280_REG_RESET, BME280_CMD_RESET);
    if (err != BME280_OK) { return err; }
    dev->bus.delay_ms(BME280_STARTUP_MS);
    return wait_status_clear(dev, BME280_STATUS_IM_UPDATE, BME280_NVM_POLL_TRIES);
}

static bme280_err_t read_calibration(bme280_dev_t *dev)
{
    uint8_t a[26], b[7];
    bme280_err_t err = rd(dev, BME280_REG_CALIB_00, a, sizeof(a));
    if (err == BME280_OK) { err = rd(dev, BME280_REG_CALIB_26, b, sizeof(b)); }
    if (err != BME280_OK) { return err; }

    bme280_calib_data_t *c = &dev->calib;
    c->dig_T1 = (uint16_t)((a[1] << 8) | a[0]);
    c->dig_T2 = (int16_t)((a[3] << 8) | a[2]);
    c->dig_T3 = (int16_t)((a[5] << 8) | a[4]);
    c->dig_P1 = (uint16_t)((a[7] << 8) | a[6]);
    c->dig_P2 = (int16_t)((a[9] << 8) | a[8]);
    c->dig_P3 = (int16_t)((a[11] << 8) | a[10]);
    c->dig_P4 = (int16_t)((a[13] << 8) | a[12]);
    c->dig_P5 = (int16_t)((a[15] << 8) | a[14]);
    c->dig_P6 = (int16_t)((a[17] << 8) | a[16]);
    c->dig_P7 = (int16_t)((a[19] << 8) | a[18]);
    c->dig_P8 = (int16_t)((a[21] << 8) | a[20]);
    c->dig_P9 = (int16_t)((a[23] << 8) | a[22]);
    c->dig_H1 = a[25];                                          /* 0xA1 (0xA0 unused) */
    c->dig_H2 = (int16_t)((b[1] << 8) | b[0]);                  /* 0xE1/0xE2 */
    c->dig_H3 = b[2];                                           /* 0xE3 */
    /* 12-bit signed values: sign-extend the MSB byte, then multiply (avoids left shift of negatives) */
    c->dig_H4 = (int16_t)((int16_t)(int8_t)b[3] * 16 + (b[4] & 0x0F)); /* 0xE4[11:4] 0xE5[3:0] */
    c->dig_H5 = (int16_t)((int16_t)(int8_t)b[5] * 16 + (b[4] >> 4));   /* 0xE6[11:4] 0xE5[7:4] */
    c->dig_H6 = (int8_t)b[6];                                   /* 0xE7 */
    c->t_fine = 0;
    return BME280_OK;
}

/* config is ignored in normal mode -> sleep first; ctrl_hum needs a following ctrl_meas write */
static bme280_err_t apply_config(const bme280_dev_t *dev)
{
    const bme280_config_t *c = &dev->config;
    bme280_mode_t run = (c->mode == BME280_MODE_NORMAL) ? BME280_MODE_NORMAL : BME280_MODE_SLEEP;
    bme280_err_t err = wr(dev, BME280_REG_CTRL_MEAS, ctrl_meas(c, BME280_MODE_SLEEP));
    if (err == BME280_OK) { err = wr(dev, BME280_REG_CTRL_HUM, (uint8_t)(c->osr_hum & 7)); }
    if (err == BME280_OK) {
        err = wr(dev, BME280_REG_CONFIG, (uint8_t)(((c->standby_time & 7) << 5) | ((c->filter & 7) << 2)));
    }
    if (err == BME280_OK) { err = wr(dev, BME280_REG_CTRL_MEAS, ctrl_meas(c, run)); }
    return err;
}

/* ------------------------------------- compensation (datasheet 4.2.3) */

static int32_t comp_t(bme280_calib_data_t *c, int32_t adc)     /* 0.01 °C */
{
    int32_t v1 = (((adc >> 3) - ((int32_t)c->dig_T1 * 2)) * (int32_t)c->dig_T2) >> 11;
    int32_t d = (adc >> 4) - (int32_t)c->dig_T1;
    /* d*d may exceed INT32_MAX for |d| > 46340 -> compute in 64 bits */
    int32_t v2 = (int32_t)(((((int64_t)d * d) >> 12) * c->dig_T3) >> 14);
    c->t_fine = v1 + v2;
    return (c->t_fine * 5 + 128) >> 8;
}

static uint32_t comp_p(const bme280_calib_data_t *c, int32_t adc) /* Q24.8 Pa */
{
    int64_t v1 = (int64_t)c->t_fine - 128000;
    int64_t v2 = v1 * v1 * c->dig_P6;
    v2 += (v1 * c->dig_P5) * 131072;
    v2 += (int64_t)c->dig_P4 * 34359738368LL;
    v1 = ((v1 * v1 * c->dig_P3) >> 8) + ((v1 * c->dig_P2) * 4096);
    v1 = ((((int64_t)1) << 47) + v1) * c->dig_P1 >> 33;
    if (v1 == 0) { return 0; }
    int64_t p = 1048576 - adc;
    p = ((p * 2147483648LL - v2) * 3125) / v1;
    v1 = ((int64_t)c->dig_P9 * (p >> 13) * (p >> 13)) >> 25;
    v2 = ((int64_t)c->dig_P8 * p) >> 19;
    return (uint32_t)(((p + v1 + v2) >> 8) + ((int64_t)c->dig_P7 * 16));
}

static uint32_t comp_h(const bme280_calib_data_t *c, int32_t adc) /* Q22.10 %RH */
{
    /* Same formula as datasheet 4.2.3, evaluated in 64 bits to avoid signed overflow */
    int64_t v = (int64_t)c->t_fine - 76800;
    int64_t x1 = ((((int64_t)adc * 16384) - ((int64_t)c->dig_H4 * 1048576) - ((int64_t)c->dig_H5 * v)) + 16384) >> 15;
    int64_t x2 = ((((((v * c->dig_H6) >> 10) * (((v * c->dig_H3) >> 11) + 32768)) >> 10) + 2097152) *
                  c->dig_H2 + 8192) >> 14;
    v = x1 * x2;
    v -= ((((v >> 15) * (v >> 15)) >> 7) * c->dig_H1) >> 4;
    if (v < 0) { v = 0; }
    if (v > 419430400) { v = 419430400; }
    return (uint32_t)(v >> 12);
}

/* ---------------------------------------------------------------- API */

bme280_err_t bme280_init(bme280_dev_t *dev, const bme280_bus_t *bus, uint8_t dev_addr,
                         const bme280_config_t *config)
{
    if (dev == NULL || bus == NULL || config == NULL || !bus_is_complete(bus)) {
        return BME280_ERR_NULL;
    }
    if (dev_addr != BME280_I2C_ADDR_PRIMARY && dev_addr != BME280_I2C_ADDR_SECONDARY) {
        return BME280_ERR_INVALID_ARG;
    }
    /* Temperature (t_fine) is required to compensate pressure and humidity */
    if (config->osr_temp == BME280_OVERSAMPLING_SKIP) {
        return BME280_ERR_INVALID_ARG;
    }

    *dev = (bme280_dev_t){ 0 };
    dev->bus = *bus;          /* store injected interface */
    dev->dev_addr = dev_addr;
    dev->config = *config;

    dev->bus.delay_ms(BME280_STARTUP_MS);

    /* Communication check: CHIP_ID register 0xD0 must read 0x60 */
    bme280_err_t err = rd(dev, BME280_REG_ID, &dev->chip_id, 1);
    if (err != BME280_OK) { return err; }
    if (dev->chip_id != BME280_CHIP_ID) { return BME280_ERR_CHIP_ID; }

    err = soft_reset(dev);
    if (err == BME280_OK) { err = read_calibration(dev); }
    if (err == BME280_OK) { err = apply_config(dev); }
    if (err != BME280_OK) { return err; }

    dev->initialized = true;
    return BME280_OK;
}

bme280_err_t bme280_deinit(bme280_dev_t *dev)
{
    if (dev == NULL) { return BME280_ERR_NULL; }
    if (!dev->initialized) { return BME280_ERR_INVALID_STATE; }
    (void)wr(dev, BME280_REG_CTRL_MEAS, ctrl_meas(&dev->config, BME280_MODE_SLEEP));
    dev->initialized = false;
    return BME280_OK;
}

bme280_err_t bme280_get_data(bme280_dev_t *dev, bme280_data_t *out)
{
    if (dev == NULL || out == NULL) { return BME280_ERR_NULL; }
    if (!dev->initialized) { return BME280_ERR_INVALID_STATE; }

    bme280_err_t err;
    if (dev->config.mode != BME280_MODE_NORMAL) {
        err = wr(dev, BME280_REG_CTRL_MEAS, ctrl_meas(&dev->config, BME280_MODE_FORCED));
        if (err != BME280_OK) { return err; }
        dev->bus.delay_ms(max_meas_ms(&dev->config));
        err = wait_status_clear(dev, BME280_STATUS_MEASURING, BME280_MEAS_EXTRA_POLLS);
        if (err != BME280_OK) { return err; }
    }

    uint8_t d[8]; /* burst read 0xF7..0xFE for data consistency */
    err = rd(dev, BME280_REG_PRESS_MSB, d, sizeof(d));
    if (err != BME280_OK) { return err; }

    int32_t adc_p = (int32_t)(((uint32_t)d[0] << 12) | ((uint32_t)d[1] << 4) | (d[2] >> 4));
    int32_t adc_t = (int32_t)(((uint32_t)d[3] << 12) | ((uint32_t)d[4] << 4) | (d[5] >> 4));
    int32_t adc_h = (int32_t)(((uint32_t)d[6] << 8) | d[7]);

    if (adc_t == BME280_SKIPPED_TP) { return BME280_ERR_INVALID_STATE; } /* t_fine required */

    *out = (bme280_data_t){ 0 };
    out->temperature_c = (float)comp_t(&dev->calib, adc_t) / 100.0f;
    if (adc_p != BME280_SKIPPED_TP) {
        out->pressure_hpa = (float)comp_p(&dev->calib, adc_p) / 25600.0f;
        out->pressure_valid = true;
    }
    if (adc_h != BME280_SKIPPED_H) {
        out->humidity_rh = (float)comp_h(&dev->calib, adc_h) / 1024.0f;
        out->humidity_valid = true;
    }
    return BME280_OK;
}

bme280_err_t bme280_set_mode(bme280_dev_t *dev, bme280_mode_t mode)
{
    if (dev == NULL) { return BME280_ERR_NULL; }
    if (!dev->initialized) { return BME280_ERR_INVALID_STATE; }
    if (mode != BME280_MODE_SLEEP && mode != BME280_MODE_FORCED && mode != BME280_MODE_NORMAL) {
        return BME280_ERR_INVALID_ARG;
    }
    dev->config.mode = mode;
    return apply_config(dev);
}

bme280_err_t bme280_reset(bme280_dev_t *dev)
{
    if (dev == NULL) { return BME280_ERR_NULL; }
    if (!dev->initialized) { return BME280_ERR_INVALID_STATE; }
    bme280_err_t err = soft_reset(dev);
    return (err == BME280_OK) ? apply_config(dev) : err;
}

bool bme280_is_ready(const bme280_dev_t *dev)
{
    return dev != NULL && dev->initialized;
}
