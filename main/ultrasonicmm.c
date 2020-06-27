/**
 * @file ultrasonic.c
 *
 * ESP-IDF driver for ultrasonic range meters, e.g. HC-SR04, HY-SRF05 and the like
 *
 * Ported from esp-open-rtos
 *
 * Copyright (C) 2016, 2018 Ruslan V. Uss <unclerus@gmail.com>
 *
 * BSD Licensed as described in the file LICENSE
 */
#include <esp_idf_lib_helpers.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <esp_err.h>
#include "ultrasonicmm.h"

static const char *TAG = "gz_ultrasonic";

#define TRIGGER_LOW_DELAY 4
#define TRIGGER_HIGH_DELAY 10
#define PING_TIMEOUT 6000
#define ECHO_TIMEOUT_US 40000
#define MAX_DISTANCE 6000

// distance in meter is (high time * speed of sound (340M / s)) / 2
// rountrip in ms and mm 340/2000 = 588.23
#define ROUNDTRIP 58
#define NB_MEASURES 20
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
#define PORT_ENTER_CRITICAL portENTER_CRITICAL(&mux)
#define PORT_EXIT_CRITICAL portEXIT_CRITICAL(&mux)

#define timeout_expired(start, len) ((esp_timer_get_time() - (start)) >= (len))

#define CHECK_ARG(VAL) do { if (!(VAL)) return ESP_ERR_INVALID_ARG; } while (0)
#define CHECK(x) do { esp_err_t __; if ((__ = x) != ESP_OK) return __; } while (0)
#define RETURN_CRITICAL(RES) do { PORT_EXIT_CRITICAL; return RES; } while(0)

int echos[NB_MEASURES];

int compare( const void* a, const void* b)
{
	int int_a = * ( (int*) a );
	int int_b = * ( (int*) b );

	if ( int_a == int_b ) return 0;
	else if ( int_a < int_b ) return -1;
	else return 1;
}


esp_err_t ultrasonic_init(const ultrasonic_sensor_t *dev)
{
    CHECK_ARG(dev);
    CHECK(gpio_set_direction(dev->trigger_pin, GPIO_MODE_OUTPUT));
    CHECK(gpio_set_direction(dev->echo_pin, GPIO_MODE_INPUT));
    return gpio_set_level(dev->trigger_pin, 0);
}

esp_err_t measure_echo(const ultrasonic_sensor_t *dev, int *echo) {
    PORT_ENTER_CRITICAL;
    // Ping: Low for 2..4 us, then high 10 us
    CHECK(gpio_set_level(dev->trigger_pin, 0));
    ets_delay_us(TRIGGER_LOW_DELAY);
    CHECK(gpio_set_level(dev->trigger_pin, 1));
    ets_delay_us(TRIGGER_HIGH_DELAY);
    CHECK(gpio_set_level(dev->trigger_pin, 0));

    if (gpio_get_level(dev->echo_pin)) {
    	RETURN_CRITICAL(ESP_ERR_ULTRASONIC_PING);
    }

    // Wait for echo
    int64_t start = esp_timer_get_time();
    while (!gpio_get_level(dev->echo_pin)) {
        if (timeout_expired(start, PING_TIMEOUT)) {
            RETURN_CRITICAL(ESP_ERR_ULTRASONIC_PING_TIMEOUT);
        }
    }
    // got echo, measuring
    int64_t echo_start = esp_timer_get_time();
    int64_t time = echo_start;
    int64_t max_time = echo_start + ECHO_TIMEOUT_US;
    while (gpio_get_level(dev->echo_pin))
    {
        if (esp_timer_get_time() > max_time) {
            *echo = (uint32_t) (time - echo_start);
            RETURN_CRITICAL(ESP_ERR_ULTRASONIC_ECHO_TIMEOUT);
        }
    }
    time = esp_timer_get_time();
    PORT_EXIT_CRITICAL;
    *echo = (uint32_t) (time - echo_start);
    ESP_LOGW(TAG, "start: %lu, end: %lu, echo: %u us",  (unsigned long) echo_start, (unsigned long) time, (unsigned) *echo);
    return ESP_OK;
}

float convert_echo_m(const int *echo, const float *temperature) {
	float c = 2000000 / (331.5 + 0.607 * (*temperature));
	return (*echo) / c;
}

esp_err_t ultrasonic_measure_echo(const ultrasonic_sensor_t *dev, int *echo) {
    CHECK_ARG(dev && echo);
    int count = 0;
    for (int i=0; i < NB_MEASURES; i++) {
    	ESP_LOGI(TAG, "measure %d", i);
    	switch ( measure_echo(dev, &echos[i]) ) {
    	case ESP_OK:
    		ESP_LOGI(TAG, "ok");
    		count++;
    		vTaskDelay(40 / portTICK_PERIOD_MS);
			break;
    	case ESP_ERR_ULTRASONIC_PING:
    		ESP_LOGW(TAG, "utrasonic ping");
    		vTaskDelay(40 / portTICK_PERIOD_MS);
    		break;
    	case ESP_ERR_ULTRASONIC_PING_TIMEOUT:
    		ESP_LOGW(TAG, "utrasonic ping timeout");
    		break;
    	case ESP_ERR_ULTRASONIC_ECHO_TIMEOUT:
    		ESP_LOGW(TAG, "utrasonic echo timeout");
    		count++;
    		break;
    	default:
    		ESP_LOGW(TAG, "unknown error");
    	}

    }
    qsort(echos, NB_MEASURES, sizeof(int), compare);
    for (int i=0; i < count; i++) {
    	ESP_LOGI(TAG, "measure %d: echo: %d", i, echos[i]);
    }
    *echo = echos[(NB_MEASURES - count) + count/2 -1];
    ESP_LOGI(TAG, "Select measure %d: %d", (NB_MEASURES - count) + count/2 - 1, *echo);
    if (count > 0) {
    	return ESP_OK;
    }
    return ESP_FAIL;
}
