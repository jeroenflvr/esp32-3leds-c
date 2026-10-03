#pragma once

#include <stdint.h>
#include "esp_err.h"

typedef enum {
    PATTERN_OFF = 0,
    PATTERN_SOLID,
    PATTERN_CANDLE,
    PATTERN_LIGHTNING,
    PATTERN_HEARTBEAT,
    PATTERN_MAX
} led_pattern_t;

typedef struct {
    led_pattern_t pattern;
    uint8_t brightness;   /* global brightness 0-255, applies to every pattern */
    uint8_t r, g, b;      /* color for SOLID and HEARTBEAT */
    uint16_t bpm;         /* heartbeat rate, 30-180 */
    uint8_t orange[3];    /* per-LED candle orangeness 0-255 (0=yellow, 255=deep orange) */
    uint8_t nervous[3];   /* per-LED candle nervousness 0-255 (0=steady, 255=jittery) */
} led_state_t;

/**
 * Initialize LED strip, load last state from NVS and start the pattern task.
 */
esp_err_t led_patterns_init(void);

/**
 * Apply a new state (thread-safe) and persist it to NVS (debounced).
 */
void led_patterns_set(const led_state_t *state);

/**
 * Get the current state (thread-safe).
 */
void led_patterns_get(led_state_t *state);

/** Pattern name for the API, e.g. "candle". NULL if out of range. */
const char *led_pattern_name(led_pattern_t pattern);

/** Parse a pattern name; returns PATTERN_MAX if unknown. */
led_pattern_t led_pattern_from_name(const char *name);
