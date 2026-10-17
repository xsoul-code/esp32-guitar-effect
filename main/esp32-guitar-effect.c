#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "include/wifi.h"
#include "nvs_flash.h"

#include "esp_system.h"
#include "esp_netif_sntp.h"

#include "include/oled.h"
#include "font8x8_basic.h"      // font from ssd1306 component, to draw single (inverted) characters
#include "include/mqtt.h"
#include "include/stm32_uart.h"
#include "include/web.h"

#define TAG "Smart Guitar Effect"   // FIRMWARE TAG

QueueHandle_t buffer1;

typedef struct // Data structure representing STM32 UART samples acquired
{
    uint32_t timestamp;
    int64_t sample;
}stm32samples; 

void debug_monitor(void *pvParameters)
{
    stm32samples sample;
    while(1)
    {
        if(xQueueReceive(buffer1, (void*)&sample, pdMS_TO_TICKS(300)))
        {
            ESP_LOGI(TAG, "STM32 sample captured on: %lu ms with value: %lld", (unsigned long)sample.timestamp, (long long)sample.sample);
        }
    }
}

static void clock_init(void) // source: Github esp-idf/examples/protocols/sntp
{
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);     // Polish time (CET/CEST)
    tzset();
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&config);
}

#define NO_INV "0000000000000000"

static const uint8_t icon_wifi[8] = {0x04, 0x12, 0x0A, 0x6A, 0x6A, 0x0A, 0x12, 0x04};   // char '\x01'
static const uint8_t icon_phone[8] = {0x00, 0x00, 0x7F, 0x51, 0x51, 0x7F, 0x00, 0x00};  // char '\x02'

static char shown[8][17];        // what is on the OLED now
static char shown_inv[8][17];

static void oled_line(int page, const char *text, const char *inv) // inv[i] = '1' -> char i inverted, redraw only if changed
{
    char t[17];
    snprintf(t, sizeof(t), "%-16.16s", text);
    if (strcmp(shown[page], t) == 0 && strcmp(shown_inv[page], inv) == 0) return;
    uint8_t image[8];
    for (int i = 0; i < 16; i++)
    {
        if (t[i] == '\x01') memcpy(image, icon_wifi, 8);
        else if (t[i] == '\x02') memcpy(image, icon_phone, 8);
        else memcpy(image, font8x8_basic_tr[(uint8_t)t[i]], 8);
        if (inv[i] == '1') ssd1306_invert(image, 8);
        if (dev._flip) ssd1306_flip(image, 8);
        ssd1306_display_image(&dev, page, i * 8, image, 8);
    }
    strcpy(shown[page], t);
    strcpy(shown_inv[page], inv);
}

static void draw_tiles(int text_page, int pad_page, const char *a, bool a_on, const char *b, bool b_on) // two 7-char tiles, 2 lines high
{
    char text[17], inv[17];
    int pa = (7 - (int)strlen(a)) / 2, pb = (7 - (int)strlen(b)) / 2;
    snprintf(text, sizeof(text), "%*s%-*s  %*s%-*s", pa, "", 7 - pa, a, pb, "", 7 - pb, b);
    snprintf(inv, sizeof(inv), "%s00%s", a_on ? "1111111" : "0000000", b_on ? "1111111" : "0000000");
    oled_line(pad_page, "", inv);
    oled_line(text_page, text, inv);
}

void ui_task(void *pvParameters)
{
    char line[32], clock[8];
    int panels = 0, loop = 0;
    while(1)
    {
        bool online = stm32_online();
        bool bypass = stm32_state.bypass == 1;
        bool any = stm32_state.fx_dist == 1 || stm32_state.fx_chorus == 1 ||
                   stm32_state.fx_echo == 1 || stm32_state.fx_reverb == 1;

        const char *left = "GuitarFX";
        if (!online) left = "STM32 ---";
        else if (bypass) left = " BYPASS ";
        else if (!any) left = "CLEAN";

        time_t now = time(NULL);
        struct tm t;
        localtime_r(&now, &t);
        if (t.tm_year > 2024 - 1900) strftime(clock, sizeof(clock), "%H:%M", &t);
        else strcpy(clock, "--:--");

        snprintf(line, sizeof(line), "%-10s%c%s", left, wifi_connected ? '\x01' : ' ', clock);
        oled_line(0, line, (online && bypass) ? "1111111100000000" : NO_INV);

        bool lit = online && !bypass;    // like the LED on a pedal
        draw_tiles(2, 1, online ? "DIST" : "-", lit && stm32_state.fx_dist == 1, online ? "CHORUS" : "-", lit && stm32_state.fx_chorus == 1);
        oled_line(3, "", NO_INV);
        draw_tiles(4, 5, online ? "ECHO" : "-", lit && stm32_state.fx_echo == 1, online ? "REVERB" : "-", lit && stm32_state.fx_reverb == 1);

        if (loop++ % 10 == 0) panels = web_client_count();     // once per second
        if (stm32_last_change.tick != 0 && xTaskGetTickCount() - stm32_last_change.tick < pdMS_TO_TICKS(3000))
        {
            oled_line(6, stm32_last_change.name, NO_INV);
            oled_line(7, stm32_last_change.value, NO_INV);
        }
        else
        {
            snprintf(line, sizeof(line), "%-13s\x02%-2d", wifi_connected ? "pedal.local" : "192.168.4.1", panels);
            oled_line(6, "", NO_INV);
            oled_line(7, line, NO_INV);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void footswitch_task(void *pvParameters)
{
    while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
}

void app_main(void)
{
    // OLED section
    oled_init();
    ssd1306_display_text(&dev, 0, "Smart Guitar FX", 15, false);

    // UART config section from void function
    ssd1306_display_text(&dev, 1, "UART: ...", 9, false);
    stm32_uart_init();
    ssd1306_display_text(&dev, 1, "UART: OK", 8, false);

    // NVS section
    ESP_LOGI(TAG, "Initializing NVS");
    ssd1306_display_text(&dev, 2, "NVS:  ...", 9, false);
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ssd1306_display_text(&dev, 2, "NVS:  OK", 8, false);

    // WiFi section
    ssd1306_display_text(&dev, 3, "WiFi:  ...", 9, false);
    ESP_LOGI(TAG, "WiFi MODE APSTA (home WiFi + pedal WiFi)");
    wifi_init_apsta();
    ssd1306_display_text(&dev, 3, "WiFi: OK", 8, false);

    // Clock section (SNTP over home WiFi)
    clock_init();

    // Web server section (panel: http://<ESP32 IP>/)
    ssd1306_display_text(&dev, 4, "WEB:  ...", 9, false);
    web_init();
    ssd1306_display_text(&dev, 4, "WEB:  OK", 8, false);

    // MQTT section
    ssd1306_display_text(&dev, 3, "MQTT: ...", 10, false);
    mqtt_init();
    ssd1306_display_text(&dev, 3, "MQTT: OK", 10, false);

    // POST time
    vTaskDelay(pdMS_TO_TICKS(5000));
    ssd1306_clear_screen(&dev, false);
    // End POST time

    // Queues section
    buffer1 = xQueueCreate(2, sizeof(stm32samples));

    // Tasks section
    xTaskCreate(debug_monitor, "debug_monitor", 2048, NULL, 5, NULL);
    xTaskCreate(stm32_rx_task, "stm32_rx_task", 4096, NULL, 6, NULL);
    xTaskCreate(stm32_ping_task, "stm32_ping_task", 3072, NULL, 5, NULL);
    xTaskCreate(ui_task, "ui_task", 4096, NULL, 5, NULL);
    xTaskCreate(footswitch_task, "footswitch_task", 2048, NULL, 5, NULL);

    // Successfull initialization loop
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
