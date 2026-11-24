idf_component_register(SRCS "bme280_check.c" "main.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_wifi wpa_supplicant nvs_flash wifi esp32_adapters button dht22 mq3 mq135 bme280 epaper)
