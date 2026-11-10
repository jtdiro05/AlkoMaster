#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Bitmap font descriptor structure
 */
typedef struct {
    const uint8_t *table;   /**< Pointer to the glyph bitmap array */
    uint8_t width;          /**< Character width in pixels (16 for Font24) */
    uint8_t height;         /**< Character height in pixels (24 for Font24) */
    uint8_t bytes_per_row;  /**< Number of bytes per glyph row ((width + 7) / 8) */
    uint8_t bytes_per_char; /**< Total bytes per character (bytes_per_row * height) */
    char first_char;        /**< First supported ASCII character (0x20 ' ') */
    char last_char;         /**< Last supported ASCII character (0x7E '~') */
} epaper_font_t;

/** 16x24 monospace bitmap font (ASCII 0x20..0x7E, 48 bytes per character) */
extern const epaper_font_t epaper_font24;

/** Raw lookup table for the 16x24 bitmap font */
extern const uint8_t epaper_font24_table[];

#ifdef __cplusplus
}
#endif
