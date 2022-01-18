#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "mqtt";

esp_mqtt_client_handle_t client;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "Event:  base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    // esp_mqtt_client_handle_t client = event->client;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        // if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        // {
        //     log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
        //     log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
        //     log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
        //     ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        // }
        break;
    default:
        ESP_LOGI(TAG, "Unknown event: event_id=%d", event->event_id);
        break;
    }
}

void init_mqtt(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_BROKER_URL,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    ESP_LOGI(TAG, "Start MQTT");
    esp_mqtt_client_start(client);
}

int mqtt_publish(char *topic, const char *data, int qos, int retain)
{
    ESP_LOGI(TAG, "Publish message");
    int msg_id = esp_mqtt_client_publish(client, topic, data, 0, qos, retain);
    return msg_id;
}

void mqtt_stop(void)
{
    ESP_LOGI(TAG, "Stop MQTT");
    esp_mqtt_client_stop(client);
}
