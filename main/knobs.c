#include "include/knobs.h"
#include "include/stm32_uart.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "KNOBS"
#define KNOB_COUNT 6
#define KNOB_MACRO 5                // index of the macro knob
#define KNOB_SAMPLES 16             // raw readings averaged per knob
#define KNOB_HYST 41                // ~1% of 0..4095, measured ADC noise at rest is ~5
#define KNOB_END 40                 // raw <= 40 or >= 4095 - 40 counts as the end of the knob
#define KNOB_GAP_MS 100             // max one SET per knob in this time (loose wire floods the STM32)
#define MACRO_COUNT 10

// ADC1 on ESP32: GPIO36 = CH0, GPIO39 = CH3, GPIO34 = CH6, GPIO35 = CH7, GPIO32 = CH4, GPIO33 = CH5 (macro)
static const adc_channel_t knob_ch[KNOB_COUNT] = { ADC_CHANNEL_0, ADC_CHANNEL_3, ADC_CHANNEL_6, ADC_CHANNEL_7, ADC_CHANNEL_4, ADC_CHANNEL_5 };
static const char *knob_param[KNOB_COUNT] = { "volume", "drive", "eq_low_db", "eq_mid_db", "eq_high_db", "macro" };
static const bool knob_on[KNOB_COUNT] = { true, true, true, true, true, true };   // unconnected pin reads 0, keep it off

typedef struct {
    const char *name;
    float min;
    float max;
    bool log;           // true = logarithmic scale
} macro_param_t;

static const macro_param_t macro_params[MACRO_COUNT] = {
    { "reverb_mix",     0.0f,   1.0f,     false },
    { "reverb_decay",   0.0f,   1.0f,     false },
    { "echo_mix",       0.0f,   1.0f,     false },
    { "echo_time_ms",   20.0f,  1000.0f,  true },
    { "echo_feedback",  0.0f,   0.95f,    false },
    { "chorus_mix",     0.0f,   1.0f,     false },
    { "chorus_depth",   0.0f,   1.0f,     false },
    { "chorus_rate_hz", 0.05f,  5.0f,     true },
    { "clip_level",     100.0f, 30000.0f, true },
    { "clean_gain",     0.0f,   64.0f,    false },
};

static adc_oneshot_unit_handle_t adc1;
static int knob_last[KNOB_COUNT];           // raw value of the last SET, -1 = send on next read
static TickType_t knob_tick[KNOB_COUNT];    // when the last SET was sent
static volatile bool resync = true;
static volatile int macro = -1;             // index in macro_params, -1 = off
static volatile bool macro_changed = false;

static int macro_find(const char *name) // index in macro_params, -1 = not in the table
{
    for (int m = 0; m < MACRO_COUNT; m++)
        if (strcmp(name, macro_params[m].name) == 0) return m;
    return -1;
}

static void macro_load(void) // source: Github esp-idf/examples/storage/nvs/nvs_rw_value
{
    nvs_handle_t nvs;
    char name[24];
    size_t len = sizeof(name);
    esp_err_t err = nvs_open("knobs", NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        err = nvs_get_str(nvs, "macro", name, &len);
        nvs_close(nvs);
    }
    if (err == ESP_OK) macro = macro_find(name);
    else if (err != ESP_ERR_NVS_NOT_FOUND) ESP_LOGW(TAG, "Macro not loaded (%s)", esp_err_to_name(err));
    ESP_LOGI(TAG, "Macro knob: %s", knobs_get_macro());
}

static void macro_save(const char *name) // source: Github esp-idf/examples/storage/nvs/nvs_rw_value
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("knobs", NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        err = nvs_set_str(nvs, "macro", name);
        if (err == ESP_OK) err = nvs_commit(nvs);
        nvs_close(nvs);
    }
    if (err != ESP_OK) ESP_LOGW(TAG, "Macro not saved (%s)", esp_err_to_name(err));
}

void knobs_init(void) // source: Github esp-idf/examples/peripherals/adc/oneshot_read
{
    adc_oneshot_unit_init_cfg_t init_config = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1));
    adc_oneshot_chan_cfg_t config = { .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT };
    for (int i = 0; i < KNOB_COUNT; i++)
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1, knob_ch[i], &config));
    macro_load();
}

void knobs_resync(void) // STM32 restarted or link came back -> send all knob positions again
{
    resync = true;
}

bool knobs_set_macro(const char *name) // parameter for the macro knob or "off", kept in NVS
{
    int m = macro_find(name);
    if (m < 0 && strcmp(name, "off") != 0) return false;
    macro = m;
    macro_changed = true;
    macro_save(name);
    return true;
}

const char *knobs_get_macro(void)
{
    int m = macro;
    return m < 0 ? "off" : macro_params[m].name;
}

static int knob_read(int i)     // average of KNOB_SAMPLES readings, 0..4095
{
    int sum = 0, raw = 0;
    for (int s = 0; s < KNOB_SAMPLES; s++) {
        adc_oneshot_read(adc1, knob_ch[i], &raw);
        sum += raw;
    }
    return sum / KNOB_SAMPLES;
}

static float knob_pos(int raw) // raw 0..4095 -> 0..1
{
    float x = (float)(raw - KNOB_END) / (4095 - 2 * KNOB_END);
    if (x < 0.0f) x = 0.0f;
    if (x > 1.0f) x = 1.0f;
    return x;
}

static float knob_value(int i, int raw) // raw 0..4095 -> parameter value
{
    float x = knob_pos(raw);
    if (i == 0) return x * x;                       // volume, audio taper
    if (i == 1) return powf(200.0f, x);             // drive 1..200, exponential
    float db = -12.0f + 24.0f * x;                  // eq, 0 dB zone in the middle (no centre click)
    if (fabsf(db) < 0.75f) db = 0.0f;
    return db;
}

static float macro_value(int m, int raw) // raw 0..4095 -> value of the parameter picked for the macro knob
{
    const macro_param_t *p = &macro_params[m];
    float x = knob_pos(raw);
    if (p->log) return p->min * powf(p->max / p->min, x);
    return p->min + (p->max - p->min) * x;
}

void knobs_task(void *pvParameters) // sends SET only when a knob really moved
{
    char line[STM32_LINE_MAX];
    while(1)
    {
        if (resync) {
            for (int i = 0; i < KNOB_COUNT; i++) knob_last[i] = -1;
            resync = false;
        }
        if (macro_changed) {
            knob_last[KNOB_MACRO] = -1;
            macro_changed = false;
        }
        int m = macro;
        for (int i = 0; i < KNOB_COUNT; i++)
        {
            bool on = (i == KNOB_MACRO) ? m >= 0 : knob_on[i];
            if (!on || !stm32_online()) continue;
            int raw = knob_read(i);
            bool moved = knob_last[i] < 0 || abs(raw - knob_last[i]) >= KNOB_HYST;
            bool at_end = (raw <= KNOB_END && knob_last[i] > KNOB_END) ||
                          (raw >= 4095 - KNOB_END && knob_last[i] < 4095 - KNOB_END);
            if (!moved && !at_end) continue;
            if (xTaskGetTickCount() - knob_tick[i] < pdMS_TO_TICKS(KNOB_GAP_MS)) continue;  // too soon, knob_last[] kept so it is sent later
            knob_last[i] = raw;
            knob_tick[i] = xTaskGetTickCount();
            const char *name = (i == KNOB_MACRO) ? macro_params[m].name : knob_param[i];
            float value = (i == KNOB_MACRO) ? macro_value(m, raw) : knob_value(i, raw);
            snprintf(line, sizeof(line), "SET %s %.3f", name, value);
            stm32_send(line);
            ESP_LOGD(TAG, "%s raw %d", name, raw);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
