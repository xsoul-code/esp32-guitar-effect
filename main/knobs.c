#include "include/knobs.h"
#include "include/stm32_uart.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "KNOBS"
#define KNOB_COUNT 5
#define KNOB_SAMPLES 16             // raw readings averaged per knob
#define KNOB_HYST 41                // ~1% of 0..4095, measured ADC noise at rest is ~5
#define KNOB_END 40                 // raw <= 40 or >= 4095 - 40 counts as the end of the knob

// ADC1 on ESP32: GPIO36 = CH0, GPIO39 = CH3, GPIO34 = CH6, GPIO35 = CH7, GPIO32 = CH4
static const adc_channel_t knob_ch[KNOB_COUNT] = { ADC_CHANNEL_0, ADC_CHANNEL_3, ADC_CHANNEL_6, ADC_CHANNEL_7, ADC_CHANNEL_4 };
static const char *knob_param[KNOB_COUNT] = { "volume", "drive", "eq_low_db", "eq_mid_db", "eq_high_db" };
static const bool knob_on[KNOB_COUNT] = { true, false, false, false, false };   // unconnected pin reads 0, keep it off

static adc_oneshot_unit_handle_t adc1;
static int knob_last[KNOB_COUNT];           // raw value of the last SET, -1 = send on next read
static volatile bool resync = true;

void knobs_init(void) // source: Github esp-idf/examples/peripherals/adc/oneshot_read
{
    adc_oneshot_unit_init_cfg_t init_config = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1));
    adc_oneshot_chan_cfg_t config = { .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT };
    for (int i = 0; i < KNOB_COUNT; i++)
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1, knob_ch[i], &config));
}

void knobs_resync(void) // STM32 restarted or link came back -> send all knob positions again
{
    resync = true;
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

static float knob_value(int i, int raw) // raw 0..4095 -> parameter value
{
    float x = (float)(raw - KNOB_END) / (4095 - 2 * KNOB_END);
    if (x < 0.0f) x = 0.0f;
    if (x > 1.0f) x = 1.0f;
    if (i == 0) return x * x;                       // volume, audio taper
    if (i == 1) return powf(200.0f, x);             // drive 1..200, exponential
    float db = -12.0f + 24.0f * x;                  // eq, 0 dB zone in the middle (no centre click)
    if (fabsf(db) < 0.75f) db = 0.0f;
    return db;
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
        for (int i = 0; i < KNOB_COUNT; i++)
        {
            if (!knob_on[i] || !stm32_online()) continue;
            int raw = knob_read(i);
            bool moved = knob_last[i] < 0 || abs(raw - knob_last[i]) >= KNOB_HYST;
            bool at_end = (raw <= KNOB_END && knob_last[i] > KNOB_END) ||
                          (raw >= 4095 - KNOB_END && knob_last[i] < 4095 - KNOB_END);
            if (!moved && !at_end) continue;
            knob_last[i] = raw;
            snprintf(line, sizeof(line), "SET %s %.3f", knob_param[i], knob_value(i, raw));
            stm32_send(line);
            ESP_LOGD(TAG, "%s raw %d", knob_param[i], raw);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
