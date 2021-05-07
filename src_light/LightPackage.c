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
#include <stdio.h>
#include "mylib.h"
#include "wiced_memory.h"
#include "LightPackage.h"
#include "light_model.h"
#include "storage.h"
#include "config.h"
#include "llog.h"
#include "xxtea_F.h"
#include "wyzebase64.h"
#include "wyze_md5.h"
#if LIGHTAI == configLIGHTAIANDONMODE
#include "schedule_learn.h"
#endif
#include "systimer.h"
#include "wiced_hal_rand.h"
#include "src_provisioner\mesh_provisioner_node.h"
#include "adv_pack.h"

///*****************************************************************************
///*                         宏定义区
///*****************************************************************************
#define _countof(array) (sizeof(array) / sizeof(array[0]))

#define ACTION_GET                       (0x01)
#define ACTION_SET                       (0x02)
#define ACTION_DELTA                     (0x03)
#define ACTION_TOGGLE                    (0x04)
#define ACTION_START                     (0x05)
#define ACTION_STATUS                    (0x06)
#define ACTION_FLASH                     (0x07)



///*****************************************************************************
///*                         Strcut结构变量定义区
///*****************************************************************************
typedef packageReply (*VendorActionGet)(uint8_t *p_data, uint16_t data_len);
typedef packageReply (*VendorActionSet)(uint8_t *p_data, uint16_t data_len);
typedef packageReply (*VendorActionDelta)(uint8_t *p_data, uint16_t data_len);
typedef packageReply (*VendorActionToggle)(uint8_t *p_data, uint16_t data_len);
typedef packageReply (*VendorActionStart)(uint8_t *p_data, uint16_t data_len);
typedef packageReply (*VendorActionStatus)(uint8_t *p_data, uint16_t data_len);

typedef struct resource_t
{
    uint8_t ResourceId;
    VendorActionGet Get;
    VendorActionSet Set;
    VendorActionDelta Delta;
    VendorActionToggle Toggle;
    VendorActionStart Start;
    VendorActionStatus Status;
    VendorActionStatus Flash;
}Resource;

typedef enum{
    enumREMOTEACTION_NULL = 0,
    enumREMOTEACTION_SHORTPRESS,
    enumREMOTEACTION_LONGPRESS,
    enumREMOTEACTION_TURN,
    enumREMOTEACTION_PRESSTURN,
    enumREMOTEACTION_DOUBLECLICK,
    enumREMOTEACTION_THREECLICK,
    enumREMOTEACTION_MAX,
}enumREMOTEACTION_t;

typedef enum{
    enumREMOTEFUN_NULL = 0,
    enumREMOTEFUN_TOGGLE,
    enumREMOTEFUN_ON,
    enumREMOTEFUN_OFF,
    enumREMOTEFUN_DELTABRIGHTNESS,
    enumREMOTEFUN_DELTAANGLE,
    enumREMOTEFUN_TURNOFFDELAY,
    enumREMOTEFUN_MORSECODE,
    enumREMOTEFUN_MAX,
}enumRESPONSEFUN_t;

typedef struct 
{
    enumREMOTEACTION_t ActionId;
    enumRESPONSEFUN_t  FunctionId;
}LightActionList_t;

typedef void (*ResponseFun_t)(int8_t,uint8_t,uint16_t);

///*****************************************************************************
///*                         函数声明区
///*****************************************************************************
static packageReply  OnOffGet(uint8_t *p_data, uint16_t data_len);
static packageReply  OnOffSet(uint8_t *p_data, uint16_t data_len);
static packageReply  OnOffToggle(uint8_t *p_data, uint16_t data_len);
static packageReply  OnOffFlash(uint8_t *p_data, uint16_t data_len);
static packageReply  BrightnessGet(uint8_t *p_data, uint16_t data_len);
static packageReply  BrightnessSet(uint8_t *p_data, uint16_t data_len);
static packageReply  BrightnessDelta(uint8_t *p_data, uint16_t data_len);
static packageReply  StatusBrightness(uint8_t *p_data, uint16_t data_len);
static packageReply  GetTurnOffDelay(uint8_t *p_data, uint16_t data_len);
static packageReply  SetTurnOffDelay(uint8_t *p_data, uint16_t data_len);
static packageReply  GetTurnOnOffTimer(uint8_t *p_data, uint16_t data_len);
static packageReply  SetTurnOnOffTimer(uint8_t *p_data, uint16_t data_len);
static packageReply  GetCountDownTimer(uint8_t *p_data, uint16_t data_len);
static packageReply  SetCountDownTimer(uint8_t *p_data, uint16_t data_len);
static packageReply  SetSysTime(uint8_t *p_data, uint16_t data_len);
static packageReply  GetSysRunTime(uint8_t *p_data, uint16_t data_len);
static packageReply  GetTokenInfo(uint8_t *p_data, uint16_t data_len);
static packageReply  GetAutoBrightness(uint8_t *p_data, uint16_t data_len);
static packageReply  SetAutoBrightness(uint8_t *p_data, uint16_t data_len);
static packageReply  GetDeivceInfo(uint8_t *p_data, uint16_t data_len);
static packageReply  GetRemoteInfo(uint8_t *p_data, uint16_t data_len);
static packageReply  GetLog(uint8_t *p_data, uint16_t data_len);
static packageReply  SetDeivceBind(uint8_t *p_data, uint16_t data_len);
static packageReply  SetDeivceAuth(uint8_t *p_data, uint16_t data_len);
static packageReply  GetDeivceAuth(uint8_t *p_data, uint16_t data_len);
static packageReply  GetexchangeKey(uint8_t *p_data, uint16_t data_len); 
static packageReply  GetPowerOnStatus(uint8_t *p_data, uint16_t data_len);
static packageReply  SetPowerOnStatus(uint8_t *p_data, uint16_t data_len);
static packageReply  SetRemoteAction(uint8_t *p_data, uint16_t data_len);
static packageReply  SetCancleAction(uint8_t *p_data, uint16_t data_len);
static packageReply  SetStartDevAction(uint8_t *p_data, uint16_t data_len);

static packageReply  SetDeltaStep(uint8_t *p_data, uint16_t data_len);
static packageReply  GetDeltaStep(uint8_t *p_data, uint16_t data_len);
static packageReply  GetRemoteActionResponse(uint8_t *p_data, uint16_t data_len);
static packageReply  SetRemoteActionResponse(uint8_t *p_data, uint16_t data_len);

///*****************************************************************************
///*                         常量定义区
///*****************************************************************************
const Resource resource_list[] = {
    {.ResourceId = 0x01,
     .Get = OnOffGet,
     .Set = OnOffSet,
     .Toggle = OnOffToggle,
     .Flash = OnOffFlash,
    },

    {.ResourceId = 0x02,
     .Get = BrightnessGet,
     .Set = BrightnessSet,
     .Delta = BrightnessDelta,
     .Status = StatusBrightness},
    
    {.ResourceId = 0x04,
     .Get = GetTurnOffDelay,
     .Set = SetTurnOffDelay},

    {.ResourceId = 0x05,
     .Get = GetTurnOnOffTimer,
     .Set = SetTurnOnOffTimer},

    {.ResourceId = 0x06,
     .Get = GetCountDownTimer,
     .Set = SetCountDownTimer},
     

    {.ResourceId = 0x07,
     .Set = SetSysTime},

    {.ResourceId = 0x08,
     .Get = GetSysRunTime},

    {.ResourceId = 0x09,
     .Get = GetTokenInfo},  
    
    {.ResourceId = 0x0A,
     .Get = GetAutoBrightness,
     .Set = SetAutoBrightness,}, 

    {.ResourceId = 0x0B,
     .Set = SetRemoteAction,}, 

    {.ResourceId = 0x0C,
     .Set = SetCancleAction,}, 

    {.ResourceId = 0x0D,
     .Set = SetDeltaStep,
     .Get = GetDeltaStep,}, 
    
    {.ResourceId = 0x0E,
     .Set = SetStartDevAction,}, 

    // {.ResourceId = 30,
    //  .Get = TXPowerGet,
    //  .Set = TXPowerSet},

    // {.ResourceId = 31,
    //  .Set = ClosingFunctionSet,
    //  .Get = ClosingFunctionGet},

    // {.ResourceId = 32,
    //  .Get = GetTTL},

    // {.ResourceId = 33,
    //  .Get = GetRSSI},
    // {
    //     .ResourceId = 0x2E,
    //     .Get = GetRemoteActionResponse,
    //     .Set = SetRemoteActionResponse,
    // },
    {
        .ResourceId = 0x2F,
        .Get = GetPowerOnStatus,
        .Set = SetPowerOnStatus,
    },
    {
        .ResourceId = 0x30,
        .Get = GetDeivceInfo,
    },
    {
        .ResourceId = 0x31,
        .Get = GetRemoteInfo,
    },
    {
        .ResourceId = 0x3D,
        .Set = SetDeivceBind,
    },
    // {
    //     .ResourceId = 0x3E,
    //     .Set = SetDeivceAuth,
    //     .Get = GetDeivceAuth,
    // },
    {
        .ResourceId = 0x42,
        .Get = GetLog,
    }
};

const Resource resourceNoEncrypt_list[] = {
    {.ResourceId = 0x3F,
     .Get = GetexchangeKey,
    },
};

const ResponseFun_t ResponseFunList[] = {
    NULL,
    LightModelTurnToggle,
    LightModelTurnOn,
    LightModelTurnOff,
    LightModelDeltaBrightness,
    LightModelDeltaAngle,
    LightModelTurnOffDelay,
    LightModelMorseCodeDisplay,
};

///*****************************************************************************
///*                         外部全局变量定义区
///*****************************************************************************

///*****************************************************************************
///*                         文件内部全局变量定义区
///*****************************************************************************
uint8_t last_Brightness = 0xFF;
static uint8_t u8lightpackSendCno = 0;
static wiced_bool_t u8lightpackAuthFlag = WICED_FALSE;

LightActionList_t LightActionList[]={
    // { .ActionId = enumREMOTEACTION_NULL,
    //   .ResponseFun = NULL, 
    // },
    { .ActionId = enumREMOTEACTION_SHORTPRESS,
      .FunctionId = enumREMOTEFUN_TOGGLE, 
    },
    { .ActionId = enumREMOTEACTION_LONGPRESS,
      .FunctionId = enumREMOTEFUN_MORSECODE, 
    },
    { .ActionId = enumREMOTEACTION_TURN,
      .FunctionId = enumREMOTEFUN_DELTABRIGHTNESS, 
    },
    { .ActionId = enumREMOTEACTION_PRESSTURN,
      .FunctionId = enumREMOTEFUN_DELTAANGLE, 
    },
    { .ActionId = enumREMOTEACTION_DOUBLECLICK,
      .FunctionId = enumREMOTEFUN_TURNOFFDELAY, 
    },
    { .ActionId = enumREMOTEACTION_THREECLICK,
      .FunctionId = enumREMOTEFUN_NULL, 
    }
};
///*****************************************************************************
///*                         函数实现区
///*****************************************************************************

void obfs(uint8_t *data, uint16_t len)
{
    uint32_t seed = data[1];

    for (int i = 2; i < len; i++)
    {
        seed = 214013 * seed + 2531011;
        data[i] ^= (seed >> 16) & 0xff;
    }
}

uint8_t Auth(uint8_t *data, uint16_t len)
{
    uint8_t result = data[0];

    for (int i = 1; i < len; i++)
    {
        result ^= data[i];
    }

    return result;
}


//*****************************************************************************
// 函数名称: OnOffGet
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  OnOffGet(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;
    uint32_t temp;
    
    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80) 
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(6);
    if(reply.p_data == NULL)
    {
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }
    
    temp = LightConfig.lightnessLevel;
    temp *= 100;
    temp += 32726;
    temp /= 65536;
    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = (p_data[2]|0x80);
    reply.p_data[3] = p_data[3];
    reply.p_data[4] = LightConfig.lightingOn;
    reply.p_data[5] = 0xff & temp;
    reply.pack_len = 6;

    reply.result = lightpackageSUCCESS;
    return reply;
}

//*****************************************************************************
// 函数名称: OnOffSet
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  OnOffSet(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;
    uint32_t temp;
    uint8_t delay = 0;
    uint8_t trans_time = DEFAULT_TRANSITION_TIME;

    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80) 
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply; 
    }
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(6);
    if(reply.p_data == NULL)
    {
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }

    if (data_len > 6)
    {
        delay = p_data[6];
    }
    if (data_len > 5)
    {
        trans_time = p_data[5];
    }

    // LightModelTurn(p_data[4], trans_time, delay); 
    LightModelTurn(p_data[4], 0, delay); 
    temp = LightConfig.lightnessLevel;
    temp *= 100;
    temp += 32726;
    temp /= 65536;
    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = (p_data[2]|0x80);
    reply.p_data[3] = p_data[3];
    reply.p_data[4] = LightConfig.lightingOn;
    reply.p_data[5] = 0xff & temp;
    reply.pack_len = 6;

    reply.result = lightpackageSUCCESS;
    return reply; 
}

//*****************************************************************************
// 函数名称: OnOffToggle
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static uint16_t rec_toggle_cnt = 0;
static packageReply  OnOffToggle(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;
    uint32_t temp;
    uint8_t delay = 0;
    uint8_t trans_time = DEFAULT_TRANSITION_TIME;
    
    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80)
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply; 
    }
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(6);
    if(reply.p_data == NULL)
    {
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }

    if (data_len > 5)
    {
        delay = p_data[5];
    }
    if (data_len > 4)
    {
        trans_time = p_data[4];
    }
    if (data_len > 6)
    {
        rec_toggle_cnt ++;
        LOG_DEBUG("rec msg num: %d total:%d=============\n ",rec_toggle_cnt,p_data[6]);
    }
    LightModelToggle(0,trans_time, delay);
    temp = LightConfig.lightnessLevel;
    temp *= 100;
    temp += 32726;
    temp /= 65536;
    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = (p_data[2]|0x80);
    reply.p_data[3] = p_data[3];
    reply.p_data[4] = LightConfig.lightingOn;
    reply.p_data[5] = 0xff & temp;
    reply.pack_len = 6;

    reply.result = lightpackageSUCCESS;
    return reply; 
}

//*****************************************************************************
// 函数名称: OnOffFlash
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  OnOffFlash(uint8_t *p_data, uint16_t data_len)
{
    extern int32_t transt_to_period(uint8_t t);
    packageReply reply;
    uint32_t temp;
    int16_t flash_timers = 3;
    int16_t flash_cycle = 60;

    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80)
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply; 
    }
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(6);
    if(reply.p_data == NULL)
    {
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }

    if (data_len > 4)
    {
        flash_cycle = p_data[4];
        if((flash_cycle>0xBF) || (0==flash_cycle))
        {
            flash_cycle = 60;
        }
        else
        {
            flash_cycle = transt_to_period(flash_cycle);
        }
    }
    if (data_len > 5)
    {
        flash_timers = p_data[5];
    }
    LightFlash(flash_cycle, flash_timers, 50, 0, 0);
    temp = LightConfig.lightnessLevel;
    temp *= 100;
    temp += 32726;
    temp /= 65536;
    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = (p_data[2]|0x80);
    reply.p_data[3] = p_data[3];
    reply.p_data[4] = LightConfig.lightingOn;
    reply.p_data[5] = 0xff & temp;
    reply.pack_len = 6;
    
    reply.result = lightpackageSUCCESS;
    return reply; 
}

//*****************************************************************************
// 函数名称: BrightnessGet
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  BrightnessGet(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;
    uint8_t current,targe;
    uint16_t transtime;
    
    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80)
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(8);
    if(reply.p_data == NULL)
    {
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }

    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = (p_data[2]|0x80);
    reply.p_data[3] = p_data[3];

    lightGetStatus(&current,&targe,&transtime);
    reply.p_data[4] = current;
    reply.p_data[5] = targe;
    reply.p_data[6] = (transtime>>8)&0xff;
    reply.p_data[7] = transtime&0xff;
    reply.pack_len = 8;

    reply.result = lightpackageSUCCESS;
    return reply;
}

//*****************************************************************************
// 函数名称: BrightnessSet
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  BrightnessSet(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;
    uint8_t delay = 0;
    uint8_t trans_time = DEFAULT_TRANSITION_TIME;
    uint8_t current,targe;
    uint16_t transtime;
    
    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80)
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(8);
    if(reply.p_data == NULL)
    {
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }

    if (data_len > 6)
    {
        delay = p_data[6];
    }
    if (data_len > 5)
    {
        trans_time = p_data[5];
    }
    LightModelSetBrightness(p_data[4], trans_time, delay);

    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = (p_data[2]|0x80);
    reply.p_data[3] = p_data[3];

    lightGetStatus(&current,&targe,&transtime);
    reply.p_data[4] = current;
    reply.p_data[5] = targe;
    reply.p_data[6] = (transtime>>8)&0xff;
    reply.p_data[7] = transtime&0xff;
    reply.pack_len = 8;

    reply.result = lightpackageSUCCESS;
    return reply;
}

//*****************************************************************************
// 函数名称: BrightnessDelta
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  BrightnessDelta(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;
    uint8_t delay = 0;
    uint8_t trans_time = DEFAULT_TRANSITION_TIME;
    uint8_t current,targe;
    uint16_t transtime;
    
    reply.p_data = NULL;
    reply.pack_len = 0;
    rec_toggle_cnt = 0;
    if(p_data[2]&0x80)
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(8);
    if(reply.p_data == NULL)
    {
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }

    if (data_len > 6)
    {
        delay = p_data[6];
    }
    if (data_len > 5)
    {
        trans_time = p_data[5];
    }
    LightModelDeltaBrightness(p_data[4], trans_time, delay);

    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = (p_data[2]|0x80);
    reply.p_data[3] = p_data[3];

    lightGetStatus(&current,&targe,&transtime);
    reply.p_data[4] = current;
    reply.p_data[5] = targe;
    reply.p_data[6] = (transtime>>8)&0xff;
    reply.p_data[7] = transtime&0xff;
    reply.pack_len = 8;

    reply.result = lightpackageSUCCESS;
    return reply;
}

//*****************************************************************************
// 函数名称: BrightnessDelta
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  StatusBrightness(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;
    
    reply.p_data = NULL;
    reply.pack_len = 0;
    
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(data_len);
    if(reply.p_data == NULL)
    {
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }
    
    //由于可能存在的mesh消息转发到GATT上，所以此status也进行了处理
    memcpy(reply.p_data,p_data,data_len);
    reply.result = lightpackageSUCCESS;
    reply.pack_len = data_len;
    return reply;
}

//*****************************************************************************
// 函数名称: SetTurnOffDelay
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  SetCountDownTimer(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;
    uint16_t delaytime;
    uint16_t remaintime;
    
    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80)
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(10);
    if(reply.p_data == NULL)
    {
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }
    delaytime = p_data[5];
    delaytime = (delaytime<<8) + p_data[6];
    
    //TODO 处理App发送的延时开关灯
    lightSetDelayOnOffTimer(p_data[4],delaytime);
    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = (p_data[2]|0x80);
    reply.p_data[3] = p_data[3];
    lightGetDelayOnOffTimer(reply.p_data+4,&delaytime,&remaintime,reply.p_data+9);
    reply.p_data[5] = (delaytime>>8)&0xff;
    reply.p_data[6] =  delaytime&0xff;
    reply.p_data[7] = (remaintime>>8)&0xff;
    reply.p_data[8] =  remaintime&0xff;
    reply.pack_len = 10;
    reply.result = lightpackageSUCCESS;
    return reply;
}
//*****************************************************************************
// 函数名称: GetTurnOffDelay
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  GetCountDownTimer(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;
    uint16_t delaytime;
    uint16_t remaintime;
    
    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80)
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(10);
    if(reply.p_data == NULL)
    {
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }
    //获取当前延时开关灯状态
    lightGetDelayOnOffTimer(reply.p_data+4,&delaytime,&remaintime,reply.p_data+9);
    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = (p_data[2]|0x80);
    reply.p_data[3] = p_data[3];
    reply.p_data[5] = (delaytime>>8)&0xff;
    reply.p_data[6] =  delaytime&0xff;
    reply.p_data[7] = (remaintime>>8)&0xff;
    reply.p_data[8] =  remaintime&0xff;
    reply.pack_len = 10;
    reply.result = lightpackageSUCCESS;
    return reply;
}

//*****************************************************************************
// 函数名称: GetTurnOffDelay
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  GetTurnOffDelay(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;
    
    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80)
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(11);
    if(reply.p_data == NULL)
    {
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }
    
    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = (p_data[2]|0x80);
    reply.p_data[3] = p_data[3];
    reply.p_data[4] = LightConfig.offdelayset;
    LOG_DEBUG("offdelaytime %d\n",LightConfig.offdelaytime);
    LOG_DEBUG("ondelaytime %d\n",LightConfig.ondelaytime);
    reply.p_data[5] = 0x00;
    reply.p_data[6] = (LightConfig.offdelaytime>>8)&0xff; 
    reply.p_data[7] = (LightConfig.offdelaytime)&0xff; 
    reply.p_data[8] = 0x01;
    reply.p_data[9] = (LightConfig.ondelaytime>>8)&0xff; 
    reply.p_data[10] = (LightConfig.ondelaytime)&0xff;
    reply.pack_len = 11;
    reply.result = lightpackageSUCCESS;
    return reply;
}

//*****************************************************************************
// 函数名称: SetTurnOffDelay
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  SetTurnOffDelay(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;
    uint16_t     temp;
    
    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80)
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(11);
    if(reply.p_data == NULL)
    {
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }
    
    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = (p_data[2]|0x80);
    reply.p_data[3] = p_data[3];
    reply.p_data[4] = p_data[4]; 
    LightConfig.offdelayset = p_data[4];
    for(uint16_t i=0; i< data_len-5; )
    {
        if(p_data[i+5] == 0x00){
            temp = p_data[i+6];
            temp = temp*256 + p_data[i+7];
            i+=3;
            LightConfig.offdelaytime = temp;
        }else if(p_data[i+5] == 0x01){
            temp = p_data[i+6];
            temp = temp*256 + p_data[i+7];
            i+=3;
            LightConfig.ondelaytime = temp;
        }
    }
    LOG_DEBUG("offdelaytime %d\n",LightConfig.offdelaytime);
    LOG_DEBUG("ondelaytime %d\n",LightConfig.ondelaytime);
    reply.p_data[5] = 0x00;
    reply.p_data[6] = (LightConfig.offdelaytime>>8)&0xff; 
    reply.p_data[7] = (LightConfig.offdelaytime)&0xff; 
    reply.p_data[8] = 0x01;
    reply.p_data[9] = (LightConfig.ondelaytime>>8)&0xff; 
    reply.p_data[10] = (LightConfig.ondelaytime)&0xff; 
    reply.pack_len = 11;
    llogsaveflag = 1;
    StoreConfig();
    reply.result = lightpackageSUCCESS;
    return reply;
}
//*****************************************************************************
// 函数名称: SetTurnOffDelay
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  GetTurnOnOffTimer(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;
    uint32_t temp;
    
    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80){
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }

    reply.p_data = (uint8_t *)wiced_bt_get_buffer(sizeof(AutoOnOff_Timer_t)+4+1);
    if(reply.p_data == NULL){
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }
    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = (p_data[2]|0x80);
    reply.p_data[3] = p_data[3];
    reply.p_data[4] = lightAutoOnOffTimer.num;
    reply.p_data[5] = lightAUTOONOFFMAXCNT;
    for(uint8_t i=0; i<lightAutoOnOffTimer.num; i++){
        temp = lightAutoOnOffTimer.timerList[i].LightTimer;
        reply.p_data[6+i*4] = (temp>>24)&0xFF;
        reply.p_data[7+i*4] = (temp>>16)&0xFF;
        reply.p_data[8+i*4] = (temp>>8)&0xFF;
        reply.p_data[9+i*4] = temp&0xFF;
    }
    // reply.p_data[4] = lightAutoOnOffTimer.num;
    // reply.pack_len = lightAutoOnOffTimer.num*4;
    // memcpy(reply.p_data+5,lightAutoOnOffTimer.timerList,reply.pack_len);
    // reply.pack_len += 5;
    reply.pack_len = 4 + 2 + lightAutoOnOffTimer.num*4;
    reply.result = lightpackageSUCCESS;
    return reply;
}
//*****************************************************************************
// 函数名称: SetTurnOffDelay
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  SetTurnOnOffTimer(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;
    uint32_t temp;
    
    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80){
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(sizeof(AutoOnOff_Timer_t)+4+1);
    if(reply.p_data == NULL){
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }
    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = (p_data[2]|0x80);
    reply.p_data[3] = p_data[3];
    lightAutoOnOffTimer.num = p_data[4];
    if(lightAutoOnOffTimer.num > lightAUTOONOFFMAXCNT){
        lightAutoOnOffTimer.num = lightAUTOONOFFMAXCNT;
    }
    for(uint8_t i=0; i<lightAutoOnOffTimer.num; i++){
        temp = p_data[5+i*4];
        temp = (temp<<8) + p_data[6+i*4];
        temp = (temp<<8) + p_data[7+i*4];
        temp = (temp<<8) + p_data[8+i*4];
        lightAutoOnOffTimer.timerList[i].LightTimer = temp;
    }
    reply.p_data[4] = lightAutoOnOffTimer.num;
    reply.p_data[5] = lightAUTOONOFFMAXCNT;
    for(uint8_t i=0; i<lightAutoOnOffTimer.num; i++){
        temp = lightAutoOnOffTimer.timerList[i].LightTimer;
        reply.p_data[6+i*4] = (temp>>24)&0xFF;
        reply.p_data[7+i*4] = (temp>>16)&0xFF;
        reply.p_data[8+i*4] = (temp>>8)&0xFF;
        reply.p_data[9+i*4] = temp&0xFF;
        LOG_DEBUG("timer %d:   brightness=%d,  time=%dmin  on=%d   weekset=%02x  r=%d    enable=%d\n",
                  i+1, lightAutoOnOffTimer.timerList[i].brightness,lightAutoOnOffTimer.timerList[i].time,
                  lightAutoOnOffTimer.timerList[i].onoffset,lightAutoOnOffTimer.timerList[i].weekset,
                  lightAutoOnOffTimer.timerList[i].timerepeat,lightAutoOnOffTimer.timerList[i].timerenable);
    }
    if(p_data[4] > lightAUTOONOFFMAXCNT){
        reply.p_data[4] |= 0x80;
    }
    reply.pack_len = 4 + 2 + lightAutoOnOffTimer.num*4;
    StoreSaveUserTimer((uint8_t*)(&lightAutoOnOffTimer),sizeof(AutoOnOff_Timer_t));
    //TODO  保存定时开关灯信息到存储器
    reply.result = lightpackageSUCCESS;
    return reply;
}
//*****************************************************************************
// 函数名称: SetTurnOffDelay
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  SetSysTime(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;
    uint32_t utc_time = 0;
    
    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80) 
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(4);
    if(reply.p_data == NULL)
    {
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }

    utc_time = p_data[4];
    utc_time = utc_time * 256 + p_data[5];
    utc_time = utc_time * 256 + p_data[6];
    utc_time = utc_time * 256 + p_data[7];
    //数据包长度小于9 则数据包中不带时区
    if(data_len < 9)
    {
        sysTimerSetSystemUTCTime(utc_time,0,0);
    }
    else if(data_len < 10)
    {
        sysTimerSetSystemUTCTime(utc_time,p_data[8],0);
    }
    else
    {
        sysTimerSetSystemUTCTime(utc_time,p_data[8],p_data[9]);
    }
    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = (p_data[2]|0x80);
    reply.p_data[3] = p_data[3];
    
    reply.pack_len = 4;
    reply.result = lightpackageSUCCESS;
    llogsaveflag = 1;
    StoreConfig();
    return reply;
}
//*****************************************************************************
// 函数名称: GetSysRunTime
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  GetSysRunTime(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;
    uint32_t lightontime = 0;
    
    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80)
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(7);
    if(reply.p_data == NULL)
    {
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }

    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = (p_data[2]|0x80);
    reply.p_data[3] = p_data[3];
    
    lightontime = LightConfig.Lightontime/60;
    reply.p_data[4] = (lightontime>>16)&0xFF;
    reply.p_data[5] = (lightontime>>8)&0xFF;
    reply.p_data[6] = (lightontime)&0xFF;
    reply.pack_len = 7;
    reply.result = lightpackageSUCCESS;
    return reply;
}
//*****************************************************************************
// 函数名称: GetTokenInfo
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  GetTokenInfo(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;
    
    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80)
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(FLASH_XXTEA_SN_LEN+4);
    if(reply.p_data == NULL)
    {
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }
    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = (p_data[2]|0x80);
    reply.p_data[3] = p_data[3];
    reply.pack_len = storage_read_sn( reply.p_data+4 );
    obfs(reply.p_data+3,reply.pack_len+1);
    reply.pack_len += 4;
    reply.result = lightpackageSUCCESS;
    return reply;
}
//*****************************************************************************
// 函数名称: GetAutoBrightness
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  GetAutoBrightness(uint8_t *p_data, uint16_t data_len)
{
    // wiced_bt_mesh_event_t *p_event_rep;
    packageReply reply;

    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80)
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }
#if LIGHTAI == configLIGHTAIANDONMODE
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(sizeof(AutoBrightnessSet_def)+4+4); //增加4个字节的头长度+开关设置的长度
#else
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(4);
#endif
    if(reply.p_data == NULL)
    {
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }
    reply.p_data [0] = 0;
    reply.p_data [1] = p_data[1];
    reply.p_data [2] = (p_data[2]|0x80);
    reply.p_data [3] = p_data[3];
#if LIGHTAI == configLIGHTAIANDONMODE
    reply.pack_len = GetCenter(reply.p_data  + 4, p_data+4, data_len-4) + 4;
#endif
    reply.result = lightpackageSUCCESS;
    return reply;
}

//*****************************************************************************
// 函数名称: SetAutoBrightness
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  SetAutoBrightness(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;

    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80)
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }
#if LIGHTAI == configLIGHTAIANDONMODE
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(sizeof(AutoBrightnessSet_def)+4+4);
#else
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(4);
#endif
    if(reply.p_data == NULL)
    {
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }
    reply.p_data [0] = 0;
    reply.p_data [1] = p_data[1];
    reply.p_data [2] = (p_data[2]|0x80);
    reply.p_data [3] = p_data[3];
#if LIGHTAI == configLIGHTAIANDONMODE
    reply.pack_len  =  SetCenter(reply.p_data+4, p_data+4, data_len-4) + 4; 
#endif
    reply.result = lightpackageSUCCESS;
    llogsaveflag = 1;
    StoreConfig();
    return reply;
}
//*****************************************************************************
// 函数名称: GetDeivceInfo
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  GetDeivceInfo(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;

    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80)
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(12);
    if(reply.p_data == NULL)
    {
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }
    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = p_data[2]|0x80;
    reply.p_data[3] = p_data[3];
    reply.p_data[4] = (MESH_CID>>8);
    reply.p_data[5] = (MESH_CID&0xFF);
    reply.p_data[6] = (MESH_PID>>8);
    reply.p_data[7] = (MESH_PID&0xFF);
    reply.p_data[8] = (MESH_VID>>8);
    reply.p_data[9] = (MESH_VID&0xFF);
    reply.p_data[10] = (MESH_FWID>>8)&0xFF;
    reply.p_data[11] = (MESH_FWID&0xFF);
    reply.pack_len = 12;
    reply.result = lightpackageSUCCESS;
    return reply;
}

//*****************************************************************************
// 函数名称: GetDeivceInfo
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  GetRemoteInfo(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;

    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80)
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(sizeof(wiced_bt_device_address_t)*REMOTE_MAX_NUM+5);
    if(reply.p_data == NULL)
    {
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }
    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = p_data[2]|0x80;
    reply.p_data[3] = p_data[3];
    reply.p_data[4] = 0;
    reply.pack_len  = adv_packReadRemoteMac(reply.p_data+5)+5;
    return reply;   
}


//*****************************************************************************
// 函数名称: SetRemoteAction
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  SetRemoteAction(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;
    uint8_t      i = 0;

    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80)
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(4);
    if(reply.p_data == NULL){
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }

    for(i=0; i<_countof(LightActionList); i++){
        LOG_DEBUG("LightActionList %d = %d \n", i, LightActionList[i].ActionId);
        if(LightActionList[i].ActionId == p_data[4]){
            break;
        }
    }
    if((i < _countof(ResponseFunList)) && (LightActionList[i].FunctionId != enumREMOTEFUN_NULL)){
        
        LOG_DEBUG("LightActionList %d = %d \n", i, LightActionList[i].FunctionId);
        if(data_len > 7){
            ResponseFunList[LightActionList[i].FunctionId](p_data[5],p_data[6],p_data[7]);
        }else{
            ResponseFunList[LightActionList[i].FunctionId](p_data[5],p_data[6],0);
        }
    }
    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = p_data[2]|0x80;
    reply.p_data[3] = p_data[3];
    reply.pack_len = 4;
    return reply;
}
//*****************************************************************************
// 函数名称: GetRemoteActionResponse
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  SetCancleAction(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;
    uint32_t     temp = 0;

    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80)
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(8);
    if(reply.p_data == NULL){
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }
    temp = p_data[4];
    temp = temp*256 + p_data[5];
    temp = temp*256 + p_data[6];
    temp = temp*256 + p_data[7];
    lightCancleAction(temp);
    memcpy(reply.p_data, p_data, 8);
    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = p_data[2]|0x80;
    reply.p_data[3] = p_data[3];
    reply.pack_len = 8;
    return reply;
}

//*****************************************************************************
// 函数名称: SetStartDevAction
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  SetStartDevAction(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;
    uint32_t     temp = 0;

    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80)
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(8);
    if(reply.p_data == NULL){
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }
    temp = p_data[4];
    temp = temp*256 + p_data[5];
    temp = temp*256 + p_data[6];
    temp = temp*256 + p_data[7];
    lightStartAction(temp);
    memcpy(reply.p_data, p_data, 8);
    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = p_data[2]|0x80;
    reply.p_data[3] = p_data[3];
    reply.pack_len = 8;
    return reply;
}
//*****************************************************************************
// 函数名称: SetDeltaStep
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  SetDeltaStep(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;

    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80)
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(6);
    if(reply.p_data == NULL){
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }
    if(p_data[4] == 1){
        LightConfig.brightdeltastep = p_data[5]; 
    }
    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = p_data[2]|0x80;
    reply.p_data[3] = p_data[3];
    reply.p_data[4] = p_data[4];
    reply.p_data[5] = LightConfig.brightdeltastep;
    reply.pack_len = 6;
    StoreConfig();
    return reply;
}

//*****************************************************************************
// 函数名称: SetDeltaStep
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  GetDeltaStep(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;

    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80)
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(6);
    if(reply.p_data == NULL){
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }
    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = p_data[2]|0x80;
    reply.p_data[3] = p_data[3];
    reply.p_data[4] = 1;
    reply.p_data[5] = LightConfig.brightdeltastep;
    reply.pack_len = 6;
    return reply;
}

//*****************************************************************************
// 函数名称: GetRemoteActionResponse
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  GetRemoteActionResponse(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;

    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80)
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }
    reply.pack_len = _countof(LightActionList)*2+4+1;
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(reply.pack_len);
    if(reply.p_data == NULL){
        reply.pack_len = 0;
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }
    for(uint8_t i=0; i<_countof(LightActionList); i++){
        reply.p_data[i*2+5] = LightActionList[i].ActionId;
        reply.p_data[i*2+5+1] = LightActionList[i].FunctionId;
    }
    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = p_data[2]|0x80;
    reply.p_data[3] = p_data[3];
    reply.p_data[4] = _countof(LightActionList);
    reply.result = lightpackageSUCCESS;
    return reply;
}


//*****************************************************************************
// 函数名称: SetRemoteActionResponse
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  SetRemoteActionResponse(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;
    uint8_t temp;

    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80)
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }
    reply.pack_len = _countof(LightActionList)*2+4+1;
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(reply.pack_len);
    if(reply.p_data == NULL){
        reply.pack_len = 0;
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }
    for(uint8_t i=0; i<p_data[4]; i++){
        temp = p_data[5+2*i+1];
        if(( p_data[5+2*i] < _countof(LightActionList)) && (temp < enumREMOTEFUN_MAX)){
            LightActionList[temp].ActionId = p_data[5+2*i];
            LightActionList[i].FunctionId  = temp;
        }
    }
    for(uint8_t i=0; i<_countof(LightActionList); i++){
        reply.p_data[i*2+5] = LightActionList[i].ActionId;
        reply.p_data[i*2+5+1] = LightActionList[i].FunctionId;
        LOG_DEBUG("LightActionList %d = %d \n", LightActionList[i].ActionId, LightActionList[i].FunctionId);
    }
    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = p_data[2]|0x80;
    reply.p_data[3] = p_data[3];
    reply.p_data[4] = p_data[4];
    reply.result = lightpackageSUCCESS;
    return reply;
}

//*****************************************************************************
// 函数名称: GetPowerOnStatus
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  GetPowerOnStatus(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;

    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80)
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(6);
    if(reply.p_data == NULL)
    {
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }
    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = p_data[2]|0x80;
    reply.p_data[3] = p_data[3];
    reply.p_data[4] = LightConfig.powerOnMode;
    reply.pack_len = 5;
    reply.result = lightpackageSUCCESS;
    return reply;
}

//*****************************************************************************
// 函数名称: SetPowerOnStatus
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  SetPowerOnStatus(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;

    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80)
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(6);
    if(reply.p_data == NULL)
    {
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }
    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = p_data[2]|0x80;
    reply.p_data[3] = p_data[3];
    reply.p_data[4] = LightConfig.powerOnMode = p_data[4];
    reply.pack_len = 5;
    reply.result = lightpackageSUCCESS;
    llogsaveflag = 1;
    StoreConfig();
    return reply;
}

//*****************************************************************************
// 函数名称: GetLog
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
packageReply  GetLog(uint8_t *p_data, uint16_t data_len)
{
    uint16_t version;
    packageReply reply;

    reply.p_data = NULL;
    reply.pack_len = 0;
    if(p_data[2]&0x80)
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }
#if ANDON_LIGHT_LOG_ENABLE
    uint16_t remainlognum;
    //TODO  获取存储log长度 申请内存
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(120); 
    remainlognum = llog_read(reply.p_data+6,&reply.pack_len,p_data[4]);
    reply.p_data[4] = (uint8_t)(remainlognum>>8);
    reply.p_data[5] = (uint8_t)(remainlognum);
    reply.pack_len = reply.pack_len+6;
#else
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(6); 
    if(reply.p_data == NULL)
    {
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }
    reply.p_data[4] = 0;
    reply.p_data[5] = 0;
    reply.pack_len  = 6;
#endif  
    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = p_data[2]|0x80;
    reply.p_data[3] = p_data[3];
    reply.result = lightpackageSUCCESS;

    return reply;
}

//*****************************************************************************
// 函数名称: GetexchangeKey
// 函数描述: 仅使用GATT连接时支持秘钥交换，mesh传输数据时不支持秘钥交换
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply  GetexchangeKey(uint8_t *p_data, uint16_t data_len)
{
    extern packageReply AndonServiceSetEncryptKey(uint8_t *data, uint8_t len);
    
    packageReply reply;
    packageReply reply1;

    reply.p_data = NULL;
    reply.pack_len = 0;
    //if((p_data[2]&0x80) || (src != 0))  //仅使用GATT连接时支持秘钥交换
    if(p_data[2]&0x80)
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }
    reply1 = AndonServiceSetEncryptKey(p_data+4,data_len-4);
    if(reply1.result != lightpackageSUCCESS)
    {
        reply.result = reply1.result;
        return reply;
    }
    reply.pack_len = reply1.pack_len+4;
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(reply.pack_len); 
    if(reply.p_data == NULL)
    {
        wiced_bt_free_buffer(reply1.p_data);
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }
    memcpy(reply.p_data+4, reply1.p_data,reply1.pack_len);
    wiced_bt_free_buffer(reply1.p_data);
    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = p_data[2]|0x80;
    reply.p_data[3] = p_data[3];
    reply.result = lightpackageSUCCESS;
    return reply;
}

//*****************************************************************************
// 函数名称: AndonServiceSetBind
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
packageReply AndonServiceSetBind(uint8_t *p_data, uint8_t len)
{
    packageReply reply={0};
    // uint8_t tmpBuffer[150] = {0};
    // uint16_t templen;
    // uint8_t *devicetoken;
    // uint32_t randomdev;

    // reply.p_data = NULL;
    // reply.pack_len = 0;

    // devicetoken = (uint8_t *)wiced_bt_get_buffer(FLASH_XXTEA_SN_LEN+1); 
    // if(devicetoken == NULL)
    // {
    //     reply.result = lightpackageMEMORYFAILED;
    //     return reply;
    // }
    
    // storage_read_sn(devicetoken);
    // devicetoken[FLASH_XXTEA_SN_LEN] = '\0';
    // if(p_data[0] == 0)
    // {   
    //     uint8_t *encryptdata;
    //     size_t  encryptlen;

    //     randomdev = wiced_hal_rand_gen_num();
    //     devicetoken[FLASH_XXTEA_ID_LEN] = '\0';  //devicetoken由DID+KEY构成，DID在前且未字符串，此处为DID增加结束符
    //     randomdev = randomdev&0x7FFFFFFF;
    //     // randomdev = 0;
    //     mylib_sprintf(tmpBuffer, "{\"device_id\":\"%s\",\"timestamp\":\"%d\"}", devicetoken, randomdev);
    //     // LOG_DEBUG("%s \n",tmpBuffer);
    //     wiced_bt_free_buffer(devicetoken);
    //     templen = strlen(tmpBuffer);
    //     // LOG_DEBUG("BindToken1: %s \n",p_data+2);
    //     // WICED_BT_TRACE_ARRAY(p_data+2,16,"BindToken1: len = %d\n",templen);
    //     encryptdata = WYZE_F_xxtea_encrypt(tmpBuffer, templen, p_data+2, &encryptlen, WICED_TRUE);
    //     if(NULL == encryptdata)
    //     {
    //         reply.result = lightpackageMEMORYFAILED;
    //         return reply;
    //     }
    //     // WICED_BT_TRACE_ARRAY(p_data+2,16,"BindToken1: len = %d\n",templen);
    //     uint8_t *base64_data = WYZE_base64_encode(encryptdata, encryptlen);
    //     wiced_bt_free_buffer(encryptdata);
    //     if(base64_data == NULL)
    //     {
    //         reply.result = lightpackageMEMORYFAILED;
    //         return reply;
    //     }
    //     templen = strlen(base64_data);
    //     reply.p_data = (uint8_t *)wiced_bt_get_buffer(templen+2); 
    //     if(reply.p_data == NULL)
    //     {
    //         wiced_bt_free_buffer(base64_data);
    //         reply.result = lightpackageMEMORYFAILED;
    //         return reply;
    //     }
    //     reply.p_data[1] = templen&0xff;
    //     LOG_DEBUG("deviceToken1 len=%d v=%s \n", templen, base64_data);
    //     memcpy(reply.p_data+2, base64_data, templen);
    //     wiced_bt_free_buffer(base64_data);
    //     reply.p_data[0] = 2;
    //     reply.pack_len = templen+2;
    // }
    // else if(p_data[0] == 1)
    // {
    //     uint8_t *encryptdata,*decryptdata;
    //     size_t  encryptlen,decryptlen;
    //     uint8_t *base64_data, *base64_key;

    //     encryptdata = wiced_bt_get_buffer(len);
    //     memset(encryptdata, 0, len);
    //     memcpy(encryptdata, p_data+2, len-2);
    //     LOG_DEBUG("encryptBindTokenB64: %s\n",encryptdata);
    //     base64_data = WYZE_base64_decode(encryptdata, &templen);
    //     wiced_bt_free_buffer(encryptdata);
    //     if(base64_data == NULL)
    //     {
    //         LOG_DEBUG("base64_data: no mem!!!\n");
    //         wiced_bt_free_buffer(devicetoken);
    //         reply.result = lightpackageMEMORYFAILED;
    //         return reply;
    //     }
    //     //devicetoken由DID+KEY构成，DID在前且未字符串，KEY在后
    //     LOG_DEBUG("encryptBindToken: %s\n",base64_data);
    //     WICED_BT_TRACE_ARRAY(base64_data,templen,"encryptBindTokenHex templen=%d\n",templen);
    //     WICED_BT_TRACE_ARRAY(devicetoken+FLASH_XXTEA_KEY_OFFSET,FLASH_XXTEA_KEY_LEN,"encryptKeyHex");
    //     base64_key = WYZE_base64_encode(devicetoken+FLASH_XXTEA_KEY_OFFSET, FLASH_XXTEA_KEY_LEN);
    //     if(base64_key == NULL)
    //     {
    //         LOG_DEBUG("base64_key: no mem!!!\n");
    //         wiced_bt_free_buffer(devicetoken);
    //         wiced_bt_free_buffer(base64_data);
    //         reply.result = lightpackageMEMORYFAILED;
    //         return reply;
    //     }
    //     decryptdata = WYZE_F_xxtea_decrypt(base64_data,templen,base64_key, &decryptlen, WICED_TRUE);
    //     wiced_bt_free_buffer(base64_data);
    //     wiced_bt_free_buffer(base64_key);
    //     if(NULL == decryptdata)
    //     {
    //         LOG_DEBUG("decryptdata: no mem!!!\n");
    //         wiced_bt_free_buffer(devicetoken);
    //         reply.result = lightpackageMEMORYFAILED;
    //         return reply;
    //     }

    //     //bindkey 保存失败 则将bindkey置为0 使得绑定失败
    //     if(WICED_FALSE == StoreBindKey(decryptdata,decryptlen))
    //     {
    //         memset(decryptdata,0,decryptlen);
    //     }
    //     WICED_BT_TRACE_ARRAY(decryptdata,decryptlen,"BindTokenHex len = %d ",decryptlen);
    //     randomdev = wiced_hal_rand_gen_num();
    //     randomdev = randomdev&0x7FFFFFFF; 
    //     devicetoken[FLASH_XXTEA_ID_LEN] = '\0';  //devicetoken由DID+KEY构成，DID在前且未字符串，此处为DID增加结束符
    //     mylib_sprintf(tmpBuffer, "{\"device_id\":\"%s\",\"timestamp\":\"%d\"}", devicetoken, randomdev);
    //     wiced_bt_free_buffer(devicetoken);
    //     templen = strlen(tmpBuffer);
    //     LOG_DEBUG("ConfirmKey: %s\n",tmpBuffer);
    //     encryptdata = WYZE_F_xxtea_encrypt(tmpBuffer,templen,decryptdata, &encryptlen, WICED_TRUE);
    //     wiced_bt_free_buffer(decryptdata);
    //     if(NULL == encryptdata)
    //     {
    //         LOG_DEBUG("encryptdata: no mem!!!\n");
    //         reply.result = lightpackageMEMORYFAILED;
    //         return reply;
    //     }
    //     base64_data = WYZE_base64_encode(encryptdata, encryptlen);
    //     wiced_bt_free_buffer(encryptdata);
    //     if(base64_data == NULL)
    //     {
    //         LOG_DEBUG("base64_data: no mem!!!\n");
    //         reply.result = lightpackageMEMORYFAILED;
    //         return reply;
    //     }
    //     templen = strlen(base64_data);
    //     reply.p_data = (uint8_t *)wiced_bt_get_buffer(templen+2); 
    //     if(reply.p_data == NULL)
    //     {
    //         LOG_DEBUG("reply: no mem!!!\n");
    //         wiced_bt_free_buffer(base64_data);
    //         reply.result = lightpackageMEMORYFAILED;
    //         return reply;
    //     }
    //     reply.p_data[1] = templen&0xFF;
    //     LOG_DEBUG("ConfirmKeyEncryptB64: %s\n",base64_data);
    //     memcpy(reply.p_data+2, base64_data, templen);
    //     wiced_bt_free_buffer(base64_data);
    //     reply.pack_len = templen+2;
    //     reply.p_data[0] = 3;
    // }
    // else 
    if(p_data[0] == 0xFF)
    {
        extern void appSetAdvFastEnable(void);
        appSetAdvFastEnable();
        StoreBindKey(NULL,0);
        storageBindkey.bindflag = WICED_FALSE;
        reply.p_data = (uint8_t *)wiced_bt_get_buffer(2); 
        reply.pack_len = 2;
        reply.p_data[0] = 0xFF;
        reply.p_data[1] = 0;
        LightFlash(60,1,30,0,0);
    }
    reply.result = lightpackageSUCCESS;
    return reply;
}

//*****************************************************************************
// 函数名称: SetDeivceBind
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply SetDeivceBind(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;
    packageReply reply1;

    reply.p_data = NULL;
    reply.pack_len = 0;

    if(p_data[2]&0x80)
    {
        reply.result = lightpackageOPCODEFAILED;
        return reply;
    }

    if(data_len < 4)
    {
        reply.result = lightpackageACTIONFAILED;
        return reply;
    }

    reply1 = AndonServiceSetBind(p_data+4,data_len-4);
    if(reply1.p_data == NULL){
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }else{
        if(reply1.result == lightpackageSUCCESS){
            reply.pack_len = reply1.pack_len+4;
            reply.p_data = (uint8_t *)wiced_bt_get_buffer(reply.pack_len); 
            memcpy(reply.p_data+4, reply1.p_data,reply1. pack_len);
            // WICED_BT_TRACE_ARRAY(reply.p_data,reply.pack_len,"Anond len=%d Binddata: \n",reply.pack_len);
        }else{
            reply.result = reply1.result;
            return reply;
        }
        wiced_bt_free_buffer(reply1.p_data);
    }
    reply.p_data[0] = 0;
    reply.p_data[1] = p_data[1];
    reply.p_data[2] = p_data[2]|0x80;
    reply.p_data[3] = p_data[3];
    reply.result = lightpackageSUCCESS;
    return reply;
}

//*****************************************************************************
// 函数名称: SetDeivceAuth
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply SetDeivceAuth(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;
    // packageReply reply1;

    // reply.p_data = NULL;
    // reply.pack_len = 0;

    // if(p_data[2]&0x80)
    // {
    //     reply.result = lightpackageOPCODEFAILED;
    //     return reply;
    // }

    // if(data_len < 4)
    // {
    //     reply.result = lightpackageACTIONFAILED;
    //     return reply;
    // }

    // {
    //     MD5_CTX md5;
    //     uint8_t *md5input;
    //     uint8_t randomLocal[32]  = {0};
    //     uint8_t randomRemote[16] = {0};
    //     uint8_t md5Remote[16]  = {0};
    //     uint16_t len;

    //     md5input = (uint8_t *)wiced_bt_get_buffer(256); 
        
    //     mylib_sprintf(randomLocal, "%u", wiced_hal_rand_gen_num());

    //     memcpy(randomRemote, p_data+4, 16);
    //     memcpy(md5Remote, p_data+4+16, 16);
    //     WYZE_MD5Init(&md5);
    //     mylib_sprintf(md5input, "%s|%s|%s", storageBindkey.bindkey, storageBindkey.bindkey, randomRemote);
    //     len = strnlen((const char *)md5input, 256);
    //     WYZE_MD5Update(&md5, md5input, len);
    //     WYZE_MD5Final(md5input, &md5);
    //     if(0 != memcpy(md5Remote,md5input,16))
    //     {
    //         //鉴权失败 不允许进行数据交互
    //         reply.pack_len = 4;
    //         lightpackSetAuthDisable();
    //     }
    //     else
    //     {
    //         lightpackSetAuthEnable();
    //         WYZE_MD5Init(&md5);
    //         mylib_sprintf(md5input, "%s|%s|%s", storageBindkey.bindkey, storageBindkey.bindkey, randomLocal);
    //         len = strnlen((const char *)md5input, 256);
    //         WYZE_MD5Update(&md5, md5input, len);
    //         WYZE_MD5Final(md5input, &md5);
    //         reply.p_data = (uint8_t *)wiced_bt_get_buffer(32+4); 
    //         memset(reply.p_data, 0, 32+4);
    //         memcpy(reply.p_data+4, randomLocal, strnlen(randomLocal,16));
    //         memcpy(reply.p_data+4+16, md5input, 16);
    //         reply.pack_len = 4 + 32;
    //     }
    //     wiced_bt_free_buffer(md5input);
    // }

    // reply.p_data[0] = 0;
    // reply.p_data[1] = p_data[1];
    // reply.p_data[2] = p_data[2]|0x80;
    // reply.p_data[3] = p_data[3];
    // reply.result = lightpackageSUCCESS;
    return reply;
}

//*****************************************************************************
// 函数名称: SetDeivceAuth
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
static packageReply GetDeivceAuth(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;
    // packageReply reply1;

    // reply.p_data = NULL;
    // reply.pack_len = 0;

    // if(p_data[2]&0x80)
    // {
    //     reply.result = lightpackageOPCODEFAILED;
    //     return reply;
    // }

    // if(data_len < 4)
    // {
    //     reply.result = lightpackageACTIONFAILED;
    //     return reply;
    // }


    // reply.p_data = (uint8_t *)wiced_bt_get_buffer(4);
    // reply.pack_len = 4;
    // reply.p_data[0] = 0;
    // reply.p_data[1] = p_data[1];
    // reply.p_data[2] = p_data[2]|0x80;
    // reply.p_data[3] = p_data[3];
    // reply.result = lightpackageSUCCESS;
    return reply;
}

//*****************************************************************************
// 函数名称: findResource
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
Resource *findResource(uint8_t resource_id)
{
    Resource *p = (Resource *)resource_list;

    for (int i = 0; i < _countof(resource_list); i++)
    {
        if (p->ResourceId == resource_id)
        {
            return p;
        }
        p++;
    }
    return NULL;
}
//*****************************************************************************
// 函数名称: findResourceNoEncrypt
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
Resource *findResourceNoEncrypt(uint8_t resource_id) 
{
    Resource *p = (Resource *)resourceNoEncrypt_list;

    for (int i = 0; i < _countof(resourceNoEncrypt_list); i++)
    {
        if (p->ResourceId == resource_id)
        {
            return p;
        }
        p++;
    }
    return NULL;
}

//*****************************************************************************
// 函数名称: lightpackUnpackMsgNoEncrypt
// 函数描述: 无加密数据传输的数据解包，与加密数据采用不同的函数，以便在未获取秘钥的情况下
//          不处理需使用加密数据传输的指令
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
packageReply lightpackUnpackMsgNoEncrypt(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;

    Resource *item = findResourceNoEncrypt(p_data[3]);
    reply.result = lightpackageSUCCESS;
    reply.pack_len = 0;
    reply.p_data   = NULL;
    if (item)
    {
        LOG_DEBUG("findResourceNoEncrypt\n");
        switch (p_data[2]&0x7F)
        {
            case ACTION_GET:
                if (item->Get)
                {
                    LOG_DEBUG("call Get\n");
                    reply = item->Get(p_data, data_len);
                }
                break;
        }
    }
    return reply;
}

//*****************************************************************************
// 函数名称: lightpackUnpackMsg
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
packageReply lightpackUnpackMsg(uint8_t *p_data, uint16_t data_len)
{
    packageReply reply;

    Resource *item = findResource(p_data[3]);
    reply.result = lightpackageSUCCESS;
    reply.pack_len = 0;
    reply.p_data   = NULL;
    // if((u8lightpackAuthFlag == WICED_FALSE) && (p_data[3] != 0x3E))
    // {
    //     reply.result = lightpackageOPCODEFAILED;
    //     return reply;
    // }
    if (item)
    {
        switch (p_data[2]&0x7F)
        {
            case ACTION_GET:
                if (item->Get)
                {
                    LOG_VERBOSE("call get\n");
                    reply = item->Get(p_data, data_len);
                }
                break;
            case ACTION_SET:
                if (item->Set)
                {
                    LOG_VERBOSE("call set\n");
                    reply = item->Set(p_data, data_len);
                }
                break;
            case ACTION_DELTA:
                if (item->Delta)
                {
                    LOG_VERBOSE("call delta\n");
                    reply = item->Delta(p_data, data_len);
                }
                break;
            case ACTION_TOGGLE:
                if (item->Toggle)
                {
                    LOG_VERBOSE("call toggle\n");
                    reply = item->Toggle(p_data, data_len);
                }
                break;
            case ACTION_START:
                if (item->Start)
                {
                    LOG_VERBOSE("call start\n");
                    reply = item->Start(p_data, data_len);
                }
                break;
            case ACTION_STATUS:
                if (item->Status)
                {
                    LOG_VERBOSE("call status\n");
                    reply = item->Status(p_data, data_len);
                }
                break;
            case ACTION_FLASH:
                if (item->Flash)
                {
                    LOG_VERBOSE("call Flash\n");
                    reply = item->Flash(p_data, data_len);
                }
                break;
        }
    }
    return reply;
}

//*****************************************************************************
// 函数名称: lightpackSetAuthDisable
// 函数描述: 设置之前鉴权失效
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
void lightpackSetAuthDisable(void)
{
    u8lightpackAuthFlag = WICED_FALSE;
}

//*****************************************************************************
// 函数名称: lightpackSetAuthEnable
// 函数描述: 设置之前鉴权失效
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
void lightpackSetAuthEnable(void)
{
    u8lightpackAuthFlag = WICED_TRUE;
}
//*****************************************************************************
// 函数名称: lightpackNotifyDevStata
// 函数描述: 向网络中设备发送自己的当前状态
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
packageReply lightpackNotifyDevStata(void)
{
    packageReply reply;
    uint8_t current,targe;
    uint16_t transtime;

    reply.p_data = NULL;
    reply.pack_len = 0;
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(8);
    if(reply.p_data == NULL)
    {
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }
   
    reply.p_data[0] = 0;
    reply.p_data[1] = ++u8lightpackSendCno;
    reply.p_data[2] = ACTION_STATUS;
    reply.p_data[3] = 2;
    lightGetStatus(&current,&targe,&transtime);
    reply.p_data[4] = current;
    reply.p_data[5] = targe;
    reply.p_data[6] = (transtime>>8)&0xff;
    reply.p_data[7] = transtime&0xff;
    reply.pack_len = 8;
    return reply;
}


//*****************************************************************************
// 函数名称: GetTurnOffDelay
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
packageReply  lightpackNotifyCountdownStata(void)
{
    packageReply reply;
    uint16_t delaytime;
    uint16_t remaintime;

    reply.p_data = NULL;
    reply.pack_len = 0;
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(10);
    if(reply.p_data == NULL)
    {
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }
    //获取当前延时开关灯状态
    lightGetDelayOnOffTimer(reply.p_data+4,&delaytime,&remaintime,reply.p_data+9);
    reply.p_data[0] = 0;
    reply.p_data[1] = ++u8lightpackSendCno;
    reply.p_data[2] = ACTION_STATUS;
    reply.p_data[3] = 0x06;
    reply.p_data[5] = (delaytime>>8)&0xff;
    reply.p_data[6] =  delaytime&0xff;
    reply.p_data[7] = (remaintime>>8)&0xff;
    reply.p_data[8] =  remaintime&0xff;
    reply.pack_len = 10;
    reply.result = lightpackageSUCCESS;
    return reply;
}


//*****************************************************************************
// 函数名称: lightpackNotifyDevStata
// 函数描述: 向网络中设备发送自己的当前状态
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
packageReply lightpackNotifyDevResetStata(void)
{
    packageReply reply;

    reply.p_data = NULL;
    reply.pack_len = 0;
    reply.p_data = (uint8_t *)wiced_bt_get_buffer(6);
    if(reply.p_data == NULL)
    {
        reply.result = lightpackageMEMORYFAILED;
        return reply;
    }
   
    reply.p_data[0] = 0;
    reply.p_data[1] = ++u8lightpackSendCno;
    reply.p_data[2] = ACTION_STATUS;
    reply.p_data[3] = 0x3D;
    reply.p_data[4] = 0xFE;
    reply.p_data[5] = 0;
    reply.result = lightpackageSUCCESS;
    reply.pack_len = 6;
    return reply;
}

//*****************************************************************************
// 函数名称: lightpackGetSendCno
// 函数描述: Gatt的数据包向mesh发送时的转换
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
uint8_t lightpackGetSendCno(void)
{
    return u8lightpackSendCno;
}

//*****************************************************************************
// 函数名称: lightpackSetSendCno
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
void lightpackSetSendCno(uint8_t seq)
{
    u8lightpackSendCno = seq;
}