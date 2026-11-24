/*
 * Platform-independent DHT22 (AM2302) driver.
 * Only standard C headers are used; all bus I/O goes through dev->bus.
 */
#include "dht22.h"

dht22_err_t dht22_init(dht22_dev_t *dev, const dht22_bus_t *bus)
{
    if (dev == NULL || bus == NULL || bus->read_raw == NULL) {
        return DHT22_ERR_NULL;
    }

    *dev = (dht22_dev_t){ 0 };
    dev->bus = *bus;
    dev->initialized = true;
    return DHT22_OK;
}

dht22_err_t dht22_deinit(dht22_dev_t *dev)
{
    if (dev == NULL) {
        return DHT22_ERR_NULL;
    }
    if (!dev->initialized) {
        return DHT22_ERR_INVALID_STATE;
    }

    dev->initialized = false;
    dev->has_valid_reading = false;
    return DHT22_OK;
}

dht22_err_t dht22_decode_raw(const uint8_t raw[DHT22_FRAME_BYTES], dht22_data_t *out_data)
{
    if (raw == NULL || out_data == NULL) {
        return DHT22_ERR_NULL;
    }

    uint8_t sum = (uint8_t)(raw[0] + raw[1] + raw[2] + raw[3]);
    if (sum != raw[4]) {
        return DHT22_ERR_CHECKSUM;
    }

    uint16_t raw_rh = (uint16_t)(((uint16_t)raw[0] << 8) | raw[1]);
    float rh = (float)raw_rh / 10.0f;

    /* Temperature uses sign-magnitude encoding: bit 7 of raw[2] is the sign bit */
    uint16_t raw_t_mag = (uint16_t)((((uint16_t)raw[2] & 0x7Fu) << 8) | raw[3]);
    float temp_c = (float)raw_t_mag / 10.0f;
    if ((raw[2] & 0x80u) != 0u) {
        temp_c = -temp_c;
    }

    if (rh < 0.0f || rh > 100.0f || temp_c < -40.0f || temp_c > 80.0f) {
        return DHT22_ERR_OUT_OF_RANGE;
    }

    out_data->temperature_c = temp_c;
    out_data->humidity_rh = rh;
    for (size_t i = 0; i < DHT22_FRAME_BYTES; i++) {
        out_data->raw[i] = raw[i];
    }
    return DHT22_OK;
}

dht22_err_t dht22_get_data(dht22_dev_t *dev, dht22_data_t *out_data)
{
    if (dev == NULL || out_data == NULL) {
        return DHT22_ERR_NULL;
    }
    if (!dev->initialized) {
        return DHT22_ERR_INVALID_STATE;
    }

    /* Enforce the 2-second minimum sampling period if a clock callback is provided */
    if (dev->bus.get_time_ms != NULL && dev->has_valid_reading) {
        uint32_t now_ms = dev->bus.get_time_ms();
        if ((uint32_t)(now_ms - dev->last_read_ms) < DHT22_MIN_INTERVAL_MS) {
            *out_data = dev->last_data;
            return DHT22_OK;
        }
    }

    uint8_t raw[DHT22_FRAME_BYTES] = { 0 };
    if (dev->bus.read_raw(dev->bus.handle, raw) != 0) {
        return DHT22_ERR_BUS;
    }

    dht22_data_t decoded = { 0 };
    dht22_err_t err = dht22_decode_raw(raw, &decoded);
    if (err != DHT22_OK) {
        return err;
    }

    dev->last_data = decoded;
    if (dev->bus.get_time_ms != NULL) {
        dev->last_read_ms = dev->bus.get_time_ms();
    }
    dev->has_valid_reading = true;
    *out_data = decoded;
    return DHT22_OK;
}

bool dht22_is_ready(const dht22_dev_t *dev)
{
    return dev != NULL && dev->initialized;
}
