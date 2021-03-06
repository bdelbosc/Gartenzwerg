#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_sleep.h"
#include "main.h"

#include "mqtt_client.h"

void mqtt_publish_async(QueueHandle_t queue);
