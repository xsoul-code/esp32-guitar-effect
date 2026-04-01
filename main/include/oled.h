#pragma once
#include "ssd1306.h"

void oled_init(void);
extern SSD1306_t dev;
extern char wifi_status[17];
extern char mqtt_status[17];