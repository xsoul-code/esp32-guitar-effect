#include "include/mqtt.h"
#include "esp_log.h"
#include "include/oled.h"
#include <inttypes.h>
#include <string.h>

esp_mqtt_client_handle_t mqtt_client = NULL;

#define TAG "MQTT_Service"

char mqtt_status[17] = "MQTT: ---";

// This function created with ESP-IDF Example - mqtt/main/app_main.c see references in README.md
static void mqtt_event_handler(void* handler_arg, esp_event_base_t base, int32_t event_id, void* event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch(event_id)
    {
        case MQTT_EVENT_CONNECTED: {
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            strncpy(mqtt_status, "MQTT: OK", sizeof(mqtt_status));
            msg_id = esp_mqtt_client_subscribe(client, "pedal/cmd", 1);
            ESP_LOGI(TAG, "subscribe successful, msg_id=%d", msg_id);
            break;
        }
        case MQTT_EVENT_DISCONNECTED: {
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            strncpy(mqtt_status, "MQTT: OFF", sizeof(mqtt_status));
            break;
        }
        case MQTT_EVENT_BEFORE_CONNECT:
        {
            ESP_LOGI(TAG, "MQTT_EVENT_BEFORE_CONNECT");
            break;
        }
        case MQTT_EVENT_PUBLISHED:
        {
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        }
        case MQTT_EVENT_SUBSCRIBED: {
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            msg_id = esp_mqtt_client_publish(client, "pedal/status", "online", 0, 1, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        }
        case MQTT_EVENT_UNSUBSCRIBED: {
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        }
        case MQTT_EVENT_DATA: {
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            ESP_LOGI(TAG, "Topic=%.*s Data=%.*s",
                    event->topic_len, event->topic,
                    event->data_len, event->data);
            break;
        }
        case MQTT_EVENT_ERROR: {
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGI(TAG, "Last captured errno : %d (%s)", event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));
        } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        } else {
            ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
            break;
        }
        default: {
            ESP_LOGI(TAG, "Other event id:%" PRIi32, event_id); // PRIi32 not %d because of i32 (inttypes.h)
            break;
        }

    }
}



void mqtt_init(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_MQTT_BROKER_URI,
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}