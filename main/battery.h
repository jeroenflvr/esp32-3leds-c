#pragma once

#include "esp_err.h"

/**
 * Initialize ADC for battery voltage measurement (GPIO1, divider /2)
 */
esp_err_t battery_init(void);

/**
 * Read battery voltage and percentage. Results are cached for a few
 * seconds to keep the web API cheap.
 * @param voltage Battery voltage in V (may be NULL)
 * @param percentage Battery charge 0-100% (may be NULL)
 */
esp_err_t battery_read(float *voltage, float *percentage);
