#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "epaper_spi_adapter.h"
#include "font24.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum panel dimensions supported by the SSD1680 controller (176 Source x 296 Gate) */
#define SSD1680_MAX_WIDTH      176
#define SSD1680_MAX_HEIGHT     296
#define SSD1680_MAX_BUFFER_SZ  ((SSD1680_MAX_WIDTH / 8) * SSD1680_MAX_HEIGHT)

/** Default 2.9" SSD1680 e-Paper physical panel dimensions (128 Source x 296 Gate) */
#define EPAPER_2IN9_WIDTH      128
#define EPAPER_2IN9_HEIGHT     296

/** Monochrome pixel byte values for B/W RAM (0x24) */
#define EPAPER_COLOR_BLACK 0x00 /**< Black pixel / fill byte (bit = 0) */
#define EPAPER_COLOR_WHITE 0xFF /**< White pixel / fill byte (bit = 1) */

/**
 * @brief Display logical orientation modes (default: horizontal / landscape)
 */
typedef enum {
    EPAPER_ORIENTATION_LANDSCAPE = 0,     /**< Default horizontal orientation (296x128 for 2.9") */
    EPAPER_ORIENTATION_PORTRAIT,          /**< Vertical orientation (128x296 for 2.9") */
    EPAPER_ORIENTATION_LANDSCAPE_FLIPPED, /**< Horizontal orientation rotated 180 degrees (296x128) */
    EPAPER_ORIENTATION_PORTRAIT_FLIPPED   /**< Vertical orientation rotated 180 degrees (128x296) */
} epaper_orientation_t;

/**
 * @brief SSD1680 Command Register Definitions (Table 7-1 of SSD1680 Datasheet)
 */
#define SSD1680_CMD_DRIVER_OUTPUT_CTRL     0x01 /**< Gate setting (MUX, GD, SM, TB) */
#define SSD1680_CMD_GATE_VOLTAGE_CTRL      0x03 /**< Gate driving voltage control (VGH) */
#define SSD1680_CMD_SOURCE_VOLTAGE_CTRL    0x04 /**< Source driving voltage control (VSH1, VSH2, VSL) */
#define SSD1680_CMD_BOOSTER_SOFT_START     0x0C /**< Booster soft start control (Phase 1..3) */
#define SSD1680_CMD_GATE_SCAN_START_POS    0x0F /**< Gate scan start position (0..295) */
#define SSD1680_CMD_DEEP_SLEEP_MODE        0x10 /**< Deep sleep mode control */
#define SSD1680_CMD_DATA_ENTRY_MODE        0x11 /**< Data entry sequence (AM, ID[1:0]) */
#define SSD1680_CMD_SW_RESET               0x12 /**< Software reset */
#define SSD1680_CMD_HV_READY_DETECT        0x14 /**< HV ready detection */
#define SSD1680_CMD_VCI_DETECT             0x15 /**< VCI level detection */
#define SSD1680_CMD_TEMP_SENSOR_CTRL       0x18 /**< Temperature sensor selection (0x80 = internal) */
#define SSD1680_CMD_TEMP_SENSOR_WRITE      0x1A /**< Write to temperature register */
#define SSD1680_CMD_TEMP_SENSOR_READ       0x1B /**< Read from temperature register */
#define SSD1680_CMD_MASTER_ACTIVATION      0x20 /**< Activate display update sequence */
#define SSD1680_CMD_DISPLAY_UPDATE_CTRL1   0x21 /**< RAM content option for display update */
#define SSD1680_CMD_DISPLAY_UPDATE_CTRL2   0x22 /**< Display update sequence option */
#define SSD1680_CMD_WRITE_RAM_BW           0x24 /**< Write Black/White RAM (0 = Black, 1 = White) */
#define SSD1680_CMD_READ_RAM               0x27 /**< Read RAM */
#define SSD1680_CMD_VCOM_SENSE             0x28 /**< Enter VCOM sensing mode */
#define SSD1680_CMD_VCOM_SENSE_DURATION    0x29 /**< VCOM sense duration */
#define SSD1680_CMD_PROGRAM_VCOM_OTP       0x2A /**< Program VCOM register into OTP */
#define SSD1680_CMD_WRITE_VCOM_CTRL        0x2B /**< Write register for VCOM control */
#define SSD1680_CMD_WRITE_VCOM_REG         0x2C /**< Write VCOM register */
#define SSD1680_CMD_OTP_REG_READ           0x2D /**< Read OTP register for display option */
#define SSD1680_CMD_USER_ID_READ           0x2E /**< Read 10-byte User ID from OTP */
#define SSD1680_CMD_STATUS_BIT_READ        0x2F /**< Read IC status bits */
#define SSD1680_CMD_PROGRAM_WS_OTP         0x30 /**< Program OTP of Waveform Setting */
#define SSD1680_CMD_LOAD_WS_OTP            0x31 /**< Load OTP of Waveform Setting */
#define SSD1680_CMD_WRITE_LUT_REG          0x32 /**< Write 153-byte LUT register */
#define SSD1680_CMD_BORDER_WAVEFORM_CTRL   0x3C /**< Select border waveform for VBD */
#define SSD1680_CMD_END_OPTION             0x3F /**< Option for LUT end (EOPT) */
#define SSD1680_CMD_READ_RAM_OPTION        0x41 /**< Select RAM for reading */
#define SSD1680_CMD_SET_RAM_X_RANGE        0x44 /**< Set RAM X-address start/end position */
#define SSD1680_CMD_SET_RAM_Y_RANGE        0x45 /**< Set RAM Y-address start/end position */
#define SSD1680_CMD_AUTO_WRITE_BW_RAM      0x47 /**< Auto write B/W RAM for regular pattern */
#define SSD1680_CMD_SET_RAM_X_COUNTER      0x4E /**< Set RAM X address counter */
#define SSD1680_CMD_SET_RAM_Y_COUNTER      0x4F /**< Set RAM Y address counter */
#define SSD1680_CMD_NOP                    0x7F /**< No operation */

/**
 * @brief SSD1680 Data Entry Mode settings (Register 0x11, Section 8.3)
 */
typedef enum {
    SSD1680_DATA_ENTRY_X_DEC_Y_DEC = 0x00, /**< X decrement, Y decrement, X-mode */
    SSD1680_DATA_ENTRY_X_INC_Y_DEC = 0x01, /**< X increment, Y decrement, X-mode */
    SSD1680_DATA_ENTRY_X_DEC_Y_INC = 0x02, /**< X decrement, Y increment, X-mode */
    SSD1680_DATA_ENTRY_X_INC_Y_INC = 0x03, /**< X increment, Y increment, X-mode (POR default) */
    SSD1680_DATA_ENTRY_Y_MODE_FLAG = 0x04  /**< Bit mask for Y-direction address counter update (AM=1) */
} epaper_data_entry_mode_t;

/**
 * @brief SSD1680 Deep Sleep Mode options (Register 0x10)
 */
typedef enum {
    SSD1680_SLEEP_MODE_NORMAL = 0x00, /**< Normal operating mode (POR) */
    SSD1680_SLEEP_MODE_1      = 0x01, /**< Deep Sleep Mode 1 (retains RAM data) */
    SSD1680_SLEEP_MODE_2      = 0x03  /**< Deep Sleep Mode 2 (does not retain RAM data) */
} epaper_sleep_mode_t;

/**
 * @brief SSD1680 Display Update Sequence options (Register 0x22)
 */
typedef enum {
    SSD1680_UPDATE_FULL_MODE_1    = 0xF7, /**< Enable clock & analog, load temp, Display Mode 1, disable analog & OSC */
    SSD1680_UPDATE_PARTIAL_MODE_2 = 0xFF, /**< Enable clock & analog, load temp, Display Mode 2, disable analog & OSC */
    SSD1680_UPDATE_DISPLAY_ONLY_1 = 0xC7, /**< Enable clock & analog, Display Mode 1, disable analog & OSC */
    SSD1680_UPDATE_DISPLAY_ONLY_2 = 0xCF  /**< Enable clock & analog, Display Mode 2, disable analog & OSC */
} epaper_update_mode_t;

/**
 * @brief Panel geometry and driver configuration for a monochrome B/W SSD1680 e-Paper display
 */
typedef struct {
    uint16_t width;                           /**< Physical Source width in pixels (128 for 2.9", max 176) */
    uint16_t height;                          /**< Physical Gate height in pixels (296 for 2.9", max 296) */
    epaper_orientation_t orientation;         /**< Display orientation (default 0: EPAPER_ORIENTATION_LANDSCAPE) */
    epaper_data_entry_mode_t data_entry_mode; /**< RAM address increment mode (default: SSD1680_DATA_ENTRY_X_INC_Y_INC) */
    uint8_t border_waveform;                  /**< Border waveform register 0x3C setting (default: 0x05) */
    uint32_t busy_timeout_ms;                 /**< Maximum time to wait for BUSY pin to go low [ms] */
} epaper_panel_config_t;

/**
 * @brief Main descriptor structure representing a monochrome B/W SSD1680 e-Paper display instance
 */
typedef struct {
    epaper_panel_config_t panel;                 /**< Panel hardware dimensions and operating parameters */
    epaper_spi_adapter_t adapter;                /**< Hardware Adapter interface for SPI and control GPIOs */
    uint8_t framebuffer[SSD1680_MAX_BUFFER_SZ];  /**< Internal 1-bpp Black/White framebuffer */
    size_t buffer_size;                          /**< Active framebuffer size in bytes ((aligned_width / 8) * height) */
    uint16_t stride_bytes;                       /**< Number of bytes per physical row (aligned_width / 8) */
    uint16_t logical_width;                      /**< Active drawing width after applying orientation (296 in landscape) */
    uint16_t logical_height;                     /**< Active drawing height after applying orientation (128 in landscape) */
    bool is_sleeping;                            /**< True if the controller is currently in Deep Sleep mode */
    bool initialized;                            /**< Driver initialization flag */
} epaper_display_t;

/**
 * @brief Initialize the monochrome SSD1680 e-Paper display using the provided Hardware Adapter.
 *        Defaults to horizontal (landscape) orientation (296x128).
 *
 * @param[out] display Pointer to the e-Paper display descriptor
 * @param[in] panel_cfg Pointer to the panel configuration structure
 * @param[in] adapter Pointer to the bound SPI Hardware Adapter interface
 * @return esp_err_t ESP_OK on success
 */
esp_err_t epaper_init(epaper_display_t *display,
                      const epaper_panel_config_t *panel_cfg,
                      const epaper_spi_adapter_t *adapter);

/**
 * @brief Deinitialize the SSD1680 e-Paper display and release adapter resources
 *
 * @param[in,out] display Pointer to the e-Paper display descriptor
 * @return esp_err_t ESP_OK on success
 */
esp_err_t epaper_deinit(epaper_display_t *display);

/**
 * @brief Change the active logical drawing orientation (landscape / portrait)
 *
 * @param[in,out] display Pointer to the e-Paper display descriptor
 * @param[in] orientation Desired display orientation
 * @return esp_err_t ESP_OK on success
 */
esp_err_t epaper_set_orientation(epaper_display_t *display, epaper_orientation_t orientation);

/**
 * @brief Perform a hardware reset sequence via the RES# pin and wait until idle
 *
 * @param[in,out] display Pointer to the e-Paper display descriptor
 * @return esp_err_t ESP_OK on success
 */
esp_err_t epaper_hw_reset(epaper_display_t *display);

/**
 * @brief Send the software reset command (0x12) and wait until BUSY goes low
 *
 * @param[in,out] display Pointer to the e-Paper display descriptor
 * @return esp_err_t ESP_OK on success
 */
esp_err_t epaper_sw_reset(epaper_display_t *display);

/**
 * @brief Poll the BUSY pin until the SSD1680 finishes its current operation or times out
 *
 * @param[in] display Pointer to the e-Paper display descriptor
 * @param[in] timeout_ms Maximum wait time in milliseconds
 * @return esp_err_t ESP_OK when idle, ESP_ERR_TIMEOUT on timeout
 */
esp_err_t epaper_wait_busy(const epaper_display_t *display, uint32_t timeout_ms);

/**
 * @brief Configure the active RAM address window (Commands 0x44 and 0x45)
 *
 * @param[in,out] display Pointer to the e-Paper display descriptor
 * @param[in] x_start Starting physical X pixel coordinate (must be a multiple of 8)
 * @param[in] y_start Starting physical Y pixel coordinate
 * @param[in] x_end Ending physical X pixel coordinate
 * @param[in] y_end Ending physical Y pixel coordinate
 * @return esp_err_t ESP_OK on success
 */
esp_err_t epaper_set_window(epaper_display_t *display,
                            uint16_t x_start, uint16_t y_start,
                            uint16_t x_end, uint16_t y_end);

/**
 * @brief Set the RAM X and Y address counters (Commands 0x4E and 0x4F)
 *
 * @param[in,out] display Pointer to the e-Paper display descriptor
 * @param[in] x Physical X pixel coordinate (divided by 8 internally for X address counter)
 * @param[in] y Physical Y pixel coordinate
 * @return esp_err_t ESP_OK on success
 */
esp_err_t epaper_set_cursor(epaper_display_t *display, uint16_t x, uint16_t y);

/**
 * @brief Trigger the display update sequence (Commands 0x22 and 0x20) and wait for completion
 *
 * @param[in,out] display Pointer to the e-Paper display descriptor
 * @param[in] mode Update mode (e.g., SSD1680_UPDATE_FULL_MODE_1)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t epaper_update(epaper_display_t *display, epaper_update_mode_t mode);

/**
 * @brief Fill the internal framebuffer and B/W display RAM (0x24) with a constant byte pattern and refresh
 *
 * @param[in,out] display Pointer to the e-Paper display descriptor
 * @param[in] pattern_byte Byte pattern to fill (EPAPER_COLOR_WHITE = 0xFF, EPAPER_COLOR_BLACK = 0x00)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t epaper_fill(epaper_display_t *display, uint8_t pattern_byte);

/**
 * @brief Clear the entire display to White (0xFF) and refresh the panel
 *
 * @param[in,out] display Pointer to the e-Paper display descriptor
 * @return esp_err_t ESP_OK on success
 */
esp_err_t epaper_clear(epaper_display_t *display);

/**
 * @brief Write a full-screen 1-bpp Black/White image buffer to RAM (0x24) and refresh the panel
 *
 * @param[in,out] display Pointer to the e-Paper display descriptor
 * @param[in] bw_buf Pointer to Black/White bitmap buffer (size: display->buffer_size; 1 = White, 0 = Black)
 * @param[in] mode Display update mode to execute after writing RAM
 * @return esp_err_t ESP_OK on success
 */
esp_err_t epaper_write_framebuffer(epaper_display_t *display,
                                   const uint8_t *bw_buf,
                                   epaper_update_mode_t mode);

/**
 * @brief Draw a single pixel at logical coordinates (x, y) into the internal framebuffer
 *
 * @param[in,out] display Pointer to the e-Paper display descriptor
 * @param[in] x Horizontal pixel coordinate (0 .. logical_width - 1; 0..295 in landscape)
 * @param[in] y Vertical pixel coordinate (0 .. logical_height - 1; 0..127 in landscape)
 * @param[in] color EPAPER_COLOR_BLACK (0x00) or EPAPER_COLOR_WHITE (0xFF)
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if out of bounds
 */
esp_err_t epaper_draw_pixel(epaper_display_t *display, uint16_t x, uint16_t y, uint8_t color);

/**
 * @brief Render a single ASCII character at logical coordinates (x, y) into the internal framebuffer
 *
 * @param[in,out] display Pointer to the e-Paper display descriptor
 * @param[in] x Top-left X pixel coordinate of the character cell
 * @param[in] y Top-left Y pixel coordinate of the character cell
 * @param[in] c ASCII character to draw
 * @param[in] font Pointer to the bitmap font descriptor (e.g., &epaper_font24)
 * @param[in] color Foreground text color (EPAPER_COLOR_BLACK or EPAPER_COLOR_WHITE)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t epaper_draw_char(epaper_display_t *display,
                           uint16_t x, uint16_t y,
                           char c,
                           const epaper_font_t *font,
                           uint8_t color);

/**
 * @brief Render an arbitrary null-terminated string starting at (x, y) into the internal framebuffer
 *        with automatic line wrapping based on the active logical orientation width.
 *
 * @param[in,out] display Pointer to the e-Paper display descriptor
 * @param[in] x Starting X pixel coordinate
 * @param[in] y Starting Y pixel coordinate
 * @param[in] text Null-terminated string to render
 * @param[in] font Pointer to the bitmap font descriptor (e.g., &epaper_font24)
 * @param[in] color Foreground text color (EPAPER_COLOR_BLACK or EPAPER_COLOR_WHITE)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t epaper_draw_string(epaper_display_t *display,
                             uint16_t x, uint16_t y,
                             const char *text,
                             const epaper_font_t *font,
                             uint8_t color);

/**
 * @brief Render an arbitrary string starting at (x, y) with automatic line wrapping
 *        and immediately update the e-Paper screen with the internal framebuffer.
 *
 * @param[in,out] display Pointer to the e-Paper display descriptor
 * @param[in] x Starting X pixel coordinate
 * @param[in] y Starting Y pixel coordinate
 * @param[in] text Null-terminated string to display
 * @param[in] font Pointer to the bitmap font descriptor (e.g., &epaper_font24)
 * @param[in] color Foreground text color (EPAPER_COLOR_BLACK or EPAPER_COLOR_WHITE)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t epaper_display_string(epaper_display_t *display,
                                uint16_t x, uint16_t y,
                                const char *text,
                                const epaper_font_t *font,
                                uint8_t color);

/**
 * @brief Put the SSD1680 controller into Deep Sleep mode (Command 0x10)
 *        Note: Exiting Deep Sleep requires calling epaper_hw_reset() or epaper_init().
 *
 * @param[in,out] display Pointer to the e-Paper display descriptor
 * @param[in] sleep_mode Deep sleep mode (SSD1680_SLEEP_MODE_1 or SSD1680_SLEEP_MODE_2)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t epaper_sleep(epaper_display_t *display, epaper_sleep_mode_t sleep_mode);

/** Error codes for float_to_str_1dec() */
#define FLOAT_TO_STR_ERR_INVALID_ARG   (-1) /**< NULL pointer or buf_size == 0 */
#define FLOAT_TO_STR_ERR_NAN           (-2) /**< Value is NaN (buf receives "nan" if buf_size >= 4) */
#define FLOAT_TO_STR_ERR_INF           (-3) /**< Value is +/-Inf (buf receives "inf"/"-inf" if fits) */
#define FLOAT_TO_STR_ERR_OVERFLOW      (-4) /**< Magnitude too large to format */
#define FLOAT_TO_STR_ERR_BUF_TOO_SMALL (-5) /**< Output buffer too small to store formatted string */

/**
 * @brief Efficient and safe conversion of a float to a null-terminated string
 *        with exactly 1 decimal place (embedded / bare-metal friendly).
 *
 * - Rounding: round-half-away-from-zero (e.g. 12.34 -> "12.3", 12.36 -> "12.4", 9.96 -> "10.0")
 * - Small negative values rounding to 0 are formatted without '-' (e.g. -0.04 -> "0.0")
 * - Decimal separator: '.'
 * - No dynamic memory allocation or stdio (sprintf/snprintf) used.
 * - Always null-terminated if buf != NULL and buf_size > 0.
 *
 * @param[in] value Input floating-point value
 * @param[out] buf Output buffer
 * @param[in] buf_size Size of the output buffer in bytes
 * @return Number of characters written (excluding '\0') on success, or a negative error code
 */
int float_to_str_1dec(float value, char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif
