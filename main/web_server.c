#include "web_server.h"
#include "led_patterns.h"
#include "battery.h"
#include <string.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"

static const char *TAG = "web_server";

static httpd_handle_t server = NULL;

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)index_html_start,
                    index_html_end - index_html_start);
    return ESP_OK;
}

static esp_err_t favicon_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t state_get_handler(httpd_req_t *req)
{
    led_state_t s;
    led_patterns_get(&s);

    float voltage = 0, percentage = 0;
    battery_read(&voltage, &percentage);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "pattern", led_pattern_name(s.pattern));
    cJSON_AddNumberToObject(root, "brightness", s.brightness);
    cJSON_AddNumberToObject(root, "r", s.r);
    cJSON_AddNumberToObject(root, "g", s.g);
    cJSON_AddNumberToObject(root, "b", s.b);
    cJSON_AddNumberToObject(root, "bpm", s.bpm);
    cJSON_AddNumberToObject(root, "battery_v", voltage);
    cJSON_AddNumberToObject(root, "battery_pct", percentage);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    free(json);
    return ESP_OK;
}

static uint8_t clamp_u8(double v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

static esp_err_t state_post_handler(httpd_req_t *req)
{
    char buf[256];
    const int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }
    buf[len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    led_state_t s;
    led_patterns_get(&s);

    const cJSON *item;
    if ((item = cJSON_GetObjectItem(root, "pattern")) && cJSON_IsString(item)) {
        const led_pattern_t p = led_pattern_from_name(item->valuestring);
        if (p == PATTERN_MAX) {
            cJSON_Delete(root);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Unknown pattern");
            return ESP_FAIL;
        }
        s.pattern = p;
    }
    if ((item = cJSON_GetObjectItem(root, "brightness")) && cJSON_IsNumber(item)) {
        s.brightness = clamp_u8(item->valuedouble);
    }
    if ((item = cJSON_GetObjectItem(root, "r")) && cJSON_IsNumber(item)) {
        s.r = clamp_u8(item->valuedouble);
    }
    if ((item = cJSON_GetObjectItem(root, "g")) && cJSON_IsNumber(item)) {
        s.g = clamp_u8(item->valuedouble);
    }
    if ((item = cJSON_GetObjectItem(root, "b")) && cJSON_IsNumber(item)) {
        s.b = clamp_u8(item->valuedouble);
    }
    if ((item = cJSON_GetObjectItem(root, "bpm")) && cJSON_IsNumber(item)) {
        s.bpm = (uint16_t)item->valuedouble;
    }
    cJSON_Delete(root);

    led_patterns_set(&s);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t web_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.lru_purge_enable = true;

    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ESP_FAIL;
    }

    const httpd_uri_t uri_index = {
        .uri = "/", .method = HTTP_GET, .handler = index_handler,
    };
    const httpd_uri_t uri_favicon = {
        .uri = "/favicon.ico", .method = HTTP_GET, .handler = favicon_handler,
    };
    const httpd_uri_t uri_state_get = {
        .uri = "/api/state", .method = HTTP_GET, .handler = state_get_handler,
    };
    const httpd_uri_t uri_state_post = {
        .uri = "/api/state", .method = HTTP_POST, .handler = state_post_handler,
    };
    httpd_register_uri_handler(server, &uri_index);
    httpd_register_uri_handler(server, &uri_favicon);
    httpd_register_uri_handler(server, &uri_state_get);
    httpd_register_uri_handler(server, &uri_state_post);

    ESP_LOGI(TAG, "Web server started on port 80");
    return ESP_OK;
}
