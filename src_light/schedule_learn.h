#pragma once

#include "stdint.h"
#include "config.h"

///*****************************************************************************
///*                         宏定义区
///*****************************************************************************
#define CENTERLENMAX   22  //=LIGHT_AUTOBRIGHTNESSMAXNUM(=20)+2 //144 //lq20200420最大模型长度
#define BUFLOGMAXSIZE   3  //lq20200420 BufLog的最大个数
#define FILTVALUE  0   //10  //1 //  //时间滤波窗口设置为10min
#define BRIGHTFILTER 15 //10 #中心模型亮度平滑参数，后1点与前1点，亮度偏差+-15以内，则前后两点取积分平均，删除后1点
#define PARTWEIGHT0  1   //center、Brightnew的权重比例[1:1],PARTWEIGHT0、PARTWEIGHT1<10
#define PARTWEIGHT1  9   //lq20200602
#define LIGHT_AUTOBRIGHTNESSMAXNUM    20 //lq20200616

#define BRIGHT_TIMER_LENGTH   60   //lq20200622 定时器 1min(600s)
#define BRIGHTDURATIONMIN 10  //1  //lq20200622 定时器 10min
#define BRIGHTWILLLENMIN 10 //lq20200622 定时器 10min
#define BRIGHTCHANGEMIN 15   //lq20200622 定时器 
#define BRIGHTGRADUAL_LENGTH  50 //lq20200622 定时器  50ms
#define PERIODVALUE 200 //20  //lq20200622 定时器  20周期
///*****************************************************************************
///*                         Strcut结构变量定义区
///*****************************************************************************
typedef union 
{
    struct AutoBrightness
    {
        uint8_t  AutoBrightnessNum;
        uint8_t  AutoBrightnessSetSave;
        union
        {
            struct{
                uint8_t  FlagModelOn:1;      //亮度模型使能标志位，初始化=0
                uint8_t  FlagAutoAdjustOn:1; //开灯时亮度自动调整，初始化=0
                uint8_t  Flagreserved:6;     //保留
            };
            uint8_t Flag;
        };
        //uint8_t  AutoBrightnessSet[LIGHT_AUTOBRIGHTNESSMAXNUM][2];
        uint16_t  AutoBrightnessPoint[LIGHT_AUTOBRIGHTNESSMAXNUM][2];//lq20200618 精度1min时
        //lq20200420亮度模型学习-------------
        //uint8_t Tbulb2;  //下载新模型时,在center坐标轴上的当前时间，经app计算获得，精度10min,最大144
        uint16_t Tbulb2;  //lq20200618 精度1min时
        //uint8_t Tbulb1; //上传备份模型时，在center坐标轴上的当前时间，精度10min
        uint16_t Tbulb1;  //lq20200618 精度1min时
        uint8_t  flagdayone;
        //----------------------------------
    }Item;
    uint8_t array[sizeof(struct AutoBrightness)];
}AutoBrightnessSet_def;

typedef struct {
    uint16_t modeltime;               //模型时间，=0-1439
    uint8_t BrightState;              //亮度，=0-100
} BufData_def;                         //存储亮度学习Log的Buffer结构体定义

///*****************************************************************************
///*                         变量定义区
///*****************************************************************************
extern int AxisDis; //有正负(-1439~1439), center坐标轴(0-1439)和SystemUTCtimer0坐标轴(0-1439)之间的系统时间偏差，单位min
extern uint16_t modeltimenew;               //当前点模型时间，=0-1439
extern uint8_t  BrightStatenew;              //当前点亮度，=0-100     
extern uint8_t centerlen;   //“中心”实际长度  
extern uint8_t bufferlog_optr; //=0\1\2 
extern uint8_t flagdayone;  // 第一天数据的标志位, 初始化为第1天
extern uint8_t Flagnewmodel; //下载模型时标志位，新下载时=1
extern uint32_t SystemUTCtimer0;

extern BufData_def center[CENTERLENMAX];
extern BufData_def bufferlog[BUFLOGMAXSIZE];
extern AutoBrightnessSet_def AutoBrightnessSet;
extern const AutoBrightnessSet_def DEFAULT_AUTOLIGHTNESS_CONFIG; 

extern wiced_timer_t BrightCheckTimer;//lq20200622 定时器

///*****************************************************************************
///*                         函数声明区
///*****************************************************************************
//extern void BrightModelLearning(void); 
extern void BrightModelLearning(uint32_t SystemTnew, uint8_t Bnew);
//-------------------------------------------
extern uint8_t AutoAdjustBrightness(void);
extern uint8_t GetCenter(uint8_t *reply,uint8_t *p_data, uint16_t data_len);//lq20200617 新增 
extern uint8_t SetCenter(uint8_t *reply, uint8_t *p_data, uint16_t data_len);//lq20200617 新增
extern uint16_t Calmodeltimenew(uint32_t SystemUTCtimer0); //lq20200616 新增
extern void LearnBufInitial(void);//lq20200616 新增
extern void BrightCheckChange(uint32_t); //lq20200622 定时器