/*
 * Copyright 2018, Cypress Semiconductor Corporation or a subsidiary of Cypress Semiconductor 
 *  Corporation. All rights reserved. This software, including source code, documentation and  related 
 * materials ("Software"), is owned by Cypress Semiconductor  Corporation or one of its 
 *  subsidiaries ("Cypress") and is protected by and subject to worldwide patent protection  
 * (United States and foreign), United States copyright laws and international treaty provisions. 
 * Therefore, you may use this Software only as provided in the license agreement accompanying the 
 * software package from which you obtained this Software ("EULA"). If no EULA applies, Cypress 
 * hereby grants you a personal, nonexclusive, non-transferable license to  copy, modify, and 
 * compile the Software source code solely for use in connection with Cypress's  integrated circuit 
 * products. Any reproduction, modification, translation, compilation,  or representation of this 
 * Software except as specified above is prohibited without the express written permission of 
 * Cypress. Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO  WARRANTY OF ANY KIND, EXPRESS 
 * OR IMPLIED, INCLUDING,  BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY 
 * AND FITNESS FOR A PARTICULAR PURPOSE. Cypress reserves the right to make changes to 
 * the Software without notice. Cypress does not assume any liability arising out of the application 
 * or use of the Software or any product or circuit  described in the Software. Cypress does 
 * not authorize its products for use in any products where a malfunction or failure of the 
 * Cypress product may reasonably be expected to result  in significant property damage, injury 
 * or death ("High Risk Product"). By including Cypress's product in a High Risk Product, the 
 *  manufacturer of such system or application assumes  all risk of such use and in doing so agrees 
 * to indemnify Cypress against all liability.
 */

/*
 * This file is the default pin configuration for the platform. The application can
 * override this configuration by adding its own pin configuration file.to the app folder.
 * Please note that its mandatory to name this file in the format "app_name_pin_config.c"
 */

#include "wiced_platform.h"
#include "src_light/pwm_control.h"

/* all the pins available on this platform and their chosen functionality */
const wiced_platform_gpio_t platform_gpio_pins[] =
    {
        [PLATFORM_GPIO_0 ] = {WICED_P00, WICED_GPIO              },      //Button
        [PLATFORM_GPIO_1 ] = {WICED_P04, WICED_PCM_OUT_I2S_DO    },
        [PLATFORM_GPIO_2 ] = {WICED_P05, WICED_PCM_IN_I2S_DI     },
        [PLATFORM_GPIO_3 ] = {WICED_P06, WICED_SPI_1_MOSI        },
        [PLATFORM_GPIO_4 ] = {WICED_P07, WICED_SPI_1_CLK         },
        [PLATFORM_GPIO_5 ] = {WICED_P12, WICED_PCM_CLK_I2S_CLK   },
        [PLATFORM_GPIO_6 ] = {WICED_P13, WICED_PCM_SYNC_I2S_WS   },
        [PLATFORM_GPIO_7 ] = {WICED_P16, WICED_SPI_1_MISO        },
        [PLATFORM_GPIO_8 ] = {WICED_P26, WICED_GPIO              },      //Default LED 2
        [PLATFORM_GPIO_9 ] = {WICED_P27, WICED_GPIO              },      //Optional LED 1
        #if (CHIP_SCHEME == CHIP_DEVKIT)
        [PLATFORM_GPIO_10] = {WICED_P29, WICED_UART_2_RXD        },
        #else
        [PLATFORM_GPIO_10] = {WICED_P29, WICED_GPIO              },
        #endif
        // [PLATFORM_GPIO_11] = {WICED_P32, WICED_UART_2_TXD        },
        [PLATFORM_GPIO_11] = {WICED_P32, WICED_GPIO              },
        [PLATFORM_GPIO_12] = {WICED_P38, WICED_SPI_1_CS          },
        #if (!CHECK_POWEROFF_VALUE) 
        [PLATFORM_GPIO_13] = {WICED_P28, WICED_GPIO         },
        #endif
        [PLATFORM_GPIO_14] = {WICED_P34, WICED_I2C_1_SCL         },
        [PLATFORM_GPIO_15] = {WICED_P10, WICED_UART_2_CTS        },
        [PLATFORM_GPIO_16] = {WICED_P11, WICED_UART_2_RTS        },
    };

/* LED configuration */
const wiced_platform_led_config_t platform_led[] =
    {
        [WICED_PLATFORM_LED_2] =
            {
                .gpio          = (wiced_bt_gpio_numbers_t*)&platform_gpio_pins[PLATFORM_GPIO_8].gpio_pin,
                .config        = ( GPIO_OUTPUT_ENABLE | GPIO_PULL_UP ),
                .default_state = GPIO_PIN_OUTPUT_HIGH,
            },
        [WICED_PLATFORM_LED_1] =
            {
                .gpio          = (wiced_bt_gpio_numbers_t*)&platform_gpio_pins[PLATFORM_GPIO_9].gpio_pin,
                .config        = ( GPIO_OUTPUT_ENABLE | GPIO_PULL_UP ),
                .default_state = GPIO_PIN_OUTPUT_HIGH,
            }
    };

const size_t led_count = (sizeof(platform_led) / sizeof(wiced_platform_led_config_t));

/* Button Configuration */
const wiced_platform_button_config_t platform_button[] =
    {
        [WICED_PLATFORM_BUTTON_2]
        {
            .gpio          = (wiced_bt_gpio_numbers_t*)&platform_gpio_pins[PLATFORM_GPIO_11].gpio_pin,
            .config        = ( GPIO_INPUT_ENABLE | GPIO_PULL_UP ),
            .default_state = GPIO_PIN_OUTPUT_LOW,
            .button_pressed_value = GPIO_PIN_OUTPUT_LOW,
        },
        [WICED_PLATFORM_BUTTON_1]
        {
			#if (!CHECK_POWEROFF_VALUE)
            .gpio          = (wiced_bt_gpio_numbers_t*)&platform_gpio_pins[PLATFORM_GPIO_13].gpio_pin,
            
            .config        = ( GPIO_INPUT_ENABLE | GPIO_PULL_UP_DOWN_NONE ),
            .default_state = GPIO_PIN_OUTPUT_LOW,
            #else
            .gpio          = (wiced_bt_gpio_numbers_t*)&platform_gpio_pins[PLATFORM_GPIO_0].gpio_pin,
            .config        = ( GPIO_INPUT_ENABLE | GPIO_PULL_UP ),
            .default_state = GPIO_PIN_OUTPUT_HIGH,
            #endif
            .button_pressed_value = GPIO_PIN_OUTPUT_LOW,
        }
    };

const size_t button_count = (sizeof(platform_button) / sizeof(wiced_platform_button_config_t));

/* GPIO Configuration */
const wiced_platform_gpio_config_t platform_gpio[] =
    {
        // {
        //     .gpio          = (wiced_bt_gpio_numbers_t*)&platform_gpio_pins[PLATFORM_GPIO_13].gpio_pin,
        //     .config        = ( GPIO_OUTPUT_ENABLE | GPIO_PULL_UP ),
        //     .default_state = GPIO_PIN_OUTPUT_LOW
        // },
        #if (CHIP_SCHEME != CHIP_DEVKIT)
        {
            .gpio          = (wiced_bt_gpio_numbers_t*)&platform_gpio_pins[PLATFORM_GPIO_10].gpio_pin,
            .config        = ( GPIO_OUTPUT_ENABLE | GPIO_PULL_UP_DOWN_NONE ),
            .default_state = GPIO_PIN_OUTPUT_LOW
        }
        #endif
    };

const size_t gpio_count = (sizeof(platform_gpio) / sizeof(wiced_platform_gpio_config_t));

