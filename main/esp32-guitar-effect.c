#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "include/wifi.h"
#include "nvs_flash.h"

#include "esp_system.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#define UART_RX GPIO_NUM_16              // ESP32 UART2 RX GPIO PIN
#define UART_TX GPIO_NUM_17             // ESP32 UART2 TX GPIO PIN
#define UART_BAUD_RATE 115200

static const int RX_BUF_SIZE = 1024;   

#define TAG "Smart Guitar Effect"   // FIRMWARE TAG

QueueHandle_t buffer1;

typedef struct // Data structure representing STM32 UART samples acquired
{
    uint32_t timestamp;
    int64_t sample;
}stm32samples; 

void init_UART(void) // UART initializaion (ESP-IDF GitHub see references in README.md)
{
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, UART_TX, UART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, RX_BUF_SIZE * 2, RX_BUF_SIZE, 0, NULL, 0));
}

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

void UART_RX_task(void *pvParameters)
{
    while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
}

void ui_task(void *pvParameters)
{
    while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
}

void footswitch_task(void *pvParameters)
{
    while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
}

void app_main(void)
{
    // UART config section from void function
    init_UART();

    // Wifi section
    ESP_LOGI(TAG, "Initializing NVS");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "WiFi MODE STA (station)");
    wifi_init_sta();
    
    // Queues section
    buffer1 = xQueueCreate(2, sizeof(stm32samples));

    // Tasks section
    xTaskCreate(debug_monitor, "debug_monitor", 2048, NULL, 5, NULL);
    xTaskCreate(UART_RX_task, "uart_rx_task", 8192, NULL, 5, NULL);
    xTaskCreate(ui_task, "ui_task", 4096, NULL, 5, NULL);
    xTaskCreate(footswitch_task, "footswitch_task", 2048, NULL, 5, NULL);
    while(1)
    {
        ESP_LOGI(TAG, " Wandering around...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
