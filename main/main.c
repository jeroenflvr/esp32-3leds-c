/* 3-LED Controller (Seeed XIAO ESP32-C6)
 *
 * Features:
 * - 3x WS2813 LEDs on GPIO0, driven via RMT
 * - Web UI to pick patterns: solid color, candle, lightning, heartbeat
 * - Global brightness control, independent of pattern
 * - Battery voltage/percentage monitoring (GPIO1 voltage divider)
 * - State persisted to NVS, restored on boot
 * - Web-based WiFi provisioning for first-time setup
 *
 * Boot sequence:
 * 1. Load WiFi config from .env (SPIFFS) or NVS
 *    - If not provisioned: start AP mode + setup web server
 * 2. Start LED pattern engine with last saved state
 * 3. Connect to WiFi
 * 4. Start control web server
 */

#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "config.h"
#include "wifi_manager.h"
#include "battery.h"
#include "led_patterns.h"
#include "web_server.h"
#include "provisioning.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, "3-LED Controller Starting");
    ESP_LOGI(TAG, "====================================");

    // Initialize NVS
    config_init_nvs();

    // Load configuration
    memset(&g_config, 0, sizeof(g_config));

    // First try to load from .env file (development mode)
    bool is_provisioned = config_load_from_env();

    // If .env not found or incomplete, try NVS
    if (!is_provisioned) {
        is_provisioned = config_load_from_nvs();
    }

    if (!is_provisioned) {
        ESP_LOGW(TAG, "Device not provisioned - entering setup mode");
        provisioning_start();
        /* Never returns */
    }

    /* Start LEDs right away with the last saved state */
    ESP_ERROR_CHECK(led_patterns_init());

    if (battery_init() != ESP_OK) {
        ESP_LOGW(TAG, "Battery monitoring unavailable");
    }

    /* Connect to WiFi; LEDs keep running even if this fails */
    if (wifi_connect() != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connection failed, restarting in 30 seconds");
        vTaskDelay(pdMS_TO_TICKS(30000));
        esp_restart();
    }

    ESP_ERROR_CHECK(web_server_start());

    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, "Ready! Open http://%s", wifi_get_ip());
    ESP_LOGI(TAG, "====================================");
}
