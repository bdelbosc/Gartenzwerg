#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "esp_err.h"
#include "esp_log.h"
#include <bmp280.h>
#include <string.h>
#include "main.h"

void bmp280_collect_data(void *pvParamters);
