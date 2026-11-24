#pragma once

/*
 * Platform-independent DHT22 (AM2302) temperature & humidity sensor driver
 * (Dependency Inversion).
 *
 * The driver depends ONLY on the abstract single-wire bus interface
 * dht22_bus_t defined below and includes only standard C headers. Any single-wire
 * timing implementation (GPIO bit-banging, hardware RMT, test mock) whose
 * functions match the dht22_*_fn signatures can be plugged in, e.g. the
 * ESP-IDF adapter (esp32_dht_adapter.h in esp32_adapters):
 *
 *   dht22_bus_t field | ESP-IDF adapter symbol | handle type behind void*
 *   ------------------+------------------------+-------------------------
 *   handle            | &dht_ctx               | esp32_dht_ctx_t *
 *   read_raw          | esp32_dht_read_raw     |
 *   get_time_ms       | esp32_get_time_ms      |
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DHT22_FRAME_BYTES     5u    /**< 40-bit frame = 5 bytes (RH_H, RH_L, T_H, T_L, CRC) */
#define DHT22_MIN_INTERVAL_MS 2000u /**< Minimum time between physical sensor reads [ms] */

/**
 * @brief Perform a single-wire transaction and capture 5 raw bytes (40 bits) from DHT22
 * @param handle    Opaque bus adapter handle
 * @param[out] data 5-byte output buffer
 * @return 0 on success, negative value on bus timeout or handshake failure
 */
typedef int (*dht22_read_raw_fn)(void *handle, uint8_t data[DHT22_FRAME_BYTES]);

/**
 * @brief Return monotonic system time in milliseconds [ms] (optional, for 2 s caching)
 */
typedef uint32_t (*dht22_get_time_ms_fn)(void);

/**
 * @brief Single-wire bus interface injected into the DHT22 driver
 */
typedef struct {
    void *handle;                     /**< Opaque adapter handle passed unchanged to read_raw */
    dht22_read_raw_fn read_raw;       /**< Required: reads 5 raw bytes from the sensor */
    dht22_get_time_ms_fn get_time_ms; /**< Optional: monotonic clock [ms] for 2 s rate limiting */
} dht22_bus_t;

/** Result codes of the DHT22 driver */
typedef enum {
    DHT22_OK                =  0,
    DHT22_ERR_NULL          = -1, /**< NULL pointer argument or missing read_raw callback */
    DHT22_ERR_INVALID_STATE = -2, /**< Driver not initialized */
    DHT22_ERR_BUS           = -3, /**< Single-wire timeout or no ACK from sensor */
    DHT22_ERR_CHECKSUM      = -4, /**< 8-bit checksum mismatch */
    DHT22_ERR_OUT_OF_RANGE  = -5  /**< Decoded values outside physical range (-40..80 °C, 0..100 %RH) */
} dht22_err_t;

/** Decoded measurement data from the DHT22 sensor */
typedef struct {
    float temperature_c;               /**< Ambient temperature [°C] */
    float humidity_rh;                 /**< Relative humidity [%RH] */
    uint8_t raw[DHT22_FRAME_BYTES];    /**< Raw 5-byte frame from the sensor */
} dht22_data_t;

/** Main descriptor structure representing a DHT22 sensor instance */
typedef struct {
    dht22_bus_t bus;         /**< Injected single-wire bus interface */
    dht22_data_t last_data;  /**< Most recent valid measurement */
    uint32_t last_read_ms;   /**< Timestamp of the last physical bus read [ms] */
    bool has_valid_reading;  /**< True once at least one valid reading was obtained */
    bool initialized;        /**< Initialization status flag */
} dht22_dev_t;

typedef dht22_dev_t dht22_sensor_t;

/**
 * @brief Initialize the DHT22 sensor descriptor and bind the abstract bus interface
 *
 * @param[out] dev Pointer to the DHT22 descriptor
 * @param[in]  bus Pointer to the bus interface (read_raw must be non-NULL)
 * @return dht22_err_t DHT22_OK on success
 */
dht22_err_t dht22_init(dht22_dev_t *dev, const dht22_bus_t *bus);

/**
 * @brief Deinitialize the DHT22 sensor descriptor
 */
dht22_err_t dht22_deinit(dht22_dev_t *dev);

/**
 * @brief Decode and validate a raw 5-byte DHT22 frame (checksum, sign-magnitude T, bounds)
 *
 * @param[in]  raw      5 raw bytes received from the sensor
 * @param[out] out_data Decoded temperature and humidity
 * @return dht22_err_t DHT22_OK on valid frame
 */
dht22_err_t dht22_decode_raw(const uint8_t raw[DHT22_FRAME_BYTES], dht22_data_t *out_data);

/**
 * @brief Read temperature and humidity from the DHT22 sensor.
 *        If get_time_ms is provided and less than 2000 ms elapsed since the last
 *        valid read, returns the cached measurement without stressing the bus.
 *
 * @param[in,out] dev      Pointer to the DHT22 descriptor
 * @param[out]    out_data Decoded measurement data
 * @return dht22_err_t DHT22_OK on success
 */
dht22_err_t dht22_get_data(dht22_dev_t *dev, dht22_data_t *out_data);

/**
 * @brief Check whether the DHT22 descriptor is initialized
 */
bool dht22_is_ready(const dht22_dev_t *dev);

#ifdef __cplusplus
}
#endif
