idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_wifi wpa_supplicant nvs_flash wifi mq3 mq135 bme280 epaper)
