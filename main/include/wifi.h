#pragma once

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"

extern char wifi_status[17];
extern volatile bool wifi_connected;     // home WiFi (STA) has IP

void wifi_init_apsta(void);
