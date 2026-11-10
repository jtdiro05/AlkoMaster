#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "epaper.h"
#include "epaper_spi_adapter.h"

static const char *TAG = "main";

/* Default ESP32 SPI and control pin assignment for SSD1680 e-Paper */
#define EPAPER_SPI_HOST     SPI2_HOST
#define EPAPER_PIN_MOSI     GPIO_NUM_11
#define EPAPER_PIN_SCLK     GPIO_NUM_12
#define EPAPER_PIN_CS       GPIO_NUM_10
#define EPAPER_PIN_DC       GPIO_NUM_18
#define EPAPER_PIN_RST      GPIO_NUM_8
#define EPAPER_PIN_BUSY     GPIO_NUM_3

/* 2.9" SSD1680 panel resolution (128x296) */
#define EPAPER_PANEL_WIDTH  EPAPER_2IN9_WIDTH
#define EPAPER_PANEL_HEIGHT EPAPER_2IN9_HEIGHT

void app_main(void)
{
    static epaper_esp32_spi_ctx_t spi_ctx;
    static epaper_spi_adapter_t spi_adapter;
    static epaper_display_t display;

    /* Configure ESP32 SPI Hardware Adapter */
    const epaper_esp32_spi_config_t spi_cfg = {
        .spi_host = EPAPER_SPI_HOST,
        .mosi_pin = EPAPER_PIN_MOSI,
        .sclk_pin = EPAPER_PIN_SCLK,
        .cs_pin = EPAPER_PIN_CS,
        .dc_pin = EPAPER_PIN_DC,
        .rst_pin = EPAPER_PIN_RST,
        .busy_pin = EPAPER_PIN_BUSY,
        .clock_speed_hz = SSD1680_SPI_DEFAULT_CLOCK_HZ,
        .max_transfer_sz = 4096,
        .init_spi_bus = true,
    };

    ESP_ERROR_CHECK(epaper_spi_esp32_create_adapter(&spi_ctx, &spi_cfg, &spi_adapter));

    /* Configure monochrome B/W SSD1680 panel */
    const epaper_panel_config_t panel_cfg = {
        .width = EPAPER_PANEL_WIDTH,
        .height = EPAPER_PANEL_HEIGHT,
        .data_entry_mode = SSD1680_DATA_ENTRY_X_INC_Y_INC,
        .border_waveform = 0x05,
        .busy_timeout_ms = 5000,
    };

    ESP_LOGI(TAG, "Initializing SSD1680 e-Paper display...");
    ESP_ERROR_CHECK(epaper_init(&display, &panel_cfg, &spi_adapter));

    bool fill_white = true;

    epaper_draw_string(&display, 0, 0, "Taki swiety jestes?", &epaper_font24, EPAPER_COLOR_BLACK);
    epaper_draw_string(&display, 0, 64, "To chuchnij", &epaper_font24, EPAPER_COLOR_BLACK);
    epaper_write_framebuffer(&display, display.framebuffer, SSD1680_UPDATE_FULL_MODE_1);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
