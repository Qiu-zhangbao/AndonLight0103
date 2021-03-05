#pragma once

#include "stdint.h"
#include "config.h"
#include "storage.h"


typedef struct
{
    uint8_t accuracy : 2;
    uint8_t period : 6;
} transition_time_t;


typedef struct
{
    uint8_t onoff;                                         //目标状态
    uint8_t flag;                                          //运行状态，DELAYRUNING  DELAYSTOP
    uint16_t settime;                                      //剩余时间,为0表示为处于倒计时过程
    uint16_t remaintime;                                   //剩余时间,为0表示为处于倒计时过程
} TurnOnOffDelay_t;

typedef struct 
{
    uint16_t flash_first;                                   //闪烁时是先亮还是先灭
    uint16_t flash_fristtime;                               //闪烁时第一个状态的持续时间
    uint16_t flash_cycle;                                   //单次闪烁的时长
    uint16_t flash_times;                                   //一次循环时闪烁的次数
    uint16_t sniffercycle;                                  //一次呼吸的总时长
    uint16_t sniffer_frist;                                 //呼吸时是先亮还是先灭
    uint16_t sniffer_times;                                 //一次循环中呼吸的次数
    uint16_t times;                                         //总的循环次数
}Light_FlashAndSniffer_t;

extern LightConfig_def currentCfg;

#define LIGHT_ON 1
#define LIGHT_OFF 0

extern uint8_t transition_time;
extern LightConfig_def LightConfig;

//one tick=10ms
typedef int32_t (*Animination)(int32_t tick, int32_t period, int32_t initiate, int32_t final);

void lightSetDelayOnOffTimer(uint8_t onoff, uint16_t delaytime);
void lightGetDelayOnOffTimer(uint8_t *onoff, uint16_t *delaytime, uint8_t *lightness);
void lightGetStatus(uint8_t *current, uint8_t *targe, uint16_t *transtime);
void lightCancleAction(uint32_t);
void lightStartAction(uint32_t);

extern void LightStore(void);
extern void LightLoad(void);
extern void LightUpdate(void);
extern void LightModelInitial(uint8_t onoff);
extern void LightFlash(uint16_t cycle, uint16_t times,uint8_t flashbrightness, uint8_t finalbrightness,uint8_t);
extern void LightSniffer(uint16_t cycle, uint16_t times, uint8_t direction,uint8_t finalbrightness,uint8_t);
extern void LightModelToggleForPowerOff(uint8_t transitiontime, uint16_t delay, uint16_t Powerstata);
// extern void LightFlashAndSniffer(uint16_t flashcycle, uint16_t times1, uint16_t sniffercycle,uint16_t times2, uint16_t times);
extern void LightFlashAndSniffer(Light_FlashAndSniffer_t);


extern void LightModelSetBrightness(int8_t percetange, uint8_t transitiontime, uint16_t delay);
extern void LightModelDeltaBrightness(int8_t delta, uint8_t transitiontime, uint16_t delay);
extern void LightModelTurn(int8_t onoff, uint8_t transition, uint16_t delay);
extern void LightModelToggle(int8_t reserved, uint8_t transitiontime, uint16_t delay);  //添加reserved 使得灯动作函数参数格式统一：动作，变化时长，延时
extern void LightModelTurnToggle(int8_t res, uint8_t transitiontime, uint16_t delay);
extern void LightModelTurnOn(int8_t reserved, uint8_t transitiontime, uint16_t delay);
extern void LightModelTurnOff(int8_t reserved, uint8_t transitiontime, uint16_t delay);
extern void LightModelTurnOffDelay(int8_t reserved, uint8_t transitiontime, uint16_t delay);
extern void LightModelMorseCodeDisplay(int8_t res,uint8_t transitiontime, uint16_t delay);


