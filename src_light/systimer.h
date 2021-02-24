//******************************************************************************
//*
//* 文 件 名 : 
//* 文件描述 :        
//* 作    者 : 张威/Andon Health CO.Ltd
//* 版    本 : V0.0
//* 日    期 : 
//******************************************************************************
#ifndef LIGHT_SYSCLOCK_H
#define LIGHT_SYSCLOCK_H
///*****************************************************************************
///*                         包含文件说明
///*****************************************************************************
#include "stdint.h"
#include "config.h"

///*****************************************************************************
///*                         宏定义区
///*****************************************************************************
#define systimer_MINSYSTIMECNT    0xFA78F00
///*****************************************************************************
///*                         Strcut结构变量定义区
///*****************************************************************************
typedef struct
{
    uint8_t nYear;
    uint8_t nMonth;
    uint8_t nDay;
    uint8_t nWeek;
    uint8_t nHour;
    uint8_t nMinute;
    uint8_t nSecond;
}systimerClock_t;

typedef struct{
    union{
        struct{
            uint32_t startday:5;
            uint32_t startmonth:4;
            uint32_t endday:5;
            uint32_t endmonth:4;  
            uint32_t daylighenable:1;  
            uint32_t reserved:13;  
        };
        uint32_t daylight;
    };
}daylightSet_t;

typedef void (*systimer_cb_t)(void);
///*****************************************************************************
///*                         外部全局常量声明区
///*****************************************************************************

///*****************************************************************************
///*                         外部全局变量声明区
///*****************************************************************************

///*****************************************************************************
///*                         函数声明区
///*****************************************************************************
extern void sysTimerInit(void);
extern uint32_t sysTimerGetSystemRunTime(void);
extern void sysTimerSetSystemRunTime(uint32_t settimer);
extern uint32_t sysTimerGetSystemUTCTime(void);
extern int8_t sysTimerGetSystemTimeZone(void);
extern systimerClock_t* sysTimerGetClockTime(void);
extern void sysTimerSetSystemUTCTime(uint32_t utctime, int8_t timezone,uint8_t);
wiced_bool_t systimerAddRemoveTimer(systimer_cb_t app_timer_cb);
wiced_bool_t systimerAddAppTimer(systimer_cb_t app_timer_cb);

#endif

