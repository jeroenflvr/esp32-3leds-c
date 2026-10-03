#pragma once

#include <stdbool.h>
#include "esp_err.h"

// Configuration structure
typedef struct {
    bool provisioned;
    char wifi_ssid[32];
    char wifi_password[64];
} config_t;

// Global configuration
extern config_t g_config;

/**
 * Initialize NVS flash
 */
void config_init_nvs(void);

/**
 * Load configuration from .env file in SPIFFS
 * @return true if configuration loaded successfully
 */
bool config_load_from_env(void);

/**
 * Load configuration from NVS
 * @return true if configuration loaded successfully
 */
bool config_load_from_nvs(void);

/**
 * Save configuration to NVS
 * @return ESP_OK on success
 */
esp_err_t config_save_to_nvs(void);
