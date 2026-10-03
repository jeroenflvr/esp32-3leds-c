#pragma once

#include "esp_err.h"

/**
 * Start the LED control web server (port 80).
 */
esp_err_t web_server_start(void);
