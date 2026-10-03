#include "led_patterns.h"
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "nvs.h"
#include "led_strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "led_patterns";

#define LED_GPIO        0
#define LED_COUNT       3
#define TICK_MS         20

/* NVS persistence */
#define NVS_NAMESPACE   "led_state"
#define SAVE_DEBOUNCE_US (2 * 1000 * 1000)

static led_strip_handle_t strip = NULL;
static SemaphoreHandle_t state_mutex = NULL;
static esp_timer_handle_t save_timer = NULL;

static led_state_t current_state = {
    .pattern = PATTERN_CANDLE,
    .brightness = 128,
    .r = 255, .g = 40, .b = 40,
    .bpm = 60,
};

static const char *pattern_names[PATTERN_MAX] = {
    [PATTERN_OFF] = "off",
    [PATTERN_SOLID] = "solid",
    [PATTERN_CANDLE] = "candle",
    [PATTERN_LIGHTNING] = "lightning",
    [PATTERN_HEARTBEAT] = "heartbeat",
};

const char *led_pattern_name(led_pattern_t pattern)
{
    if (pattern >= PATTERN_MAX) return NULL;
    return pattern_names[pattern];
}

led_pattern_t led_pattern_from_name(const char *name)
{
    for (int i = 0; i < PATTERN_MAX; i++) {
        if (strcmp(name, pattern_names[i]) == 0) {
            return (led_pattern_t)i;
        }
    }
    return PATTERN_MAX;
}

/* ---------- helpers ---------- */

static uint32_t rand_range(uint32_t min, uint32_t max)
{
    return min + (esp_random() % (max - min + 1));
}

static float rand_float(void)
{
    return (float)esp_random() / (float)UINT32_MAX;
}

/* Set one pixel with global brightness applied */
static void set_pixel(int i, float r, float g, float b, uint8_t brightness)
{
    const float scale = brightness / 255.0f;
    led_strip_set_pixel(strip, i,
                        (uint32_t)(r * scale),
                        (uint32_t)(g * scale),
                        (uint32_t)(b * scale));
}

/* ---------- candle ---------- */

typedef struct {
    float level;
    float target;
    int hold_ticks;
} candle_led_t;

static candle_led_t candle[LED_COUNT];

static void render_candle(const led_state_t *s)
{
    for (int i = 0; i < LED_COUNT; i++) {
        candle_led_t *c = &candle[i];
        if (--c->hold_ticks <= 0) {
            /* occasional deep dip makes the flame "dance" */
            if (rand_float() < 0.15f) {
                c->target = 0.15f + rand_float() * 0.25f;
            } else {
                c->target = 0.55f + rand_float() * 0.45f;
            }
            c->hold_ticks = rand_range(2, 12);
        }
        c->level += (c->target - c->level) * 0.25f;

        /* warm flame; green drops faster than red when dim -> redder when low */
        const float lvl = c->level;
        set_pixel(i, 255.0f * lvl, 140.0f * lvl * lvl, 12.0f * lvl * lvl, s->brightness);
    }
}

/* ---------- lightning ---------- */

typedef enum { STORM_IDLE, STORM_FLASH_ON, STORM_FLASH_OFF } storm_phase_t;

static struct {
    storm_phase_t phase;
    int ticks_left;
    int flashes_left;
    uint32_t flash_mask;
    float flash_intensity;
    float glow;             /* afterglow decay */
} storm;

static void render_lightning(const led_state_t *s)
{
    if (--storm.ticks_left <= 0) {
        switch (storm.phase) {
        case STORM_IDLE:
            storm.flashes_left = rand_range(1, 4);
            /* fall through to first flash */
        case STORM_FLASH_OFF:
            if (storm.flashes_left > 0) {
                storm.flashes_left--;
                storm.phase = STORM_FLASH_ON;
                storm.ticks_left = rand_range(1, 4);          /* 20-80 ms flash */
                storm.flash_mask = rand_range(1, (1 << LED_COUNT) - 1);
                storm.flash_intensity = 0.6f + rand_float() * 0.4f;
            } else {
                storm.phase = STORM_IDLE;
                storm.ticks_left = rand_range(40, 400);       /* 0.8-8 s calm */
            }
            break;
        case STORM_FLASH_ON:
            storm.phase = STORM_FLASH_OFF;
            storm.ticks_left = rand_range(2, 8);              /* 40-160 ms gap */
            storm.glow = storm.flash_intensity * 0.5f;        /* afterglow */
            break;
        }
    }

    storm.glow *= 0.85f;

    for (int i = 0; i < LED_COUNT; i++) {
        if (storm.phase == STORM_FLASH_ON && (storm.flash_mask & (1 << i))) {
            const float f = storm.flash_intensity;
            set_pixel(i, 255.0f * f, 255.0f * f, 255.0f * f, s->brightness);
        } else {
            /* dim stormy-night blue base + fading afterglow */
            const float g = storm.glow;
            set_pixel(i, 3.0f + 200.0f * g, 4.0f + 200.0f * g, 16.0f + 220.0f * g,
                      s->brightness);
        }
    }
}

/* ---------- heartbeat ---------- */

static uint32_t heartbeat_t_ms = 0;

static float gauss(float t, float center, float sigma)
{
    const float d = (t - center) / sigma;
    return expf(-d * d);
}

static void render_heartbeat(const led_state_t *s)
{
    const uint32_t period_ms = 60000 / (s->bpm ? s->bpm : 60);
    heartbeat_t_ms += TICK_MS;
    if (heartbeat_t_ms >= period_ms) {
        heartbeat_t_ms -= period_ms;
    }

    /* compress the lub-dub if the period gets short */
    const float squeeze = period_ms < 600 ? period_ms / 600.0f : 1.0f;
    const float t = (float)heartbeat_t_ms;

    /* lub (strong) then dub (weaker), like S1/S2 heart sounds */
    float env = gauss(t, 80.0f * squeeze, 45.0f * squeeze)
              + 0.55f * gauss(t, 320.0f * squeeze, 55.0f * squeeze)
              + 0.03f;
    if (env > 1.0f) env = 1.0f;
    env = env * env;  /* gamma for a punchier pulse */

    for (int i = 0; i < LED_COUNT; i++) {
        set_pixel(i, s->r * env, s->g * env, s->b * env, s->brightness);
    }
}

/* ---------- solid / off ---------- */

static void render_solid(const led_state_t *s)
{
    for (int i = 0; i < LED_COUNT; i++) {
        set_pixel(i, s->r, s->g, s->b, s->brightness);
    }
}

static void render_off(void)
{
    for (int i = 0; i < LED_COUNT; i++) {
        led_strip_set_pixel(strip, i, 0, 0, 0);
    }
}

/* ---------- persistence ---------- */

static void save_timer_cb(void *arg)
{
    led_state_t s;
    led_patterns_get(&s);

    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS to save LED state");
        return;
    }
    nvs_set_u8(nvs, "pattern", (uint8_t)s.pattern);
    nvs_set_u8(nvs, "bright", s.brightness);
    nvs_set_u8(nvs, "r", s.r);
    nvs_set_u8(nvs, "g", s.g);
    nvs_set_u8(nvs, "b", s.b);
    nvs_set_u16(nvs, "bpm", s.bpm);
    nvs_commit(nvs);
    nvs_close(nvs);
    ESP_LOGI(TAG, "LED state saved (pattern=%s)", led_pattern_name(s.pattern));
}

static void load_state(void)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return;
    }
    uint8_t u8;
    uint16_t u16;
    if (nvs_get_u8(nvs, "pattern", &u8) == ESP_OK && u8 < PATTERN_MAX) {
        current_state.pattern = (led_pattern_t)u8;
    }
    if (nvs_get_u8(nvs, "bright", &u8) == ESP_OK) current_state.brightness = u8;
    if (nvs_get_u8(nvs, "r", &u8) == ESP_OK) current_state.r = u8;
    if (nvs_get_u8(nvs, "g", &u8) == ESP_OK) current_state.g = u8;
    if (nvs_get_u8(nvs, "b", &u8) == ESP_OK) current_state.b = u8;
    if (nvs_get_u16(nvs, "bpm", &u16) == ESP_OK && u16 >= 30 && u16 <= 180) {
        current_state.bpm = u16;
    }
    nvs_close(nvs);
    ESP_LOGI(TAG, "LED state restored (pattern=%s, brightness=%d)",
             led_pattern_name(current_state.pattern), current_state.brightness);
}

/* ---------- pattern task ---------- */

static void pattern_task(void *arg)
{
    while (1) {
        led_state_t s;
        led_patterns_get(&s);

        switch (s.pattern) {
        case PATTERN_SOLID:     render_solid(&s); break;
        case PATTERN_CANDLE:    render_candle(&s); break;
        case PATTERN_LIGHTNING: render_lightning(&s); break;
        case PATTERN_HEARTBEAT: render_heartbeat(&s); break;
        default:                render_off(); break;
        }
        led_strip_refresh(strip);

        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
    }
}

/* ---------- public API ---------- */

esp_err_t led_patterns_init(void)
{
    const led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = LED_COUNT,
        .led_model = LED_MODEL_WS2812,  /* WS2813 is protocol-compatible */
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };
    const led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
    };
    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED strip: %s", esp_err_to_name(err));
        return err;
    }
    led_strip_clear(strip);

    state_mutex = xSemaphoreCreateMutex();

    const esp_timer_create_args_t timer_args = {
        .callback = save_timer_cb,
        .name = "led_save",
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &save_timer));

    load_state();

    xTaskCreate(pattern_task, "led_pattern", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "LED patterns initialized (%d LEDs on GPIO%d)", LED_COUNT, LED_GPIO);
    return ESP_OK;
}

void led_patterns_set(const led_state_t *state)
{
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    current_state = *state;
    if (current_state.pattern >= PATTERN_MAX) current_state.pattern = PATTERN_OFF;
    if (current_state.bpm < 30) current_state.bpm = 30;
    if (current_state.bpm > 180) current_state.bpm = 180;
    xSemaphoreGive(state_mutex);

    /* debounce NVS writes while the user drags a slider */
    esp_timer_stop(save_timer);
    esp_timer_start_once(save_timer, SAVE_DEBOUNCE_US);
}

void led_patterns_get(led_state_t *state)
{
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    *state = current_state;
    xSemaphoreGive(state_mutex);
}
