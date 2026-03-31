#include "include/mqtt.h"

esp_mqtt_client_handle_t mqtt_client = NULL;

void mqtt_init(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
    .broker.address.uri = "mqtt://" MQTT_BROKER_IP,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(mqtt_client);
}