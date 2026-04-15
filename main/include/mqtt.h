#pragma once
#include "mqtt_client.h"
#include "esp_event_base.h"

extern esp_mqtt_client_handle_t mqtt_client;
extern char mqtt_status[17];

void mqtt_init(void);