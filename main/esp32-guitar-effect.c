#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "include/wifi.h"
#include "nvs_flash.h"

#include "esp_system.h"

#include "include/oled.h"
#include "include/mqtt.h"
#include "include/stm32_uart.h"

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

void ui_task(void *pvParameters) 
{
    ssd1306_display_text(&dev, 0, "Smart Guitar FX", 15, false);
    
    int counter = 0;
    char buf[22];
    while(1)
    {
        // Uptime - linia 5
        snprintf(buf, sizeof(buf), "uptime: %ds", counter++);
        ssd1306_display_text(&dev, 5, "                ", 16, false);
        ssd1306_display_text(&dev, 5, buf, strlen(buf), false);

        vTaskDelay(pdMS_TO_TICKS(1000));
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
    ESP_LOGI(TAG, "WiFi MODE STA (station)");
    wifi_init_sta();
    ssd1306_display_text(&dev, 3, "WiFi: OK", 8, false);

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
