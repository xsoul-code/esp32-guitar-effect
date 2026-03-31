#pragma once

#include "mqtt_client.h"

#define MQTT_BROKER_IP "192.168.x.x"

extern esp_mqtt_client_handle_t mqtt_client;

void mqtt_init(void);