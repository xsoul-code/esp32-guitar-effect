#include "include/stm32_uart.h"
#include "include/web.h"
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define TAG "STM32_UART"

#define STM32_UART UART_NUM_2
#define UART_TX GPIO_NUM_17              // -> STM32 PD9 (USART3_RX)
#define UART_RX GPIO_NUM_16              // <- STM32 PD8 (USART3_TX)
#define UART_BAUD_RATE 115200

#define RX_BUF_SIZE 1024

static volatile TickType_t last_rx_tick = 0;

void stm32_uart_init(void) // UART initializaion (ESP-IDF GitHub see references in README.md)
{
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(STM32_UART, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(STM32_UART, UART_TX, UART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(STM32_UART, RX_BUF_SIZE * 2, RX_BUF_SIZE, 0, NULL, 0));
}

int stm32_send(const char *cmd) // protocol: docs/UART_PROTOCOL.md
{
    char buf[STM32_LINE_MAX];
    if (strpbrk(cmd, "\r\n") != NULL) return -1;
    int len = snprintf(buf, sizeof(buf), "%s\n", cmd);
    if (len < 0 || len >= (int)sizeof(buf)) return -1;
    return uart_write_bytes(STM32_UART, buf, len);
}

bool stm32_online(void) // any line from STM32 in last 3 s
{
    return last_rx_tick != 0 && (xTaskGetTickCount() - last_rx_tick) < pdMS_TO_TICKS(3000);
}

static void stm32_handle_line(char *line)
{
    bool was_online = stm32_online();
    last_rx_tick = xTaskGetTickCount();

    if (strcmp(line, "PONG") == 0) {
        ESP_LOGD(TAG, "PONG");
    } else if (strncmp(line, "HELLO", 5) == 0) {
        ESP_LOGI(TAG, "STM32 restarted: %s", line);
        if (was_online) stm32_send("GET ALL");
    } else if (strncmp(line, "ERR", 3) == 0) {
        ESP_LOGW(TAG, "%s", line);
    } else {
        ESP_LOGI(TAG, "%s", line);
    }
}

void stm32_rx_task(void *pvParameters) // source: Github esp-idf/examples/peripherals/uart/uart_echo
{
    uint8_t *data = (uint8_t *) malloc(RX_BUF_SIZE);
    char line[STM32_LINE_MAX];
    static char out[RX_BUF_SIZE + STM32_LINE_MAX];  // lines for web clients, one WebSocket frame per read
    int pos = 0;
    int out_len = 0;
    bool too_long = false;
    while(1)
    {
        int len = uart_read_bytes(STM32_UART, data, RX_BUF_SIZE, pdMS_TO_TICKS(20));
        for (int i = 0; i < len; i++)
        {
            char c = data[i];
            if (c == '\r') continue;
            if (c == '\n')
            {
                line[pos] = '\0';
                if (pos > 0 && !too_long)
                {
                    stm32_handle_line(line);
                    if (strcmp(line, "PONG") != 0)
                        out_len += snprintf(out + out_len, sizeof(out) - out_len, "%s\n", line);
                }
                pos = 0;
                too_long = false;
            }
            else if (pos < STM32_LINE_MAX - 1) line[pos++] = c;
            else too_long = true;
        }
        if (out_len > 0)
        {
            web_broadcast(out);
            out_len = 0;
        }
    }
    free(data);
}

void stm32_ping_task(void *pvParameters)
{
    bool was_online = false;
    while(1)
    {
        stm32_send("PING");
        vTaskDelay(pdMS_TO_TICKS(1000));

        bool online = stm32_online();
        if (online != was_online)
        {
            ESP_LOGI(TAG, "STM32 %s", online ? "online" : "offline");
            web_broadcast(online ? "LINK up" : "LINK down");
            if (online) stm32_send("GET ALL");
            was_online = online;
        }
    }
}
