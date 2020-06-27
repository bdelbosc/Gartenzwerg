#include "ultrason.h"

static const char *TAG = "gz_ultrason";
#define TRIGGER_PIN GPIO_NUM_33
#define ECHO_PIN GPIO_NUM_32
#define MAX_DISTANCE_MM 5000 // 5m max


void send_msg(QueueHandle_t queue, int echo) {
	// publish data
	ESP_LOGI(TAG, "Sending data to queue: echo: %d us\n", echo);
	struct UltrasonicMessage msg;
	msg.echo_us = echo;
	xQueueSendToFront(queue, (void* )&msg, pdMS_TO_TICKS(200));
}

/**
 * @brief collect data from  ultrasonic range meters, e.g. HC-SR04, HY-SRF05
 * 
 * @param pvParamters a pointer to QueueHandle_t to which this function will send the data collected from the sensor
 */
void ultrasonic_collect_data(void *pvParamters)
{
	ESP_LOGI(TAG, "Start ultrasonic collect");

	// init sensor
	ultrasonic_sensor_t dev = {
			.trigger_pin = TRIGGER_PIN,
			.echo_pin = ECHO_PIN
	};
	esp_err_t ret = ultrasonic_init(&dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error on init got: %d, %s", ret, esp_err_to_name(ret));
    }

    // read distance
    int echo;
    ret = ultrasonic_measure_echo(&dev, &echo);
    if (ret != ESP_OK) {
    	ESP_LOGE(TAG, "Error on measure got: %d, %s", ret, esp_err_to_name(ret));
    }

    // send message
	send_msg((QueueHandle_t) pvParamters, echo);

    // terminate
	vTaskDelete(NULL);
	ESP_LOGI(TAG, "End ultrasonic collect");
}
