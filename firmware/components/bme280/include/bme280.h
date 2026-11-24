#pragma once

/*
 * Platform-independent BME280 driver (Dependency Inversion).
 *
 * The driver depends ONLY on the abstract bus interface bme280_bus_t defined
 * below. It includes nothing but standard C headers. Any I2C implementation
 * whose functions match the bme280_*_fn signatures can be plugged in, e.g.
 * the generic ESP-IDF adapter (esp32_i2c_adapter.h):
 *
 *   bme280_bus_t field | ESP-IDF adapter symbol        | handle type behind void*
 *   -------------------+-------------------------------+---------------------------
 *   handle             | (user-provided)               | i2c_master_dev_handle_t
 *   read               | esp32_i2c_read                |
 *   write              | esp32_i2c_write               |
 *   delay_ms           | esp32_delay_ms                |
 *
 * The adapter does not know about this header - the binding happens in the
 * application code (composition root, e.g. main.c).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* Constants (datasheet BST-BME280-DS002)                                     */
/* -------------------------------------------------------------------------- */

/** 7-bit I2C addresses (SDO -> GND / SDO -> VDDIO) */
#define BME280_I2C_ADDR_PRIMARY   0x76
#define BME280_I2C_ADDR_SECONDARY 0x77

#define BME280_CHIP_ID            0x60 /**< Expected content of the id register */
#define BME280_CMD_RESET          0xB6 /**< Soft reset command word */

/** Register map */
#define BME280_REG_CALIB_00       0x88 /**< calib00..calib25: 0x88..0xA1 */
#define BME280_REG_ID             0xD0 /**< Chip ID register */
#define BME280_REG_RESET          0xE0 /**< Soft reset register */
#define BME280_REG_CALIB_26       0xE1 /**< calib26..calib41: 0xE1..0xF0 */
#define BME280_REG_CTRL_HUM       0xF2 /**< Humidity control register */
#define BME280_REG_STATUS         0xF3 /**< Device status register */
#define BME280_REG_CTRL_MEAS      0xF4 /**< Pressure/temperature control register */
#define BME280_REG_CONFIG         0xF5 /**< Standby, filter, SPI 3-wire */
#define BME280_REG_PRESS_MSB      0xF7 /**< Start of 8-byte data block 0xF7..0xFE */

/** Status register bits (0xF3) */
#define BME280_STATUS_MEASURING   (1 << 3) /**< 1 while a conversion is running */
#define BME280_STATUS_IM_UPDATE   (1 << 0) /**< 1 while NVM data are being copied */

/* -------------------------------------------------------------------------- */
/* Abstract bus interface (the "port" the driver depends on)                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief Read @p len bytes starting at register @p reg_addr of device @p dev_addr
 * @return 0 on success, negative value on error
 */
typedef int (*bme280_read_fn)(void *handle, uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, size_t len);

/**
 * @brief Write @p len bytes starting at register @p reg_addr of device @p dev_addr
 * @return 0 on success, negative value on error
 */
typedef int (*bme280_write_fn)(void *handle, uint8_t dev_addr, uint8_t reg_addr, const uint8_t *data, size_t len);

/**
 * @brief Blocking delay in milliseconds
 */
typedef void (*bme280_delay_ms_fn)(uint32_t ms);

/**
 * @brief Bus interface injected into the driver
 */
typedef struct {
    void *handle;                /**< Opaque bus/device handle, passed unchanged to read/write */
    bme280_read_fn read;         /**< Register read implementation */
    bme280_write_fn write;       /**< Register write implementation */
    bme280_delay_ms_fn delay_ms; /**< Delay implementation */
} bme280_bus_t;

/* -------------------------------------------------------------------------- */
/* Driver types                                                               */
/* -------------------------------------------------------------------------- */

/** Driver result codes (platform-independent) */
typedef enum {
    BME280_OK                =  0,
    BME280_ERR_NULL          = -1, /**< NULL pointer argument or incomplete bus interface */
    BME280_ERR_INVALID_ARG   = -2, /**< Invalid argument value */
    BME280_ERR_INVALID_STATE = -3, /**< Driver not initialized */
    BME280_ERR_BUS           = -4, /**< Bus read/write returned an error */
    BME280_ERR_CHIP_ID       = -5, /**< Chip ID mismatch (not a BME280) */
    BME280_ERR_TIMEOUT       = -6  /**< Device did not become ready in time */
} bme280_err_t;

/** Operating mode (ctrl_meas.mode[1:0]) */
typedef enum {
    BME280_MODE_SLEEP  = 0x00,
    BME280_MODE_FORCED = 0x01,
    BME280_MODE_NORMAL = 0x03
} bme280_mode_t;

/** Oversampling (osrs_t / osrs_p / osrs_h [2:0]) */
typedef enum {
    BME280_OVERSAMPLING_SKIP = 0x00,
    BME280_OVERSAMPLING_X1   = 0x01,
    BME280_OVERSAMPLING_X2   = 0x02,
    BME280_OVERSAMPLING_X4   = 0x03,
    BME280_OVERSAMPLING_X8   = 0x04,
    BME280_OVERSAMPLING_X16  = 0x05
} bme280_oversampling_t;

/** IIR filter coefficient (config.filter[2:0]) */
typedef enum {
    BME280_FILTER_OFF = 0x00,
    BME280_FILTER_2   = 0x01,
    BME280_FILTER_4   = 0x02,
    BME280_FILTER_8   = 0x03,
    BME280_FILTER_16  = 0x04
} bme280_filter_t;

/** Standby time in normal mode (config.t_sb[2:0]) */
typedef enum {
    BME280_STANDBY_0_5_MS  = 0x00,
    BME280_STANDBY_62_5_MS = 0x01,
    BME280_STANDBY_125_MS  = 0x02,
    BME280_STANDBY_250_MS  = 0x03,
    BME280_STANDBY_500_MS  = 0x04,
    BME280_STANDBY_1000_MS = 0x05,
    BME280_STANDBY_10_MS   = 0x06,
    BME280_STANDBY_20_MS   = 0x07
} bme280_standby_time_t;

/** Factory calibration coefficients (datasheet Table 16) */
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
    int32_t  t_fine; /**< Fine temperature shared by P and H compensation */
} bme280_calib_data_t;

/** Measurement configuration */
typedef struct {
    bme280_mode_t mode;
    bme280_oversampling_t osr_temp;
    bme280_oversampling_t osr_press;
    bme280_oversampling_t osr_hum;
    bme280_filter_t filter;
    bme280_standby_time_t standby_time;
} bme280_config_t;

/** Compensated measurement (a skipped channel is reported as 0 with its valid flag false) */
typedef struct {
    float temperature_c; /**< [°C] */
    float pressure_hpa;  /**< [hPa] */
    float humidity_rh;   /**< [%RH] */
    bool pressure_valid;
    bool humidity_valid;
} bme280_data_t;

/** Device descriptor */
typedef struct {
    bme280_bus_t bus;          /**< Injected bus interface */
    uint8_t dev_addr;          /**< 7-bit I2C address */
    bme280_config_t config;    /**< Measurement configuration */
    bme280_calib_data_t calib; /**< Calibration read from NVM */
    uint8_t chip_id;           /**< Chip ID read during init */
    bool initialized;
} bme280_dev_t;

/* -------------------------------------------------------------------------- */
/* API                                                                        */
/* -------------------------------------------------------------------------- */

/**
 * @brief Bind the bus, verify CHIP_ID (0xD0 == 0x60), soft-reset, read calibration and apply config
 *
 * @param[out] dev      Device descriptor
 * @param[in]  bus      Bus interface (copied; all members except handle must be non-NULL)
 * @param[in]  dev_addr BME280_I2C_ADDR_PRIMARY or BME280_I2C_ADDR_SECONDARY
 * @param[in]  config   Measurement configuration
 */
bme280_err_t bme280_init(bme280_dev_t *dev, const bme280_bus_t *bus, uint8_t dev_addr,
                         const bme280_config_t *config);

/** @brief Put the sensor into sleep mode and mark the descriptor as uninitialized */
bme280_err_t bme280_deinit(bme280_dev_t *dev);

/** @brief Read compensated data (triggers a forced conversion unless in NORMAL mode) */
bme280_err_t bme280_get_data(bme280_dev_t *dev, bme280_data_t *out);

/** @brief Change operating mode and re-apply configuration */
bme280_err_t bme280_set_mode(bme280_dev_t *dev, bme280_mode_t mode);

/** @brief Soft reset and re-apply the stored configuration */
bme280_err_t bme280_reset(bme280_dev_t *dev);

/** @brief true if the descriptor is initialized */
bool bme280_is_ready(const bme280_dev_t *dev);

#ifdef __cplusplus
}
#endif
