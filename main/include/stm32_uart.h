#pragma once
#include <stdbool.h>
#include "freertos/FreeRTOS.h"

#define STM32_LINE_MAX 64

typedef struct {
    int bypass;
    int fx_dist;
    int fx_chorus;
    int fx_echo;
    int fx_reverb;
} stm32_state_t;

extern volatile stm32_state_t stm32_state;  // last values from VAL, -1 = unknown

typedef struct {
    char name[24];
    char value[16];
    TickType_t tick;        // when it changed, 0 = never
} stm32_change_t;

extern stm32_change_t stm32_last_change;   // last parameter changed by SET, shown on OLED for 3 s

void stm32_uart_init(void);
void stm32_rx_task(void *pvParameters);
void stm32_ping_task(void *pvParameters);
int stm32_send(const char *cmd);
bool stm32_online(void);
