#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the WiFi module
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_init(void);

#ifdef __cplusplus
}
#endif
