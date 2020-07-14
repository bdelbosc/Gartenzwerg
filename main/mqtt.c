#include "mqtt.h"

static const char *TAG = "gz_mqtt";

/**
 * @brief read message from the queue and send it to the MQTT topic
 * 
 * @param client MQTT client
 * @param weather_msg_queue FreeRTOS Queue which contains updates 
 */
static void send_message_to_mqtt(esp_mqtt_client_handle_t *client,
        QueueHandle_t *json_queue) {
    int msg_id;
    // if our queue handle points to somewhere
    if (json_queue != 0) {
        // wait for the new message to arrive
        ESP_LOGI(TAG, "Receiving message from the queue");
        struct JsonMessage msg;
        if (xQueueReceive(json_queue, &(msg), pdMS_TO_TICKS(1000))) {
            ESP_LOGI(TAG, "Got new message from the queue");
            // publish JSON message to the MQTT topic
            ESP_LOGI(TAG, "Sending JSON message: %s", msg.json);
            msg_id = esp_mqtt_client_publish(*client, "weather", msg.json, 0, 1,
                    0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

            // enable deep sleep mode for SLEEP_TIME ms
            // deep sleep lowers the power comsumption to about 0.15 mA, which will make out battery last much longer
            // TODO this is a bad place for this code since it clearly violates Single Responsibility Principle
            // This should be moved into separate function that will be called elsewhere
            const float SLEEP_TIME = 30 * 1e6;
            ESP_LOGI(TAG, "going to deep sleep for %.1f", SLEEP_TIME / 1e6);
            ESP_ERROR_CHECK(esp_wifi_stop());
            esp_deep_sleep(SLEEP_TIME);
        }
    }
}

/**
 * @brief MQTT event processing handler
 * 
 * @param event set by ESP-IDF
 * @param weather_msg_queue the queue to publish WeatherMessages
 * @return esp_err_t 
 */
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event,
        QueueHandle_t json_queue) {
    esp_mqtt_client_handle_t client = event->client;
    switch (event->event_id) {
        case MQTT_EVENT_BEFORE_CONNECT:
            ESP_LOGI(TAG, "Receiving event: MQTT_EVENT_BEFORE_CONNECT");
            break;
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Receiving event: MQTT_EVENT_CONNECTED");
            send_message_to_mqtt(&client, json_queue);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Receiving event: MQTT_EVENT_DISCONNECTED");
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_ESP_TLS) {
                ESP_LOGE(TAG, "Last error code reported from esp-tls: 0x%x",
                        event->error_handle->esp_tls_last_esp_err);
                ESP_LOGE(TAG, "Last tls stack error number: 0x%x",
                        event->error_handle->esp_tls_stack_err);
            } else if (event->error_handle->error_type
                    == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGE(TAG, "Connection refused error: 0x%x",
                        event->error_handle->connect_return_code);
            } else {
                ESP_LOGW(TAG, "Unknown error type: 0x%x",
                        event->error_handle->error_type);
            }
            break;
        default:
            ESP_LOGI(TAG, "Receiving unknown event: %d", event->event_id);
            break;
    }
    return ESP_OK;
}

/**
 * @brief A top-level handler that wraps around mqtt_event_handler_cb 
 * 
 * @param handler_args QueueHandle_t queue for WeatherMessages
 * @param base set by ESP-IDF
 * @param event_id set by ESP-IDF
 * @param event_data set by ESP-IDF
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
        int32_t event_id, void *event_data) {
    QueueHandle_t json_queue = (QueueHandle_t) handler_args;
    ESP_LOGI(TAG, "mqtt_event_handler QueueHandle: %p", json_queue);
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base,
            event_id);
    mqtt_event_handler_cb(event_data, json_queue);
}

/**
 * @brief starts the MQTT component
 *
 */
void mqtt_publish_async(QueueHandle_t queue) {
    ESP_LOGI(TAG, "mqtt_publish start Queues: %p", queue);
    ESP_LOGI(TAG, "Connecting to MQTT server: %s", CONFIG_BROKER_URL);
    esp_mqtt_client_config_t mqtt_cfg = { .uri = CONFIG_BROKER_URL };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    // let's register our handler
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler,
            (void*) queue);
    esp_mqtt_client_start(client);
}
