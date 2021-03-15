//******************************************************************************
//*
//* 文 件 名 : 
//* 文件描述 :        
//* 作    者 : 张威/Andon Health CO.Ltd
//* 版    本 : V0.0
//* 日    期 : 
//*
//* 更新历史 : 
//*     日期       作者    版本     描述
//*         
//*          
//******************************************************************************

///*****************************************************************************
///*                         包含文件说明
///*****************************************************************************

#include "systimer.h"
#include "light_model.h"
#include "wiced_timer.h"
#include "storage.h"
#include "vendor.h"
#include "wiced_platform.h"
///*****************************************************************************
///*                         宏定义区
///*****************************************************************************
#define SYSTIMERAPPCBNUM      10

///*****************************************************************************
///*                         Strcut结构变量定义区
///*****************************************************************************

///*****************************************************************************
///*                         函数声明区
///*****************************************************************************

///*****************************************************************************
///*                         常量定义区
///*****************************************************************************
static const uint16_t Month_Day1[]={31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};   //平年
static const uint16_t Month_Day2[]={31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};   //闰年  
static const uint16_t FOURYEARS = (366 + 365 +365 +365); //每个四年的总天数  
static const uint32_t DAYMS = (24*3600);   //每天的秒数
///*****************************************************************************
///*                         外部全局变量定义区
///*****************************************************************************

///*****************************************************************************
///*                         文件内部全局变量定义区
///*****************************************************************************
int8_t   sysTimerTimezone  = 0;
uint32_t SystemUpCounter = 0;
uint32_t SystemUTCtimer  = 0;
systimerClock_t sysTimerClock= {0};
daylightSet_t daylightset = {0};
static wiced_timer_t systimer;
extern uint16_t turnOffTimeCnt;
#if LIGHTAI == configLIGHTAIANDONMODE
extern uint32_t SystemUTCtimer0;
#endif
static systimer_cb_t systimerAppcb[SYSTIMERAPPCBNUM] = { 0 };
///*****************************************************************************
///*                         函数实现区
///*****************************************************************************

//*****************************************************************************
// 函数名称: 
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
void systimer_cb(uint32_t p)
{
    uint16_t i;

    for (i = 0; i < SYSTIMERAPPCBNUM; i++)
    {
        if (systimerAppcb[i] != NULL)
        {
            (*systimerAppcb[i])();
        }
    }
    
    SystemUTCtimer ++;
    SystemUpCounter ++;
#if LIGHTAI == configLIGHTAIANDONMODE
    SystemUTCtimer0 ++;
#endif
}

//*****************************************************************************
// 函数名称: sysTimerGetSystemRunTime
// 函数描述: 获取系统运行的时间，单位s
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
uint32_t sysTimerGetSystemRunTime(void)
{
    return SystemUpCounter;
}

//*****************************************************************************
// 函数名称: sysTimerSetSystemRunTime
// 函数描述: 获取系统运行的时间，单位s
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
void sysTimerSetSystemRunTime(uint32_t settimer)
{
    SystemUpCounter = settimer;
#if LIGHTAI == configLIGHTAIANDONMODE
    SystemUTCtimer0 = settimer;
#endif
}

//*****************************************************************************
// 函数名称: sysTimerSetSystemUTCTime
// 函数描述: 设置UTC时间，单位s
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
void sysTimerSetSystemUTCTime(uint32_t utctime, int8_t timezone, uint8_t daylight)
{
#if LIGHTAI == configLIGHTAIANDONMODE
    if(SystemUTCtimer > systimer_MINSYSTIMECNT){
        if(utctime > SystemUTCtimer) {
            SystemUTCtimer0 += utctime - SystemUTCtimer;
        }else{
            SystemUTCtimer0 -= SystemUTCtimer - utctime;
        }
    }
#endif
    SystemUTCtimer = utctime;
    sysTimerTimezone = timezone;
    daylightset.daylight = daylight;
    LOG_DEBUG("timezone: %d,  daylight %x \n", sysTimerTimezone, daylight);
    // LOG_DEBUG("daylight Start %d m %d d\n", daylightset.startmonth,daylightset.startday);
    // LOG_DEBUG("daylight end %d m %d d\n", daylightset.endmonth,daylightset.endday);
    
}

//*****************************************************************************
// 函数名称: sysTimerGetSystemUTCTime
// 函数描述: 获取UTC时间，单位s
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
uint32_t sysTimerGetSystemUTCTime(void)
{
    return SystemUTCtimer;
}
//*****************************************************************************
// 函数名称: sysTimerGetSystemUTCTime
// 函数描述: 获取UTC时间，单位s
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
int8_t sysTimerGetSystemTimeZone(void)
{
    int8_t timezone;
    timezone = sysTimerTimezone;
    if(daylightset.daylight){
        timezone += DAYLIGHTBITOFFSET;  //时区的范围是-28~24 超过24则表示处于夏令时时段
    }
    return timezone;
}
//*****************************************************************************
// 函数名称: sysTimerGetClockTime
// 函数描述: 获取系统实时时钟，年月日时分秒vendorClock_t
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/

systimerClock_t* sysTimerGetClockTime(void)
{
    //TODO  转化为实时时钟，年月日星期时分秒格式
    uint16_t nDays;
    uint16_t nYear4;
    uint16_t nRemain;
    uint16_t nDesYear;
    uint16_t nDesMonth;
    uint16_t nDesDay;
    uint16_t *pMonths; 
    uint32_t timestamp;
    uint16_t daylightstartdays;
    uint16_t daylightenddays;
    
    if((sysTimerTimezone>24) || (sysTimerTimezone<-28))
    {
        sysTimerTimezone = 0;
    }
    
    //需要注意 上电时未同步时间前，时间不准确
    timestamp = SystemUTCtimer + (sysTimerTimezone/2)*3600  + (sysTimerTimezone%2)*1800 + 1000000000;
    if(daylightset.daylight)  //夏令时段
    {
        timestamp = timestamp + 3600;
    }

	nDays = timestamp/DAYMS + 1;    //time函数获取的是从1970年以来的秒数，因此需要先得到天数  
	nYear4 = nDays/FOURYEARS;   //得到从1970年以来的周期（4年）的次数  1970年开始为平年，1971年为平年，1972年为闰年，1973年为平年
	nRemain = nDays%FOURYEARS;  //得到不足一个周期的天数  
	nDesYear = 1970 + nYear4*4;  
	nDesMonth = 0, 
	nDesDay = 0;  
	pMonths = (uint16_t *)(Month_Day1);  
	
	if ( nRemain<365 )//一个周期内，第一年  
	{//平年  
	     
	}  
	else if ( nRemain<(365+365) )//一个周期内，第二年  
	{//平年  
	    nDesYear += 1;  
	    nRemain -= 365;  
	}  
	else if ( nRemain<(365+365+366) )//一个周期内，第三年  
	{//润年  
	    nDesYear += 2;  
	    nRemain -= (365+365);
	    pMonths = (uint16_t *)(Month_Day2);  
	}  
	else//一个周期内，第四年，这一年是平年  
	{//平年  
	    nDesYear += 3;  
	    nRemain -= (365+365+366);  
	}

	for(uint16_t i=0; i<12; i++)
	{
	    nDesMonth = i+1;
        nDesDay = nRemain;
	    if(nRemain <= pMonths[i]){
	        break;
	    }
	    nRemain = nRemain - pMonths[i];
	}
    


    timestamp = timestamp%DAYMS;
    if(nDesYear<2000){
        nDesYear = 2000;
    }
    sysTimerClock.nYear   = (uint8_t)(nDesYear-2000);
    sysTimerClock.nMonth  = (uint8_t)(nDesMonth);
    sysTimerClock.nDay    = (uint8_t)(nDesDay);
    sysTimerClock.nHour   = (uint8_t)(timestamp/3600);
    sysTimerClock.nMinute = (uint8_t)((timestamp%3600)/60);
    sysTimerClock.nSecond = (uint8_t)(timestamp%60);
    sysTimerClock.nWeek   = (uint8_t)((nDays+4-1)%7);
    if(0 == sysTimerClock.nWeek){
        sysTimerClock.nWeek = 7;
    }
    return &sysTimerClock;
}

//*****************************************************************************
// 函数名称: systimerAddAppTimer
// 函数描述: 添加1s的定时回调函数
// 函数输入:  
// 函数返回值: 成功返回WICED_TRUE 失败返回WICED_FALSE(失败是由于无空间添加新的回调)
//*****************************************************************************/
wiced_bool_t systimerAddAppTimer(systimer_cb_t app_timer_cb)
{
    int i;

    // First check whetehr the timer has been enabled already
    for (i = 0; i < SYSTIMERAPPCBNUM; i++)
    {
        if (systimerAppcb[i] == app_timer_cb)
        {
            return WICED_TRUE;
        }
    }

    // The timer not enabled yet, enable it now
    for (i = 0; i < SYSTIMERAPPCBNUM; i++)
    {
        if (systimerAppcb[i] == NULL)
        {
            systimerAppcb[i] = app_timer_cb;
            return WICED_TRUE;
        }
    }
    return WICED_FALSE;
}

//*****************************************************************************
// 函数名称: systimerAddRemoveTimer
// 函数描述: 移除1s定时回调
// 函数输入:  
// 函数返回值: 成功返回WICED_TRUE 失败返回WICED_FALSE(失败是由于回调未添加到系统时间定时回调中)
//*****************************************************************************/
wiced_bool_t systimerAddRemoveTimer(systimer_cb_t app_timer_cb)
{
    int i;
    for (i = 0; i < SYSTIMERAPPCBNUM; i++)
    {
        if (systimerAppcb[i] == app_timer_cb)
        {
            systimerAppcb[i] = NULL;
            return WICED_TRUE;
        }
    }
    return WICED_FALSE;
}

void sysTimerInit(void)
{
    int i;
    if (!wiced_is_timer_in_use(&systimer))
    {
        wiced_init_timer(&systimer, systimer_cb, 0, WICED_MILLI_SECONDS_PERIODIC_TIMER);
        wiced_start_timer(&systimer, 1000);
    }
    for (i = 0; i < SYSTIMERAPPCBNUM; i++)
    {
        systimerAppcb[i] = NULL;
    }
}