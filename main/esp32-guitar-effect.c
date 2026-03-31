#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "include/wifi.h"
#include "nvs_flash.h"

#define TAG "Smart Guitar Effect"

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

void UART_task(void *pvParameters)
{

}

void ui_task(void *pvParameters)
{

}

void footswitch_task(void *pvParameters)
{

}

void app_main(void)
{
    // Wifi section
    ESP_LOGI(TAG, "Initializing NVS");
    nvs_flash_init();

    ESP_LOGI(TAG, "WiFi MODE STA (station)");
    wifi_init_sta();
    
    // Queues section
    buffer1 = xQueueCreate(2, sizeof(stm32samples));

    // Tasks section
    xTaskCreate(debug_monitor, "data_logger", 2048, NULL, 5, NULL);
    xTaskCreate(UART_task, "uart_task", 8192, NULL, 5, NULL);
    xTaskCreate(ui_task, "ui_task", 4096, NULL, 5, NULL);
    xTaskCreate(footswitch_task, "footswitch_task", 2048, NULL, 5, NULL);
}
