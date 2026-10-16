#pragma once
#include <stdbool.h>

#define STM32_LINE_MAX 64

void stm32_uart_init(void);
void stm32_rx_task(void *pvParameters);
void stm32_ping_task(void *pvParameters);
int stm32_send(const char *cmd);
bool stm32_online(void);
