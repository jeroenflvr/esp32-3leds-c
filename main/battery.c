#include "battery.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "battery";

/* Battery ADC Configuration */
#define BATTERY_ADC_CHANNEL  ADC_CHANNEL_1  /* GPIO1 */
#define BATTERY_ADC_ATTEN    ADC_ATTEN_DB_12
#define BATTERY_ADC_SAMPLES  8
#define BATTERY_ADC_DISCARD  2
#define VOLTAGE_DIVIDER      2.0  /* R1=R2=330k, so Vbat = 2 * Vadc */
#define ADC_CALIBRATION_FACTOR 1.051  /* Multimeter 4.10V, firmware read 4.83V with factor 1.238 */
#define CACHE_VALID_US       (10 * 1000 * 1000)  /* 10 s */

static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t adc_cali_handle = NULL;

static int64_t last_read_us = 0;
static float cached_voltage = 0;
static float cached_percentage = 0;

esp_err_t battery_init(void)
{
    const adc_oneshot_unit_init_cfg_t adc_init_config = {
        .unit_id = ADC_UNIT_1,
    };
    esp_err_t err = adc_oneshot_new_unit(&adc_init_config, &adc_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADC init failed");
        return err;
    }

    const adc_oneshot_chan_cfg_t adc_chan_config = {
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    err = adc_oneshot_config_channel(adc_handle, BATTERY_ADC_CHANNEL, &adc_chan_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADC channel config failed");
        return err;
    }

    const adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    err = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ADC calibration failed, readings will be uncalibrated");
        adc_cali_handle = NULL;
    }

    ESP_LOGI(TAG, "ADC initialized for battery voltage on GPIO1");
    return ESP_OK;
}

/* Piecewise LiPo discharge curve: voltage -> percentage */
static float voltage_to_percentage(float v)
{
    if (v >= 4.2) return 100.0;
    if (v >= 4.0) return 95.0 + (v - 4.0) * 25.0;
    if (v >= 3.9) return 85.0 + (v - 3.9) * 100.0;
    if (v >= 3.8) return 70.0 + (v - 3.8) * 150.0;
    if (v >= 3.7) return 50.0 + (v - 3.7) * 200.0;
    if (v >= 3.6) return 30.0 + (v - 3.6) * 200.0;
    if (v >= 3.5) return 15.0 + (v - 3.5) * 150.0;
    if (v >= 3.3) return 5.0 + (v - 3.3) * 50.0;
    if (v >= 3.2) return (v - 3.2) * 50.0;
    return 0.0;
}

esp_err_t battery_read(float *voltage, float *percentage)
{
    if (adc_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const int64_t now = esp_timer_get_time();
    if (last_read_us == 0 || (now - last_read_us) > CACHE_VALID_US) {
        int sum = 0;
        int count = 0;
        for (int i = 0; i < BATTERY_ADC_SAMPLES; i++) {
            int raw;
            if (adc_oneshot_read(adc_handle, BATTERY_ADC_CHANNEL, &raw) == ESP_OK &&
                i >= BATTERY_ADC_DISCARD) {
                sum += raw;
                count++;
            }
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        if (count == 0) {
            return ESP_FAIL;
        }
        const int adc_raw_avg = sum / count;

        int voltage_mv;
        if (adc_cali_handle) {
            adc_cali_raw_to_voltage(adc_cali_handle, adc_raw_avg, &voltage_mv);
            voltage_mv = (int)(voltage_mv * ADC_CALIBRATION_FACTOR);
        } else {
            voltage_mv = (adc_raw_avg * 3100) / 4095;
        }

        cached_voltage = (voltage_mv * VOLTAGE_DIVIDER) / 1000.0;
        cached_percentage = voltage_to_percentage(cached_voltage);
        last_read_us = now;

        ESP_LOGI(TAG, "Battery: %.2fV (%.0f%%)", cached_voltage, cached_percentage);
    }

    if (voltage) *voltage = cached_voltage;
    if (percentage) *percentage = cached_percentage;
    return ESP_OK;
}
