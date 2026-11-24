idf_component_register(SRCS "esp32_i2c_adapter.c" "esp32_adc_adapter.c" "esp32_gpio_adapter.c" "esp32_dht_adapter.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_driver_i2c esp_adc esp_driver_gpio freertos log)
