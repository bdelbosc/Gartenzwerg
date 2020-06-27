#include "main.h"

#include "nvs_flash.h"

#include "bmp.h"
#include "ultrason.h"
#include "wifi.h"
#include "mqtt.h"

#include "esp_log.h"
#include <string.h>

static const char *TAG = "gz_main";

/**
 * @brief encode a WeatherMessage in JSON format
 * 
 * @param msg the output result will be written here
 * @param msg_struct WeatherMessage to be encoded
 */
void create_weather_msg(char *msg, struct BmpMessage *bmp, struct UltrasonicMessage *ultrasonic)
{
	float distance = convert_echo_m(&ultrasonic->echo_us, &bmp->temperature);
	sprintf(msg,
			"{\"temp\":%.2f,\"pressure\":%.2f,\"humidity\":%.2f,\"distance\":%.3f}",
			bmp->temperature, bmp->pressure, bmp->humidity, distance);
}

void log_init() {
	esp_log_level_set("*", ESP_LOG_INFO);
	esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
	esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
	esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
	esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
	esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
	esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
	esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);
}

void nvs_init() {
	ESP_LOGI(TAG, "Init NonVolatileStorage");
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
}

void i2c_init() {
	ESP_LOGI(TAG, "Init i2c");
	ESP_ERROR_CHECK(i2cdev_init());
}

QueueHandle_t bmp_collect_async() {
	QueueHandle_t queue = xQueueCreate(2, sizeof(struct BmpMessage));
	xTaskCreatePinnedToCore(bmp280_collect_data, "bmp280_collect_data",
			configMINIMAL_STACK_SIZE * 8, (void*) queue, 5, NULL, APP_CPU_NUM);
	return queue;
}

QueueHandle_t ultrasonic_collect_async() {
	QueueHandle_t queue = xQueueCreate(2, sizeof(struct UltrasonicMessage));
	xTaskCreatePinnedToCore(ultrasonic_collect_data, "ultrasonic_collect_data",
			configMINIMAL_STACK_SIZE * 8, (void*) queue, 5, NULL, PRO_CPU_NUM);
	return queue;
}

void wifi_init() {
	ESP_LOGI(TAG, "Init Wifi ...");
	EventGroupHandle_t event_group = xEventGroupCreate();
	wifi_connect_blocking(&event_group);
	ESP_LOGI(TAG, "Init Wifi done");
}

void app_main()
{
	ESP_LOGI(TAG, "Start main");

	log_init();

	i2c_init();

	nvs_init();

	QueueHandle_t bmp_queue = bmp_collect_async();

	QueueHandle_t ultrasonic_queue = ultrasonic_collect_async();

	// blocking
	wifi_init();

	QueueHandle_t queue = xQueueCreate(2, sizeof(struct JsonMessage));
	mqtt_publish_async(queue);

	struct BmpMessage bmp_msg;
	if (xQueueReceive(bmp_queue, &(bmp_msg), pdMS_TO_TICKS(1000))) {
		ESP_LOGI(TAG, "Got data from BMP");
	} else {
		ESP_LOGE(TAG, "Unable to get BMP sensor data");
	}
	struct UltrasonicMessage ultrasonic_msg;
	if (xQueueReceive(ultrasonic_queue, &(ultrasonic_msg), pdMS_TO_TICKS(1000))) {
		ESP_LOGI(TAG, "Got data from Ultrasonic sensor");
	} else {
		ESP_LOGE(TAG, "Unable to get Ultrasonic data");
	}
	struct JsonMessage json_msg;
	create_weather_msg(json_msg.json, &bmp_msg, &ultrasonic_msg);
	xQueueSendToFront(queue, (void *)&json_msg, pdMS_TO_TICKS(200));
}
