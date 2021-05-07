/**
 * @file config.h
 * @author ljy
 * @date 2019-03-01
 *
 *  All hyper configurations should be placed in this file.
 *
 */

#pragma once

#include "stdint.h"
#include "version.h"

#include "wiced_bt_cfg.h"
#include "wiced_hal_nvram.h"
#include "wiced_bt_mesh_app.h"

#include "pwm_control.h"

#define USE_STANDARD_MODEL                 (0)
#define MESH_DFU                           (0)
#define ANDON_LIGHT_LOG_ENABLE             (0)

/**
 * \brief Mesh Light Lightness Server Device.
 * \details The Mesh Light Lightness Server Device device implements all required models for a dimmable bulb.
 * The time and scheduler models are optional and can be commented out to reduce memory footprint.
 */
// #define WICED_BT_MESH_MODEL_LIGHT_LIGHTNESS_SERVER \
//     { MESH_COMPANY_ID_BT_SIG, WICED_BT_MESH_CORE_MODEL_ID_GENERIC_ONOFF_SRV, mesh_lightness_message_handler, NULL, NULL }, \
//     { MESH_COMPANY_ID_BT_SIG, WICED_BT_MESH_CORE_MODEL_ID_GENERIC_LEVEL_SRV, mesh_lightness_message_handler, NULL, NULL }, \
//     { MESH_COMPANY_ID_BT_SIG, WICED_BT_MESH_CORE_MODEL_ID_LIGHT_LIGHTNESS_SRV, mesh_lightness_message_handler, NULL, NULL }, \


#define MESH_CID                (0x0804)

#if PRESS_TEST == 2
#define MESH_PID                (0x3015)  
#else
#define MESH_PID                (0x3130)
#endif

#define MANUFACTURER_CODE       "Andon"
#define PRODUCT_CODE            "JA_SL10"
// #define PRODUCT_CODE            "CO_TH1"

#define ANDON_HW_MAJOR          1         //0--9
#define ANDON_HW_MINOR          0         //0--9
#define ANDON_HW_PATCH          0         //0--127
#define MESH_VID                ((ANDON_HW_MAJOR<<12)&0xF000 | (ANDON_HW_MINOR<<8)&0x0F00 | ANDON_HW_PATCH&0xFF)
// #define MESH_VID      (0x0101)

#define ANDON_FW_MAJOR          0         //0--9
#define ANDON_FW_MINOR          1         //0--9
#define ANDON_FW_PATCH          0xA        //0--127
#define VERSION                 ((ANDON_FW_MAJOR<<12)&0xF000 | (ANDON_FW_MINOR<<8)&0x0F00 | ANDON_FW_PATCH&0xFF)
//#define MESH_FWID 0x3008000101010001
// #define VERSION       (0x0005)

#if ANDON_TEST
    #define MESH_FWID (VERSION|0x80)
#else
    #define MESH_FWID (VERSION&0XFF7F)
#endif

#if PRESS_TEST == 2
#define MESH_CACHE_REPLAY_SIZE (0x0080)  
#else
#define MESH_CACHE_REPLAY_SIZE (0x0008)
#endif

#define MESH_VENDOR_COMPANY_ID           MESH_CID // Cypress Company ID 
#define MESH_VENDOR_MODEL_ID             1       // ToDo.  This need to be modified

#define NVRAM_CONFIG_POSITION            (WICED_NVRAM_VSID_START)
#define NVRAM_REMOTEMAC_POSITION         0x210
#define NVRAM_TOOLING_POSITION           0x220
#define NVRAM_AUTOBRIGHTNESS_POSITION    0x230

//#define WRITE_ADDRESS (0x250)

#define MAX_LEVEL (100UL)
#define MIN_LEVEL (1UL)

#define SCAN_BEACON_PERIOD (10)
#define DEFAULT_TRANSITION_TIME (8) // *100ms

extern  char *mesh_dev_name;
// extern uint8_t mesh_appearance[];


extern wiced_bt_cfg_settings_t mywiced_bt_cfg_settings;

#define MESH_LIGHT_CTL_SERVER_ELEMENT_INDEX 0
#define MESH_LIGHT_CTL_TEMPERATURE_SERVER_ELEMENT_INDEX 1

#define configLIGHTRSTCNT            250   //恢复出厂设置的上下电设置, 实际次数为255-configLIGHTRSTCNT     
#define configLIGHTRSTRESTARTCNT     200   //恢复出厂设置重启时的计数   

// #define CONFIG_POWERON_OFF           0     //上电关灯
// #define CONFIG_POWERON_ON            1     //上电开灯
// #define CONFIG_POWERON_HOLD          2     //上电维持原来状态

#define CONFIG_POWERON_NORMOL        0     //上电关灯
#define CONFIG_POWERON_RESETSWITCH   1     //上电开灯

#define CONFIG_UNPAIR                0     //未被配对过 出厂模式
#define CONFIG_PAIRED                0x55  //已被配对过 配对后常规恢复出厂设置的方法不改变此标记

#define CONFIG_BLEANDMESH            0     //同时开启ble和mesh
#define CONFIG_BLEONLY               1     //仅开启ble
#define CONFIG_MESHONLY              2     //仅开启mesh

#define configLIGHTAINULL            0     
#define configLIGHTAIANDONMODE       1     
#define configLIGHTAIWYZEMODE        2      

#define CONFIG_OFFDELAY_MIN          3      //单位s
#define CONFIG_OFFDELAY_MAX          36000  //单位s
#define CONFIG_ONDELAY_MIN           3      //单位s
#define CONFIG_ONDELAY_MAX           36000  //单位s
#define CONFIG_DELTASTEP_MIN         2
#define CONFIG_DELTASTEP_MAX         20

#define CONFIG_DEFAULT_POWERONMode   CONFIG_POWERON_RESETSWITCH
#define CONFIG_DEFAULT_STEP          11
#define CONFIG_DEFAULT_BRIGHTNESS    65535    //默认最大亮度
#define CONFIG_DEFAULT_DELAYSET      1    //默认延迟功能开
#define CONFIG_DEFAULT_OFFDELAY      30   //关灯延迟，单位秒
#define CONFIG_DEFAULT_ONDELAY       30   //关灯延迟，单位秒

//默认关灯延迟10s
#define DEFAULT_LIGHTNESS_CONFIG { \
    .lightingOn = 1,               \
    .lightnessLevel = CONFIG_DEFAULT_BRIGHTNESS,       \
    .lightnessCTL = 32768,          \
    .Lightontime  = 0,              \
    .offdelayset  = CONFIG_DEFAULT_DELAYSET,           \
    .offdelaytime = CONFIG_DEFAULT_OFFDELAY,           \
    .ondelaytime  = CONFIG_DEFAULT_ONDELAY,            \
    .Lightruntime = 0,                  \
    .LightUTCtime = 0,                  \
    .Lightzone    = 0,                  \
    .brightdeltastep  = CONFIG_DEFAULT_STEP, \
    .bleonly      = CONFIG_BLEONLY,  \
    .powerOnMode  = CONFIG_DEFAULT_POWERONMode,  \
    .fristpair    = CONFIG_UNPAIR,      \
    .fwupgrade    = 0              \
    };

    