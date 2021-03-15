/**
 * @file storage.h
 * @author ljy
 * @date 2019-03-01
 * 
 *  储存控制
 */

#pragma once

#include "stdint.h"
#include "raw_flash.h"


#define LOG_IDLETYPE                0xFFFF
#define LOG_TIMETYPE                0
#define LOG_ONOFFTYPE               1
#define LOG_BRIGHTNESSTYPE          2
#define LOG_DETLATYPE_BRIGHTNESS    3
#define LOG_DETLATYPE_TEMPERATURE   4
#define LOG_TEMPERATURETYPE         5
#define LOG_TEMPERATURETYPE         5

#define DAYLIGHTBITOFFSET           64 
#define stargeBINDKEYMAXLEN         80     //不能超过FLASH_USERBLOCK_SIZE-10

typedef struct {
    uint16_t lightnessLevel;
    uint16_t lightnessCTL;
    uint8_t  lightingOn;
    uint8_t  fwupgrade;
    uint8_t  powerOnMode;
    uint8_t  offdelayset;
    uint32_t Lightontime;
    uint32_t Lightruntime;
    uint32_t LightUTCtime;
    int8_t   Lightzone;   //timezone在夏令时时段时，其值会+DAYLIGHTBITOFFSET
    uint8_t  bleonly;
    uint8_t  fristpair;
    uint8_t  brightdeltastep;
    uint16_t offdelaytime;
    uint16_t ondelaytime;
} LightConfig_def;

typedef struct {
    uint8_t     bindflag;
    uint8_t     bindkeylen;
    uint8_t     bindkey[stargeBINDKEYMAXLEN];
}storageBindkey_def;

typedef struct{
    union{
        struct{
            uint32_t brightness:7;
            uint32_t time:11;
            uint32_t weekset:7;  
            uint32_t timerenable:1; 
            uint32_t timerepeat:1;   
            uint32_t onoffset:1;    
            uint32_t Aienable:1;  
            uint32_t reserved:3;   
        };
        uint32_t LightTimer;
    };
}LightTimerBit_t;

#define lightAUTOONOFFMAXCNT   20

typedef struct
{
    uint8_t num;
    LightTimerBit_t timerList[lightAUTOONOFFMAXCNT];
} AutoOnOff_Timer_t;

extern storageBindkey_def storageBindkey;
extern AutoOnOff_Timer_t lightAutoOnOffTimer;

extern void StoreConfig(void);
extern void StoreConfigDelay(void);
extern void StoreConfigImmediately(void);
extern wiced_bool_t StoreBindKey(uint8_t *key, uint8_t len);
extern void StoreBindKeyDelay(uint8_t *key, uint8_t len);
extern wiced_bool_t StoreSaveUserTimer(uint8_t *timer, uint8_t len);
extern uint8_t LoadConfig(void);
extern void ResetConfig(void);
extern void ResetToolConfig(void);
extern uint16_t storage_read_sn(uint8_t *data);

