/**
 * @file ultrasonicmm.h
 * @defgroup ultrasonic ultrasonic
 * @{
 *
 * ESP-IDF driver for ultrasonic range meters, e.g. HC-SR04, HY-SRF05 and so on
 *
 * Ported from esp-open-rtos
 *
 * Copyright (C) 2016, 2018 Ruslan V. Uss <unclerus@gmail.com>
 *
 * BSD Licensed as described in the file LICENSE
 */
#pragma once

#include <driver/gpio.h>
#include "esp_log.h"

#define ESP_ERR_ULTRASONIC_PING         0x200
#define ESP_ERR_ULTRASONIC_PING_TIMEOUT 0x201
#define ESP_ERR_ULTRASONIC_ECHO_TIMEOUT 0x202

/**
 * Device descriptor
 */
typedef struct {
    gpio_num_t trigger_pin;
    gpio_num_t echo_pin;
} ultrasonic_sensor_t;

/**
 * Init ranging module
 * @param dev Pointer to the device descriptor
 * @return `ESP_OK` on success
 */
esp_err_t ultrasonic_init(const ultrasonic_sensor_t *dev);

/**
 * Measure echo length in us
 * @param dev Pointer to the device descriptor
 * @return `ESP_OK` on success
 */
esp_err_t ultrasonic_measure_echo(const ultrasonic_sensor_t *dev, int *echo);

float convert_echo_m(const int *echo, const float *temperature);
