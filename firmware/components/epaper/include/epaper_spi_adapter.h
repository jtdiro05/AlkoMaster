#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Default and maximum 4-wire SPI write clock frequencies for SSD1680 (per Table 12-1) */
#define SSD1680_SPI_DEFAULT_CLOCK_HZ (4 * 1000 * 1000)  /**< 4 MHz default SPI clock */
#define SSD1680_SPI_MAX_WRITE_HZ     (20 * 1000 * 1000) /**< 20 MHz max write clock */

/**
 * @brief Hardware Adapter interface for the SSD1680 4-wire SPI and control lines.
 *
 * Decouples the SSD1680 display driver from platform-specific SPI and GPIO drivers
 * following the Hardware Adapter design pattern.
 */
typedef struct {
    void *ctx; /**< Adapter-specific context pointer (e.g., epaper_esp32_spi_ctx_t*) */

    /**
     * @brief Initialize underlying SPI bus, device handle, and control GPIOs
     */
    esp_err_t (*init)(void *ctx);

    /**
     * @brief Release underlying SPI device, bus, and GPIO resources
     */
    esp_err_t (*deinit)(void *ctx);

    /**
     * @brief Send a single command byte over 4-wire SPI (D/C# = 0, CS# = 0)
     */
    esp_err_t (*write_cmd)(void *ctx, uint8_t cmd);

    /**
     * @brief Send a buffer of data bytes over 4-wire SPI (D/C# = 1, CS# = 0)
     */
    esp_err_t (*write_data)(void *ctx, const uint8_t *data, size_t len);

    /**
     * @brief Control the hardware reset pin RES# (Active Low: false = asserted, true = released)
     */
    esp_err_t (*set_reset)(void *ctx, bool level);

    /**
     * @brief Read the BUSY status pin (SSD1680 outputs HIGH when busy)
     */
    bool (*is_busy)(void *ctx);

    /**
     * @brief Platform delay in milliseconds
     */
    void (*delay_ms)(uint32_t ms);
} epaper_spi_adapter_t;

/**
 * @brief ESP32-specific SPI and GPIO configuration for the SSD1680 adapter
 */
typedef struct {
    spi_host_device_t spi_host; /**< ESP32 SPI host peripheral (e.g., SPI2_HOST / SPI3_HOST) */
    gpio_num_t mosi_pin;        /**< SDA (MOSI) serial data output pin */
    gpio_num_t sclk_pin;        /**< SCL (SCLK) serial clock output pin */
    gpio_num_t cs_pin;          /**< CS# chip select output pin (Active Low) */
    gpio_num_t dc_pin;          /**< D/C# data/command control pin (Low: Cmd, High: Data) */
    gpio_num_t rst_pin;         /**< RES# hardware reset output pin (Active Low) */
    gpio_num_t busy_pin;        /**< BUSY status input pin (High: Busy) */
    int clock_speed_hz;         /**< SPI clock frequency in Hz (up to 20 MHz for write) */
    int max_transfer_sz;        /**< Maximum SPI transfer size in bytes (0 for default 4096) */
    bool init_spi_bus;          /**< Set true to initialize the SPI bus inside the adapter */
} epaper_esp32_spi_config_t;

/**
 * @brief Concrete runtime context for the ESP32 SPI Hardware Adapter
 */
typedef struct {
    epaper_esp32_spi_config_t config; /**< Hardware pin and bus configuration */
    spi_device_handle_t spi_handle;   /**< ESP-IDF SPI device handle */
    bool bus_owned;                   /**< True if this adapter initialized the SPI bus */
    bool initialized;                 /**< Adapter initialization state */
} epaper_esp32_spi_ctx_t;

/**
 * @brief Create and bind an ESP32 SPI Hardware Adapter instance for the SSD1680 driver
 *
 * @param[out] esp_ctx Pointer to the persistent ESP32 adapter context structure
 * @param[in] config Pointer to the ESP32 SPI and GPIO configuration
 * @param[out] out_adapter Pointer to the generic Hardware Adapter interface to populate
 * @return esp_err_t ESP_OK on success
 */
esp_err_t epaper_spi_esp32_create_adapter(epaper_esp32_spi_ctx_t *esp_ctx,
                                          const epaper_esp32_spi_config_t *config,
                                          epaper_spi_adapter_t *out_adapter);

#ifdef __cplusplus
}
#endif
