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

#include "include/oled.h"
#include "include/mqtt.h"

#define UART_RX GPIO_NUM_16              // ESP32 UART2 RX GPIO PIN
#define UART_TX GPIO_NUM_17              // ESP32 UART2 TX GPIO PIN
#define UART_BAUD_RATE 115200            // UART MATCH BAUD RATE SETTING

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

void UART_RX_task(void *pvParameters) // source: Github esp-idf/examples/peripherals/uart/uart_async_rxtxtasks/main/uart_async_rxtxtasks_main.c
{
    esp_log_level_set(TAG, ESP_LOG_INFO);
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE + 1);
    while(1) 
    {
        const int rxBytes = uart_read_bytes(UART_NUM_2, data, RX_BUF_SIZE, 1000 / portTICK_PERIOD_MS);
        if (rxBytes > 0) 
        {
            data[rxBytes] = 0;
            ESP_LOGI(TAG, "Read %d bytes: '%s'", rxBytes, data);
            ESP_LOG_BUFFER_HEXDUMP(TAG, data, rxBytes, ESP_LOG_INFO);    
        } 
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
    free(data);
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
    init_UART();
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
    xTaskCreate(UART_RX_task, "uart_rx_task", 8192, NULL, 5, NULL);
    xTaskCreate(ui_task, "ui_task", 4096, NULL, 5, NULL);
    xTaskCreate(footswitch_task, "footswitch_task", 2048, NULL, 5, NULL);

    // Successfull initialization loop
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
