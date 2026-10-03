#include "config.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "config";

/* NVS Configuration Keys */
static const char NVS_NAMESPACE[] = "led_cfg";
static const char NVS_KEY_PROVISIONED[] = "provisioned";
static const char NVS_KEY_WIFI_SSID[] = "wifi_ssid";
static const char NVS_KEY_WIFI_PASS[] = "wifi_pass";

config_t g_config;

void config_init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");
}

bool config_load_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS namespace not found, device not provisioned");
        return false;
    }

    /* Check if provisioned */
    uint8_t provisioned = 0;
    err = nvs_get_u8(nvs_handle, NVS_KEY_PROVISIONED, &provisioned);
    if (err != ESP_OK || provisioned == 0) {
        ESP_LOGW(TAG, "Device not provisioned");
        nvs_close(nvs_handle);
        return false;
    }

    g_config.provisioned = true;

    size_t len;

    len = sizeof(g_config.wifi_ssid);
    nvs_get_str(nvs_handle, NVS_KEY_WIFI_SSID, g_config.wifi_ssid, &len);

    len = sizeof(g_config.wifi_password);
    nvs_get_str(nvs_handle, NVS_KEY_WIFI_PASS, g_config.wifi_password, &len);

    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "Configuration loaded from NVS");
    ESP_LOGI(TAG, "  WiFi SSID: %s", g_config.wifi_ssid);

    return true;
}

esp_err_t config_save_to_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing");
        return err;
    }

    nvs_set_u8(nvs_handle, NVS_KEY_PROVISIONED, 1);
    nvs_set_str(nvs_handle, NVS_KEY_WIFI_SSID, g_config.wifi_ssid);
    nvs_set_str(nvs_handle, NVS_KEY_WIFI_PASS, g_config.wifi_password);

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Configuration saved to NVS");
    } else {
        ESP_LOGE(TAG, "Failed to commit NVS");
    }

    return err;
}

bool config_load_from_env(void)
{
    ESP_LOGI(TAG, "Checking for .env file...");

    /* Mount SPIFFS */
    const esp_vfs_spiffs_conf_t spiffs_conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = false
    };

    const esp_err_t ret = esp_vfs_spiffs_register(&spiffs_conf);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(ret));
        return false;
    }

    /* Try to open .env file */
    FILE *const f = fopen("/spiffs/.env", "r");
    if (f == NULL) {
        ESP_LOGI(TAG, ".env file not found, will use NVS or provisioning");
        esp_vfs_spiffs_unregister("storage");
        return false;
    }

    ESP_LOGI(TAG, "Loading configuration from .env file");

    /* Parse .env file */
    char line[256];
    bool has_wifi_ssid = false;
    bool has_wifi_pass = false;

    while (fgets(line, sizeof(line), f) != NULL) {
        /* Remove newline */
        line[strcspn(line, "\r\n")] = 0;

        /* Skip empty lines and comments */
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        /* Parse KEY=VALUE */
        char *equals = strchr(line, '=');
        if (equals == NULL) {
            continue;
        }

        *equals = '\0';
        char *key = line;
        char *value = equals + 1;

        /* Remove quotes if present */
        if (value[0] == '"' || value[0] == '\'') {
            value++;
            size_t len = strlen(value);
            if (len > 0 && (value[len-1] == '"' || value[len-1] == '\'')) {
                value[len-1] = '\0';
            }
        }

        if (strcmp(key, "WIFI_SSID") == 0) {
            strncpy(g_config.wifi_ssid, value, sizeof(g_config.wifi_ssid) - 1);
            has_wifi_ssid = true;
        } else if (strcmp(key, "WIFI_PASSWORD") == 0) {
            strncpy(g_config.wifi_password, value, sizeof(g_config.wifi_password) - 1);
            has_wifi_pass = true;
        }
    }

    fclose(f);
    esp_vfs_spiffs_unregister("storage");

    const bool config_complete = has_wifi_ssid && has_wifi_pass;

    if (config_complete) {
        g_config.provisioned = true;
        ESP_LOGI(TAG, "Configuration loaded from .env file");
        ESP_LOGI(TAG, "  WiFi SSID: %s", g_config.wifi_ssid);
    } else {
        ESP_LOGW(TAG, ".env file incomplete (need WIFI_SSID and WIFI_PASSWORD)");
    }

    return config_complete;
}
