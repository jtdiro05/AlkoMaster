#include "wifi.h"
#include "esp_log.h"

static const char *TAG = "wifi";

esp_err_t wifi_init(void)
{
    ESP_LOGI(TAG, "Initializing WiFi module...");
    return ESP_OK;
}
