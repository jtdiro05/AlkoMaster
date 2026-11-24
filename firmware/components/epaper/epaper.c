#include "epaper.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "epaper_ssd1680";

#define SSD1680_DEFAULT_BUSY_TIMEOUT_MS 5000
#define SSD1680_TEMP_SENSOR_INTERNAL    0x80
#define SSD1680_BYPASS_RED_RAM_OPT      0x40 /**< A[7:4] = 0100b: Bypass Red RAM content as 0 */

/** Helper to send a single command byte via the Hardware Adapter */
static inline esp_err_t ssd1680_send_cmd(const epaper_display_t *display, uint8_t cmd)
{
    return display->adapter.write_cmd(display->adapter.ctx, cmd);
}

/** Helper to send a buffer of data bytes via the Hardware Adapter */
static inline esp_err_t ssd1680_send_data(const epaper_display_t *display, const uint8_t *data, size_t len)
{
    return display->adapter.write_data(display->adapter.ctx, data, len);
}

/** Helper to send a single data byte via the Hardware Adapter */
static inline esp_err_t ssd1680_send_data_byte(const epaper_display_t *display, uint8_t byte)
{
    return display->adapter.write_data(display->adapter.ctx, &byte, 1);
}

static void epaper_update_logical_dimensions(epaper_display_t *display)
{
    if (display->panel.orientation == EPAPER_ORIENTATION_LANDSCAPE ||
        display->panel.orientation == EPAPER_ORIENTATION_LANDSCAPE_FLIPPED) {
        display->logical_width = display->panel.height;
        display->logical_height = display->panel.width;
    } else {
        display->logical_width = display->panel.width;
        display->logical_height = display->panel.height;
    }
}

esp_err_t epaper_set_orientation(epaper_display_t *display, epaper_orientation_t orientation)
{
    if (display == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    display->panel.orientation = orientation;
    epaper_update_logical_dimensions(display);
    return ESP_OK;
}

esp_err_t epaper_wait_busy(const epaper_display_t *display, uint32_t timeout_ms)
{
    if (display == NULL || display->adapter.is_busy == NULL || display->adapter.delay_ms == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t elapsed = 0;
    const uint32_t step_ms = 10;

    while (display->adapter.is_busy(display->adapter.ctx)) {
        if (elapsed >= timeout_ms) {
            ESP_LOGE(TAG, "Timeout waiting for SSD1680 BUSY signal (%lu ms)", (unsigned long)timeout_ms);
            return ESP_ERR_TIMEOUT;
        }
        display->adapter.delay_ms(step_ms);
        elapsed += step_ms;
    }

    return ESP_OK;
}

esp_err_t epaper_hw_reset(epaper_display_t *display)
{
    if (display == NULL || display->adapter.set_reset == NULL || display->adapter.delay_ms == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Section 9.1 Step 2: Hardware Reset pulse (RES# active low) */
    esp_err_t err = display->adapter.set_reset(display->adapter.ctx, true);
    if (err != ESP_OK) {
        return err;
    }
    display->adapter.delay_ms(10);

    err = display->adapter.set_reset(display->adapter.ctx, false);
    if (err != ESP_OK) {
        return err;
    }
    display->adapter.delay_ms(10);

    err = display->adapter.set_reset(display->adapter.ctx, true);
    if (err != ESP_OK) {
        return err;
    }
    display->adapter.delay_ms(10);

    display->is_sleeping = false;
    return epaper_wait_busy(display, display->panel.busy_timeout_ms);
}

esp_err_t epaper_sw_reset(epaper_display_t *display)
{
    if (display == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ssd1680_send_cmd(display, SSD1680_CMD_SW_RESET);
    if (err != ESP_OK) {
        return err;
    }
    display->adapter.delay_ms(10);

    return epaper_wait_busy(display, display->panel.busy_timeout_ms);
}

esp_err_t epaper_set_window(epaper_display_t *display,
                            uint16_t x_start, uint16_t y_start,
                            uint16_t x_end, uint16_t y_end)
{
    if (display == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Section 8.4: Set RAM X - Address Start / End Position (0x44), units of 8 pixels */
    uint8_t x_range[2] = {
        (uint8_t)((x_start >> 3) & 0x1F),
        (uint8_t)((x_end >> 3) & 0x1F),
    };
    esp_err_t err = ssd1680_send_cmd(display, SSD1680_CMD_SET_RAM_X_RANGE);
    if (err != ESP_OK) {
        return err;
    }
    err = ssd1680_send_data(display, x_range, sizeof(x_range));
    if (err != ESP_OK) {
        return err;
    }

    /* Section 8.5: Set RAM Y - Address Start / End Position (0x45), 9-bit addresses */
    uint8_t y_range[4] = {
        (uint8_t)(y_start & 0xFF),
        (uint8_t)((y_start >> 8) & 0x01),
        (uint8_t)(y_end & 0xFF),
        (uint8_t)((y_end >> 8) & 0x01),
    };
    err = ssd1680_send_cmd(display, SSD1680_CMD_SET_RAM_Y_RANGE);
    if (err != ESP_OK) {
        return err;
    }
    return ssd1680_send_data(display, y_range, sizeof(y_range));
}

esp_err_t epaper_set_cursor(epaper_display_t *display, uint16_t x, uint16_t y)
{
    if (display == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Section 8.6: Set RAM X address counter (0x4E) */
    esp_err_t err = ssd1680_send_cmd(display, SSD1680_CMD_SET_RAM_X_COUNTER);
    if (err != ESP_OK) {
        return err;
    }
    err = ssd1680_send_data_byte(display, (uint8_t)((x >> 3) & 0x1F));
    if (err != ESP_OK) {
        return err;
    }

    /* Section 8.6: Set RAM Y address counter (0x4F) */
    uint8_t y_addr[2] = {
        (uint8_t)(y & 0xFF),
        (uint8_t)((y >> 8) & 0x01),
    };
    err = ssd1680_send_cmd(display, SSD1680_CMD_SET_RAM_Y_COUNTER);
    if (err != ESP_OK) {
        return err;
    }
    return ssd1680_send_data(display, y_addr, sizeof(y_addr));
}

esp_err_t epaper_init(epaper_display_t *display,
                      const epaper_panel_config_t *panel_cfg,
                      const epaper_spi_adapter_t *adapter)
{
    if (display == NULL || panel_cfg == NULL || adapter == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (panel_cfg->width == 0 || panel_cfg->height == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (adapter->init == NULL || adapter->write_cmd == NULL || adapter->write_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(display, 0, sizeof(epaper_display_t));
    display->panel = *panel_cfg;
    display->adapter = *adapter;

    /* If width and height were passed in landscape order (e.g. 296x128), normalize physical Source x Gate */
    if (display->panel.width > SSD1680_MAX_WIDTH && display->panel.height <= SSD1680_MAX_WIDTH) {
        uint16_t tmp = display->panel.width;
        display->panel.width = display->panel.height;
        display->panel.height = tmp;
    }

    if (display->panel.width > SSD1680_MAX_WIDTH || display->panel.height > SSD1680_MAX_HEIGHT) {
        return ESP_ERR_INVALID_ARG;
    }

    if (display->panel.busy_timeout_ms == 0) {
        display->panel.busy_timeout_ms = SSD1680_DEFAULT_BUSY_TIMEOUT_MS;
    }

    /* Align physical Source width up to 8 pixels for 1-bpp RAM byte calculation */
    uint16_t aligned_width = (display->panel.width + 7U) & ~7U;
    display->stride_bytes = aligned_width / 8U;
    display->buffer_size = (size_t)display->stride_bytes * display->panel.height;
    epaper_update_logical_dimensions(display);

    /* Default internal framebuffer to White (0xFF) */
    memset(display->framebuffer, EPAPER_COLOR_WHITE, display->buffer_size);

    /* Step 1 & 2: Initialize SPI/GPIO Hardware Adapter, perform HW Reset and SW Reset (0x12) */
    esp_err_t err = display->adapter.init(display->adapter.ctx);
    if (err != ESP_OK) {
        return err;
    }

    err = epaper_hw_reset(display);
    if (err != ESP_OK) {
        return err;
    }

    err = epaper_sw_reset(display);
    if (err != ESP_OK) {
        return err;
    }

    /* Step 3a: Driver Output Control (0x01) -> MUX = height - 1, GD=0, SM=0, TB=0 */
    uint16_t mux = display->panel.height - 1U;
    uint8_t driver_ctrl[3] = {
        (uint8_t)(mux & 0xFF),
        (uint8_t)((mux >> 8) & 0x01),
        0x00, /* B[2:0]: GD=0, SM=0, TB=0 */
    };
    err = ssd1680_send_cmd(display, SSD1680_CMD_DRIVER_OUTPUT_CTRL);
    if (err != ESP_OK) {
        return err;
    }
    err = ssd1680_send_data(display, driver_ctrl, sizeof(driver_ctrl));
    if (err != ESP_OK) {
        return err;
    }

    /* Step 3b: Data Entry Mode Setting (0x11) */
    uint8_t entry_mode = (uint8_t)(display->panel.data_entry_mode & 0x07);
    err = ssd1680_send_cmd(display, SSD1680_CMD_DATA_ENTRY_MODE);
    if (err != ESP_OK) {
        return err;
    }
    err = ssd1680_send_data_byte(display, entry_mode);
    if (err != ESP_OK) {
        return err;
    }

    /* Step 3c: Set RAM X and Y window to cover full panel (0x44, 0x45) */
    err = epaper_set_window(display, 0, 0, aligned_width - 1U, display->panel.height - 1U);
    if (err != ESP_OK) {
        return err;
    }

    /* Step 3d: Border Waveform Control (0x3C) */
    uint8_t border = (display->panel.border_waveform != 0) ? display->panel.border_waveform : 0x05;
    err = ssd1680_send_cmd(display, SSD1680_CMD_BORDER_WAVEFORM_CTRL);
    if (err != ESP_OK) {
        return err;
    }
    err = ssd1680_send_data_byte(display, border);
    if (err != ESP_OK) {
        return err;
    }

    /* Step 3e: Display Update Control 1 (0x21) -> Bypass Red RAM as 0 (0x40), Source mode S8..S167 (0x80) for 2.9" 128x296 */
    uint8_t update_ctrl1[2] = { SSD1680_BYPASS_RED_RAM_OPT, 0x80 };
    err = ssd1680_send_cmd(display, SSD1680_CMD_DISPLAY_UPDATE_CTRL1);
    if (err != ESP_OK) {
        return err;
    }
    err = ssd1680_send_data(display, update_ctrl1, sizeof(update_ctrl1));
    if (err != ESP_OK) {
        return err;
    }

    /* Step 4: Select Internal Temperature Sensor (0x18 -> 0x80) */
    err = ssd1680_send_cmd(display, SSD1680_CMD_TEMP_SENSOR_CTRL);
    if (err != ESP_OK) {
        return err;
    }
    err = ssd1680_send_data_byte(display, SSD1680_TEMP_SENSOR_INTERNAL);
    if (err != ESP_OK) {
        return err;
    }

    err = epaper_set_cursor(display, 0, 0);
    if (err != ESP_OK) {
        return err;
    }

    err = epaper_wait_busy(display, display->panel.busy_timeout_ms);
    if (err != ESP_OK) {
        return err;
    }

    display->initialized = true;
    ESP_LOGI(TAG, "SSD1680 initialized (physical=%ux%u, logical=%ux%u, orientation=%d, buffer=%u bytes)",
             display->panel.width, display->panel.height,
             display->logical_width, display->logical_height,
             (int)display->panel.orientation,
             (unsigned)display->buffer_size);
    return ESP_OK;
}

esp_err_t epaper_deinit(epaper_display_t *display)
{
    if (display == NULL || !display->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = ESP_OK;
    if (display->adapter.deinit != NULL) {
        err = display->adapter.deinit(display->adapter.ctx);
    }

    display->initialized = false;
    ESP_LOGI(TAG, "SSD1680 deinitialized");
    return err;
}

esp_err_t epaper_update(epaper_display_t *display, epaper_update_mode_t mode)
{
    if (display == NULL || !display->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Section 9.1 Step 5: Drive display panel by Command 0x22, 0x20 and wait BUSY low */
    esp_err_t err = ssd1680_send_cmd(display, SSD1680_CMD_DISPLAY_UPDATE_CTRL2);
    if (err != ESP_OK) {
        return err;
    }
    err = ssd1680_send_data_byte(display, (uint8_t)mode);
    if (err != ESP_OK) {
        return err;
    }

    err = ssd1680_send_cmd(display, SSD1680_CMD_MASTER_ACTIVATION);
    if (err != ESP_OK) {
        return err;
    }

    return epaper_wait_busy(display, display->panel.busy_timeout_ms);
}

esp_err_t epaper_fill(epaper_display_t *display, uint8_t pattern_byte)
{
    if (display == NULL || !display->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(display->framebuffer, pattern_byte, display->buffer_size);
    return epaper_write_framebuffer(display, display->framebuffer, SSD1680_UPDATE_FULL_MODE_1);
}

esp_err_t epaper_clear(epaper_display_t *display)
{
    return epaper_fill(display, EPAPER_COLOR_WHITE);
}

esp_err_t epaper_write_framebuffer(epaper_display_t *display,
                                   const uint8_t *bw_buf,
                                   epaper_update_mode_t mode)
{
    if (display == NULL || !display->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (bw_buf == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = epaper_set_cursor(display, 0, 0);
    if (err != ESP_OK) {
        return err;
    }

    err = ssd1680_send_cmd(display, SSD1680_CMD_WRITE_RAM_BW);
    if (err != ESP_OK) {
        return err;
    }

    err = ssd1680_send_data(display, bw_buf, display->buffer_size);
    if (err != ESP_OK) {
        return err;
    }

    return epaper_update(display, mode);
}

esp_err_t epaper_draw_pixel(epaper_display_t *display, uint16_t x, uint16_t y, uint8_t color)
{
    if (display == NULL || !display->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (x >= display->logical_width || y >= display->logical_height) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t phys_x;
    uint16_t phys_y;

    switch (display->panel.orientation) {
        case EPAPER_ORIENTATION_LANDSCAPE:
            phys_x = (display->panel.width - 1U) - y;
            phys_y = x;
            break;
        case EPAPER_ORIENTATION_LANDSCAPE_FLIPPED:
            phys_x = y;
            phys_y = (display->panel.height - 1U) - x;
            break;
        case EPAPER_ORIENTATION_PORTRAIT_FLIPPED:
            phys_x = (display->panel.width - 1U) - x;
            phys_y = (display->panel.height - 1U) - y;
            break;
        case EPAPER_ORIENTATION_PORTRAIT:
        default:
            phys_x = x;
            phys_y = y;
            break;
    }

    size_t byte_idx = (size_t)phys_y * display->stride_bytes + (phys_x >> 3);
    uint8_t bit_mask = (uint8_t)(0x80U >> (phys_x & 0x07U));

    if (color == EPAPER_COLOR_BLACK) {
        display->framebuffer[byte_idx] &= (uint8_t)~bit_mask;
    } else {
        display->framebuffer[byte_idx] |= bit_mask;
    }

    return ESP_OK;
}

esp_err_t epaper_draw_char(epaper_display_t *display,
                           uint16_t x, uint16_t y,
                           char c,
                           const epaper_font_t *font,
                           uint8_t color)
{
    if (display == NULL || !display->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (font == NULL || font->table == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Replace non-printable characters with space */
    if (c < font->first_char || c > font->last_char) {
        c = ' ';
    }

    uint8_t bg_color = (color == EPAPER_COLOR_BLACK) ? EPAPER_COLOR_WHITE : EPAPER_COLOR_BLACK;
    size_t char_offset = (size_t)(c - font->first_char) * font->bytes_per_char;
    const uint8_t *glyph = &font->table[char_offset];

    for (uint8_t row = 0; row < font->height; row++) {
        uint16_t py = y + row;
        if (py >= display->logical_height) {
            break;
        }

        const uint8_t *row_bytes = &glyph[(size_t)row * font->bytes_per_row];
        for (uint8_t col = 0; col < font->width; col++) {
            uint16_t px = x + col;
            if (px >= display->logical_width) {
                break;
            }

            uint8_t byte_val = row_bytes[col >> 3];
            bool bit_set = (byte_val & (0x80U >> (col & 0x07U))) != 0;
            epaper_draw_pixel(display, px, py, bit_set ? color : bg_color);
        }
    }

    return ESP_OK;
}

esp_err_t epaper_draw_string(epaper_display_t *display,
                             uint16_t x, uint16_t y,
                             const char *text,
                             const epaper_font_t *font,
                             uint8_t color)
{
    if (display == NULL || !display->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (text == NULL || font == NULL || font->table == NULL || font->width == 0 || font->height == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (x >= display->logical_width || y >= display->logical_height) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t cursor_x = x;
    uint16_t cursor_y = y;
    const uint16_t screen_w = display->logical_width;
    const uint16_t screen_h = display->logical_height;

    const char *p = text;
    while (*p != '\0') {
        /* Stop if another full character row does not fit vertically */
        if (cursor_y + font->height > screen_h) {
            break;
        }

        /* Handle explicit newline characters */
        if (*p == '\n') {
            cursor_x = 0;
            cursor_y += font->height;
            p++;
            continue;
        }
        if (*p == '\r') {
            p++;
            continue;
        }

        /* Skip leading space at the beginning of a wrapped line */
        if (*p == ' ' && cursor_x == 0 && p != text) {
            p++;
            continue;
        }

        /* Check if a word starts here and verify whether it fits in the current row */
        if (*p != ' ') {
            size_t word_chars = 0;
            while (p[word_chars] != '\0' && p[word_chars] != ' ' &&
                   p[word_chars] != '\n' && p[word_chars] != '\r') {
                word_chars++;
            }

            uint32_t word_px = (uint32_t)word_chars * font->width;
            /* If the word does not fit on the rest of this line, but fits on a new line, wrap now */
            if (cursor_x > 0 && (cursor_x + word_px > screen_w) && (word_px <= screen_w)) {
                cursor_x = 0;
                cursor_y += font->height;
                if (cursor_y + font->height > screen_h) {
                    break;
                }
            }
        }

        /* Character-level wrap verification if current X cannot fit even a single character */
        if (cursor_x + font->width > screen_w) {
            cursor_x = 0;
            cursor_y += font->height;
            if (cursor_y + font->height > screen_h) {
                break;
            }
            if (*p == ' ') {
                p++;
                continue;
            }
        }

        epaper_draw_char(display, cursor_x, cursor_y, *p, font, color);
        cursor_x += font->width;
        p++;
    }

    return ESP_OK;
}

esp_err_t epaper_display_string(epaper_display_t *display,
                                uint16_t x, uint16_t y,
                                const char *text,
                                const epaper_font_t *font,
                                uint8_t color)
{
    esp_err_t err = epaper_draw_string(display, x, y, text, font, color);
    if (err != ESP_OK) {
        return err;
    }

    return epaper_write_framebuffer(display, display->framebuffer, SSD1680_UPDATE_FULL_MODE_1);
}

esp_err_t epaper_sleep(epaper_display_t *display, epaper_sleep_mode_t sleep_mode)
{
    if (display == NULL || !display->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Section 9.1 Step 6: Deep sleep by Command 0x10 */
    esp_err_t err = ssd1680_send_cmd(display, SSD1680_CMD_DEEP_SLEEP_MODE);
    if (err != ESP_OK) {
        return err;
    }
    err = ssd1680_send_data_byte(display, (uint8_t)sleep_mode);
    if (err != ESP_OK) {
        return err;
    }

    display->is_sleeping = (sleep_mode != SSD1680_SLEEP_MODE_NORMAL);
    return ESP_OK;
}

int float_to_str_1dec(float value, char *buf, size_t buf_size)
{
    if (buf == NULL || buf_size == 0) {
        return FLOAT_TO_STR_ERR_INVALID_ARG;
    }

    /* IEEE 754 bit-level representation */
    union {
        float f;
        uint32_t u;
    } pun = { .f = value };

    const uint32_t u = pun.u;
    const bool sign = (u >> 31) != 0;
    const uint32_t exp = (u >> 23) & 0xFF;
    const uint32_t mant = u & 0x7FFFFF;

    /* Handle IEEE 754 special values (NaN, +/-Inf) */
    if (exp == 0xFF) {
        if (mant != 0) {
            /* NaN */
            if (buf_size < 4) {
                buf[0] = '\0';
                return FLOAT_TO_STR_ERR_BUF_TOO_SMALL;
            }
            buf[0] = 'n';
            buf[1] = 'a';
            buf[2] = 'n';
            buf[3] = '\0';
            return FLOAT_TO_STR_ERR_NAN;
        } else {
            /* +/- Infinity */
            if (sign) {
                if (buf_size < 5) {
                    buf[0] = '\0';
                    return FLOAT_TO_STR_ERR_BUF_TOO_SMALL;
                }
                buf[0] = '-';
                buf[1] = 'i';
                buf[2] = 'n';
                buf[3] = 'f';
                buf[4] = '\0';
            } else {
                if (buf_size < 4) {
                    buf[0] = '\0';
                    return FLOAT_TO_STR_ERR_BUF_TOO_SMALL;
                }
                buf[0] = 'i';
                buf[1] = 'n';
                buf[2] = 'f';
                buf[3] = '\0';
            }
            return FLOAT_TO_STR_ERR_INF;
        }
    }

    double mag = (value < 0.0f) ? -(double)value : (double)value;

    /* Guard against magnitude exceeding uint64_t representation */
    if (mag > 1e17) {
        buf[0] = '\0';
        return FLOAT_TO_STR_ERR_OVERFLOW;
    }

    /* Round half away from zero with an epsilon compensating for binary float literal inaccuracies */
    double scaled = mag * 10.0;
    double eps = scaled * 1e-7;
    uint64_t total_tenths = (uint64_t)(scaled + 0.5 + eps);

    /* Negative zero check: do not emit '-' if rounded value is 0 */
    const bool is_negative = sign && (total_tenths != 0);

    uint64_t whole = total_tenths / 10;
    const uint32_t fraction = (uint32_t)(total_tenths % 10);

    /* Format integer part in reverse order into stack buffer */
    char temp_digits[24];
    size_t num_digits = 0;
    do {
        temp_digits[num_digits++] = (char)('0' + (whole % 10));
        whole /= 10;
    } while (whole > 0);

    /* Required length without '\0': [sign] + num_digits + '.' + fraction */
    const size_t required_len = (is_negative ? 1 : 0) + num_digits + 2;
    if (required_len + 1 > buf_size) {
        buf[0] = '\0';
        return FLOAT_TO_STR_ERR_BUF_TOO_SMALL;
    }

    size_t idx = 0;
    if (is_negative) {
        buf[idx++] = '-';
    }
    while (num_digits > 0) {
        buf[idx++] = temp_digits[--num_digits];
    }
    buf[idx++] = '.';
    buf[idx++] = (char)('0' + fraction);
    buf[idx] = '\0';

    return (int)idx;
}
