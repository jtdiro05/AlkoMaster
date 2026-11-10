idf_component_register(SRCS "epaper.c" "epaper_spi_esp32.c" "font24.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_driver_spi esp_driver_gpio freertos)
