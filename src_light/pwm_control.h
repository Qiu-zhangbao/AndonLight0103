/**
 * @file pwm_control.h
 * @author ljy
 * @date 2019-03-01
 * 
 *  PWM 输出控制
 */

#pragma once

#include <stdint.h>
#include "wiced_hal_pwm.h"
#include "wiced_hal_gpio.h"
#include "wiced_hal_aclk.h"

// #define CHIP_SCHEME CHIP_DEVKIT

#define CHIP1424 1424
#define CHIP6322 6322
#define CHIP2306 2306
#define CHIP5600 5600
#define CHIP58834 58834
#define CHIP_DEVKIT -1

extern void led_controller_initial(void);
extern void led_controller_status_update(uint8_t onoff, uint16_t level, uint16_t temperature);

