#include "light_model.h"
#include "wiced_timer.h"
#include "log.h"
#include "llog.h"
#include "config.h"
#include "storage.h"
#include "adv_pack.h"
#include "vendor.h"
#include "systimer.h"

#include "wiced_platform.h"
#if LIGHTAI == configLIGHTAIANDONMODE
#include "schedule_learn.h"
#elif LIGHTAI == configLIGHTAIWYZEMODE
#include "aiparser.h"
#endif

#define lightSTARTTRANSIONTIME   (1<<0)

#define lightCANCLETRANSIONTIME  (1<<0)
#define lightCANCLECOUNTDOWN     (1<<1)

#define LIGHT_TIMER_CB_LENGTH    50

#define MORSE_MAX_CHAR           15
#define MORSE_MAX_BIT            100

#define LIGHT_MORSECODE_T        300
#define MORSEDI_MASK             0
#define MORSEDA_MASK             1
#define MORSECHAR_MASK           2
#define MORSEWORD_MASK           3

#define MORSEDI_LENGTH           1
#define MORSEDA_LENGTH           3
#define MORSESPACE_LENGTH        1
#define MORSECHAR_LENGTH         3
#define MORSEWORD_LENGTH         7

#define LIGHT_TIMER_UINT  (LIGHT_TIMER_CB_LENGTH>10?(LIGHT_TIMER_CB_LENGTH/10):1*(10/LIGHT_TIMER_CB_LENGTH))  


#define TURNONOFFDELAYSTOP              0
#define TURNONOFFDELAYRUNING            1


#if LIGHTAI == configLIGHTAIWYZEMODE
#define sysclockToWyzeAIClock(wyzeAIClock, ptrsysClock)                              \
    do                                                                               \
    {                                                                                \
        ptrsysClock = sysTimerGetClockTime();                                        \
        sysClock.nYear = ptrsysClock->nYear;                                         \
        sysClock.nMonth = ptrsysClock->nMonth;                                       \
        sysClock.nDay = ptrsysClock->nDay;                                           \
        sysClock.nHour = ptrsysClock->nHour;                                         \
        sysClock.nMinute = ptrsysClock->nMinute;                                     \
        sysClock.nSecond = ptrsysClock->nSecond;                                     \
    } while (0)
#endif


//摩斯密码表 最前面为长度 0表示滴（短） 1表示嗒（长） 2表示字符间隔（MORSECHAR_MASK） 3表示单次间隔（MORSEWORD_MASK）
const uint8_t Morsecodemask[][6] = {
    {2,0,1},            //A
    {4,1,0,0,0},
    {4,1,0,1,0},
    {3,1,0,0},
    {1,0},
    {4,0,0,1,0},
    {3,1,1,0},
    {4,0,0,0,0},
    {2,0,0},
    {4,0,1,1,1},
    {3,1,0,1},
    {4,0,1,0,0},
    {2,1,1},
    {2,1,0},
    {3,1,1,1},
    {4,0,1,1,0},
    {4,1,1,0,1},
    {3,0,1,0},
    {3,0,0,0},
    {1,1},
    {3,0,0,1},
    {4,0,0,0,1},
    {3,0,1,1},
    {4,1,0,0,1},
    {4,1,0,1,1},
    {3,1,1,0},          //Z
};

extern void andonServerSendLightStatus(void);
void MorseCodeTimerCb(TIMER_PARAM_TYPE parameter);
void delay_count_Timer_cb(TIMER_PARAM_TYPE parameter);
int8_t uint16_to_percentage(uint16_t value);
int32_t light_flashing(int32_t tick, int32_t period, int32_t initiate, int32_t final);
int32_t light_sniffer(int32_t tick, int32_t period, int32_t initiate, int32_t final);
int32_t light_flashandsniffer(int32_t tick, int32_t period, int32_t initiate, int32_t final);
int32_t turn_onoff_procedure(int32_t tick, int32_t period, int32_t initiate, int32_t final);
int32_t brightness_procedure(int32_t tick, int32_t period, int32_t initiate, int32_t final);


static int32_t flashandsniffer_allcycle,flashandsniffer_flashcycle,flashandsniffer_sniffercycle; //flashandsniffer_times;
static int32_t flashandsniffer_flashtimes,flashandsniffer_sniffertimes; 
static uint16_t flashandsniffer_flashtfrist,flashandsniffer_flash_fristtime,flashandsniffer_snifferfrist; 
static uint16_t flashandsniffer_flashfrist,flashandsniffer_snifferfrist; 
static int32_t flashing_cycle, flashing_times;
static int32_t sniffer_cycle; //sniffer_times;
static int16_t sniffer_direction; //sniffer_times;
static wiced_bool_t morsecodedisplay = WICED_FALSE;
// #define adv_pair_enable()
// #define adv_pair_disable()
AutoOnOff_Timer_t lightAutoOnOffTimer = {0};
TurnOnOffDelay_t  TurnOnOffDelay = {0};

uint8_t morsecodelist[MORSE_MAX_CHAR] = "SOS";           //要使用摩斯码显示的字符
uint8_t morsecodemaskdisplaycnt;                         //待显示的掩码计数 //最高位表示要显示滴答间隔
uint8_t morsecodemaskdisplayallcnt;                      //待显示的掩码总数
uint8_t morsecodemasklist[MORSE_MAX_BIT] = {0};          //摩斯码的掩码

static wiced_timer_t morsecodeTimer;

LightConfig_def LightConfig = DEFAULT_LIGHTNESS_CONFIG;

// uint16_t lightnesslogbuf[4] = {0};

static wiced_timer_t transitionTimer;
static wiced_timer_t delay_count_Timer_500ms;
static wiced_bool_t  lighttimeraienable = WICED_TRUE;

LightConfig_def currentCfg;

uint8_t transition_time = DEFAULT_TRANSITION_TIME;

uint16_t turnOffTimeCnt = 0;  

int32_t liner_transfer(int32_t tick, int32_t period, int32_t initiate, int32_t final)
{
    if (tick < period)
    {
        return (((final - initiate) * tick * 2 + 1) / period + initiate * 2) / 2;
    }
    else
    {
        return final;
    }
}

int32_t polyline_transfer1(int32_t tick, int32_t period, int32_t initiate, int32_t final)
{
    if (period < 100)
        return liner_transfer(tick, period, initiate, final);

    if (tick < period / 5)
        return liner_transfer(tick, period / 5, initiate, (final + initiate) / 2);
    else
        return liner_transfer(tick - period / 5, period, (final + initiate) / 2, final);
}

int32_t polyline_transfer2(int32_t tick, int32_t period, int32_t initiate, int32_t final)
{
    return liner_transfer(tick, period, final / 5 + initiate * 4 / 5, final);
}

int32_t parabola_transfer(int32_t tick, int32_t period, int32_t initiate, int32_t final)
{
    int64_t t = tick, p = period, i = initiate, f = final;
    if (tick < period)
    {
        return (i - f) * t * t / p / p + 2l * (f - i) * t / p + i;
    }
    else
    {
        return final;
    }
}

int32_t polyline_transfer3(int32_t tick, int32_t period, int32_t initiate, int32_t final)
{
    if (period <= (250/LIGHT_TIMER_UINT))
        return polyline_transfer2(tick, period, initiate, final);

    // //延迟开灯的亮度小于10%，直接给10%亮度
    // if((initiate==0)&&(10>=uint16_to_percentage(final))){
    //     return(uint16_to_percentage(10));
    // }else if((final==0)&&(5>uint16_to_percentage(initiate))){ 
    //     //关灯时亮度小于5% 恒定5%亮度不变，直到关灯
    //     if(tick == period){
    //         return final;
    //     }else{
    //         return(uint16_to_percentage(5));
    //     }
    // }else

    // if (tick < (50/LIGHT_TIMER_UINT))
    //     if (initiate < 29491)
    //     {
    //         if (tick < (25/LIGHT_TIMER_UINT))
    //             return liner_transfer(tick, 25/LIGHT_TIMER_UINT, initiate, 45875);
    //         else
    //             return liner_transfer(tick - 25/LIGHT_TIMER_UINT, 25/LIGHT_TIMER_UINT, 45875, (final + initiate) / 2);
    //     }
    //     else
    //     {
    //         return liner_transfer(tick, 50/LIGHT_TIMER_UINT, initiate, (final + initiate) / 2);
    //     }
    // else 
    if (tick < period - (20/LIGHT_TIMER_UINT))
        return (final + initiate) / 3;
    else
        return liner_transfer(tick - (period - 20/LIGHT_TIMER_UINT), 20/LIGHT_TIMER_UINT, (final + initiate) / 3, final);
}

Animination ani_table[] = {
    liner_transfer,
    polyline_transfer1,
    polyline_transfer2,
    polyline_transfer3,
    parabola_transfer
};

uint8_t ani_method = 3;

struct
{
    Animination anim;
    int32_t tick;
    int32_t period;
    int32_t initiate;
    int32_t final;
    uint16_t PrePowerTick;
    uint16_t PreLightnessLevel;  //灯调节的目标亮度，使用此变量，是由于闪烁时亮度与final的亮度不同
} ctx = {0};

void lightModelCb(TIMER_PARAM_TYPE parameter)
{
    volatile uint64_t tick;
    //uint32_t time;
    uint32_t sec;
    uint32_t ms;
        
    tick = wiced_bt_mesh_core_get_tick_count();
    sec  = (tick / 1000) % 1000;
    ms  = tick % 1000;

    //LOG_DEBUG("[%d.%d]andon_lightModelCb start\n",sec,ms);
    if (ctx.anim)
    {
        LOG_VERBOSE("lightModelCb tick %d   period %d  PrePowerTick %d \n",ctx.tick,ctx.period,ctx.PrePowerTick);
        if(ctx.PrePowerTick == 0xFFFF)
        {
            LOG_VERBOSE("ctx.initiate %d  ctx.final %d lightnessLevel %d\n",ctx.initiate, ctx.final, LightConfig.lightnessLevel);
        }
        if((!ctx.initiate) && (ctx.final) && (ctx.PreLightnessLevel < 17309))
        {
            //if((ctx.PrePowerTick == 0xFFFF) && (ctx.period > 2*100/LIGHT_TIMER_CB_LENGTH))
            if(ctx.PrePowerTick == 0xFFFF)
            {
                if(turnOffTimeCnt < 5)
                ctx.PrePowerTick = 0;
                if(turnOffTimeCnt >4 && turnOffTimeCnt <7)
                ctx.PrePowerTick = 1*50/LIGHT_TIMER_CB_LENGTH;
                if(turnOffTimeCnt >6 && turnOffTimeCnt<17)
                ctx.PrePowerTick = 1*100/LIGHT_TIMER_CB_LENGTH;
                if(turnOffTimeCnt >16 && turnOffTimeCnt<90)
                ctx.PrePowerTick = 1*150/LIGHT_TIMER_CB_LENGTH;
                if(turnOffTimeCnt > 89)
                ctx.PrePowerTick = 2*100/LIGHT_TIMER_CB_LENGTH; 
                ctx.period += ctx.PrePowerTick;
                ctx.PrePowerTick += 1;
            }
        }
        else
        {
            ctx.PrePowerTick = 0;
        }

        if(ctx.tick < ctx.PrePowerTick)
        {
            currentCfg.lightingOn = 1;
            if (ctx.PrePowerTick < 5)
            {
                if (turnOffTimeCnt > 12 && turnOffTimeCnt < 17)
                {
                    currentCfg.lightnessLevel = 22281;
                }
                else
                {
                    currentCfg.lightnessLevel = 14483;
                }
            }
            else if(ctx.PrePowerTick == 5)
            {
                if (turnOffTimeCnt > 16 && turnOffTimeCnt < 24)
                {
                    currentCfg.lightnessLevel = 22281;
                }
                else if(turnOffTimeCnt >23 && turnOffTimeCnt < 40)
                {
                    currentCfg.lightnessLevel = 27852;
                }
                else
                {
                    currentCfg.lightnessLevel = 30000;
                }   
            }
            else
            {
                currentCfg.lightnessLevel = 32768;
            } 
            LOG_VERBOSE("ctx.tick %d  turnOffTimeCnt %d lightnessLevel %d\n",ctx.tick,turnOffTimeCnt,currentCfg.lightnessLevel);
            LightUpdate();
        }
        else if (ctx.tick < (ctx.period + ctx.PrePowerTick))
        {
            turnOffTimeCnt = 0;  
            ctx.anim(ctx.tick-ctx.PrePowerTick, ctx.period-ctx.PrePowerTick, ctx.initiate, ctx.final);
            LOG_VERBOSE("ctx.tick %d  turnOffTimeCnt %d lightnessLevel %d\n",ctx.tick,turnOffTimeCnt,currentCfg.lightnessLevel);
        }
        else
        {
            //渐变周期大于1s时，渐变完成后主动上报一次状态
            if(ctx.period > 1000/LIGHT_TIMER_CB_LENGTH){
                if(LightConfig.bleonly != CONFIG_BLEONLY){
                    vendorSendDevStatus();
                }
                if(LightConfig.bleonly != CONFIG_MESHONLY){
                    andonServerSendLightStatus();
                }
            }
            turnOffTimeCnt = 0;
            LOG_DEBUG("lightModelCb ctx.tick = %d, period = %d\n",ctx.tick, ctx.period);
            ctx.anim(ctx.tick-ctx.PrePowerTick, ctx.period-ctx.PrePowerTick, ctx.initiate, ctx.final);
            LOG_VERBOSE("ctx.tick %d  turnOffTimeCnt %d lightnessLevel %d\n",ctx.tick,turnOffTimeCnt,currentCfg.lightnessLevel);
            memset(&ctx, 0, sizeof(ctx));
            ctx.PrePowerTick = 0;
        }
        //ctx.tick += LIGHT_TIMER_UINT;
        ctx.tick ++;
    }
    else
    {
        turnOffTimeCnt = 0;
        LOG_VERBOSE("TransitionTimer Deinit \n");
        wiced_stop_timer(&transitionTimer);
        //wiced_deinit_timer(&transitionTimer);
    }
    tick = wiced_bt_mesh_core_get_tick_count();
    sec  = (tick / 1000) % 1000;
    ms  = tick % 1000;

    //LOG_DEBUG("[%d.%d]andon_lightModelCb stop\n",sec,ms);
}

//*****************************************************************************
// 函数名称: ligntModelAppTimerCb
// 函数描述: 灯的1s定时器处理
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
void ligntModelAppTimerCb(void)
{
    uint16_t temp;
    uint32_t systemUTCtimer;
    wiced_bt_device_address_t bd_addr;
    systimerClock_t *myclock;
    uint16_t timerflag = 0;

    wiced_bt_dev_read_local_addr(bd_addr);

    if((currentCfg.lightingOn) == 1)
    {
        LightConfig.Lightontime++;
        if(LightConfig.Lightontime > 0x3FFFFFFE)
        {
            LightConfig.Lightontime = 0x3FFFFFFE;
        }
        //每30分钟自动保存一次
        if((LightConfig.Lightontime % 1800) == 0)
        {
            llogsaveflag = 1;
            StoreConfig();
        }
    }
    else  //开灯时不统计关灯计时
    {
        turnOffTimeCnt ++;
        if(turnOffTimeCnt > 300)
        {
            turnOffTimeCnt = 300;
        }
    }
    //未进行时间同步
    systemUTCtimer = sysTimerGetSystemUTCTime();
    if(systemUTCtimer < systimer_MINSYSTIMECNT)  
    {
        if(systemUTCtimer > (systimer_MINSYSTIMECNT-0x1000))
        {
            systemUTCtimer = systimer_MINSYSTIMECNT-0x1000;
        }
        return;
    }

    if(advpackispairing() == WICED_TRUE){
        return;
    }

    myclock = sysTimerGetClockTime();
    //仅整分时进行定时开关灯识别
    if(myclock->nSecond == 0){
        uint8_t week;

        LOG_DEBUG("clock: w %d  h %d min %d \n", myclock->nWeek, myclock->nHour, myclock->nMinute);
        for(uint8_t i=0; i<lightAutoOnOffTimer.num; i++){
            LOG_DEBUG("timer: w %x  time %d  enable %d\n", 
                       lightAutoOnOffTimer.timerList[i].weekset, lightAutoOnOffTimer.timerList[i].time,lightAutoOnOffTimer.timerList[i].timerenable);
            if(lightAutoOnOffTimer.timerList[i].timerenable == 0){
                continue;
            }
            week = 1<<(myclock->nWeek-1);
            //设置了重复，但未到设定的星期x
            // if( ( (lightAutoOnOffTimer.timerList[i].timerepeat) || (lightAutoOnOffTimer.timerList[i].weekset))
            //     && ((week&lightAutoOnOffTimer.timerList[i].weekset) == 0) ){
            if( (lightAutoOnOffTimer.timerList[i].weekset) && ((week&lightAutoOnOffTimer.timerList[i].weekset) == 0) ){
                continue;
            }
            temp = myclock->nHour;
            temp = temp*60 + myclock->nMinute;
            if(temp == ((lightAutoOnOffTimer.timerList[i].time)%1440)){
                //处于延迟开关灯过程中，不执行定时器操作，因为延迟开灯的操作时效性更高
                if(TurnOnOffDelay.flag == TURNONOFFDELAYSTOP){
                    //定时开灯且开灯亮度设置非0
                    if(lightAutoOnOffTimer.timerList[i].Aienable){
                        lighttimeraienable = WICED_TRUE;
                    }else{
                        lighttimeraienable = WICED_FALSE;
                    }
                    if((lightAutoOnOffTimer.timerList[i].brightness)&&(lightAutoOnOffTimer.timerList[i].onoffset)){
                        lighttimeraienable = WICED_FALSE;
                        LightConfig.lightnessLevel = lightAutoOnOffTimer.timerList[i].brightness;
                    }
                    //当前灯的开关状态和定时设置的开关状态相同，不响应定时器
                    if(currentCfg.lightingOn != lightAutoOnOffTimer.timerList[i].onoffset){
                        LightModelTurn(lightAutoOnOffTimer.timerList[i].onoffset,0x04,0);
                    }else if(lightAutoOnOffTimer.timerList[i].onoffset){ 
                        //定时开灯&已被点亮&设置了亮度时，调整亮度
                        LightModelSetBrightness(uint16_to_percentage(LightConfig.lightnessLevel),4,0);
                    }
                }
                if((lightAutoOnOffTimer.timerList[i].timerepeat == 0) && (lightAutoOnOffTimer.timerList[i].weekset == 0)){
                    timerflag = 1;
                    lightAutoOnOffTimer.timerList[i].timerenable = 0;
                }
            }
        }
    }

    if(timerflag == 1){
        StoreSaveUserTimer((uint8_t*)(&lightAutoOnOffTimer),sizeof(AutoOnOff_Timer_t));
    }

    // //使用MAC而不是NODE_ID，是MAC是足够随机的，NODE_ID随着设备的增删可能不够随机
    // //addr_num = wiced_bt_mesh_core_get_local_addr();
    // temp = bd_addr[5];
    // temp = temp *256 + bd_addr[4];
    // if((systemUTCtimer%60) != addr_num%60)
    // {
    //     return;
    // }
    //vendorSendDevStatus();
    
}

uint16_t percentage_to_uint16(int8_t percentage)
{
    if (percentage < 0)
        percentage = 0;
    if (percentage > 100)
        percentage = 100;

    uint32_t temp = percentage;
    temp *= 65535;
    temp += 50;
    temp /= 100;
    temp += 1;
    if (temp > 65535)
        temp = 65535;

    return temp;
}

int8_t uint16_to_percentage(uint16_t value)
{
    uint32_t temp = value;

    temp *= 100;
    temp += 32767;
    temp /= 65536;

    if (temp > 100)
        temp = 100;

    return temp;
}

// store light configuration to nvram
void LightStore(void)
{
    llogsaveflag = 1;
    StoreConfig();
}


// load light configuratin from nvram
void LightLoad(void)
{
    
    LOG_DEBUG("pre_load_cfg: On:%d Level:%d\n",LightConfig.lightingOn, LightConfig.lightnessLevel);
    LoadConfig();

    if(LightConfig.fwupgrade == 1)  //如果是由于OTA重启
    {
        LightConfig.brightdeltastep = CONFIG_DEFAULT_STEP;
        // LOG_DEBUG("Lightruntime:%d LightUTCtime:%d  brightdeltastep:%d\n ",LightConfig.Lightruntime,LightConfig.LightUTCtime,LightConfig.brightdeltastep);
        if(LightConfig.Lightruntime)
        {
            sysTimerSetSystemRunTime(LightConfig.Lightruntime);
        }
        if(LightConfig.LightUTCtime)
        {
            sysTimerSetSystemUTCTime(LightConfig.LightUTCtime,LightConfig.Lightzone,0);
        }
        
    }
    else
    {
#if LIGHTAI == configLIGHTAIANDONMODE
        AutoBrightnessSet.Item.AutoBrightnessNum = 0; //lq20200616
        // AutoBrightnessSet.Item.FlagModelOn = 0; //lq20200616
#endif
        //if(LightConfig.powerOnMode == CONFIG_POWERON_ON)  
        {
            //此处强制置为1 是为了在LightModelInitial与上电状态比对后做亮度调整
            LightConfig.lightingOn = 1;
        }

    }
    LightConfig.fwupgrade = 0;
    //TODO Test code
    //AutoBrightnessSet = DEFAULT_AUTOLIGHTNESS_CONFIG;  
    //SystemUTCtimer = 18*60*60;
    //TODO Test end
#if LIGHTAI == configLIGHTAIANDONMODE
    AutoBrightnessSet.Item.AutoBrightnessSetSave = 1;
#endif
    llogsaveflag = 1;
    StoreConfig(); 
}

// passthrough light configuration to PWM output
void LightUpdate(void)
{
    uint32_t temp;

    temp = currentCfg.lightnessLevel;
    //LOG_VERBOSE("LightUpdate duty = %d\n",temp*100/65535);
    led_controller_status_update(currentCfg.lightingOn, currentCfg.lightnessLevel, currentCfg.lightnessCTL);
}

void LightModelInitial(uint8_t onoff)
{
    LightConfig_def pre_load_cfg;

    memcpy(&pre_load_cfg,&LightConfig,sizeof(LightConfig_def));
    // first load configuration
    LightLoad();
#if LIGHTAI == configLIGHTAIANDONMODE
    LearnBufInitial();
#elif LIGHTAI == configLIGHTAIWYZEMODE
    init_model();
#endif
    // copy permantent configuration to current configuration
    
    //currentCfg.lightingOn = LightConfig.lightingOn;
    //LightUpdate();
    // clear callback context
    // clear callback timer
    //if the transitionTimer is runing, the node to be provisioned and reinit light model
    if (!wiced_is_timer_in_use(&transitionTimer))
    {
        memset(&ctx, 0, sizeof(ctx));
        wiced_init_timer(&transitionTimer, lightModelCb, 0, WICED_MILLI_SECONDS_PERIODIC_TIMER);
    }
    if (!wiced_is_timer_in_use(&morsecodeTimer))
    {
        wiced_init_timer(&morsecodeTimer, MorseCodeTimerCb, 0, WICED_MILLI_SECONDS_PERIODIC_TIMER);
    }
    if (!wiced_is_timer_in_use(&delay_count_Timer_500ms))
    {
        wiced_init_timer(&delay_count_Timer_500ms, delay_count_Timer_cb, 0, WICED_MILLI_SECONDS_PERIODIC_TIMER);
    }
    morsecodedisplay = WICED_FALSE;
    LOG_DEBUG("Light Init 1: %d %d %d\n", currentCfg.lightingOn, LightConfig.lightnessLevel, LightConfig.lightnessCTL);
    // wiced_init_timer(&transitionTimer, lightModelCb, 0, WICED_MILLI_SECONDS_PERIODIC_TIMER);
    #if (LIGHT != 1) && (LIGHT != 2)
    memset(&currentCfg,0,sizeof(currentCfg));
    led_controller_initial();
    if(onoff)
    {
        ctx.PrePowerTick = 0xFFFF;
        // LightModelTurn(1,8,0);
        LightModelTurn(1,0,0);
    }
    else
    {
        ctx.PrePowerTick = 0;
        led_controller_status_update(0, LightConfig.lightnessLevel, LightConfig.lightnessCTL);
        // LightModelTurn(0,0,0);
    }
    lightModelCb(0);
    #else
    if(onoff){
        if(LightConfig.lightnessLevel != pre_load_cfg.lightnessLevel){
            memcpy(&currentCfg, &LightConfig, sizeof(currentCfg));
            currentCfg.lightnessLevel = pre_load_cfg.lightnessLevel;
            // LightModelTurn(1,0,0);
            LightModelSetBrightness(uint16_to_percentage(LightConfig.lightnessLevel),8,0);
        }else{  //如果相等 保持之前的亮度及相关设置
            pre_load_cfg.brightdeltastep = LightConfig.brightdeltastep; 
            memcpy(&LightConfig,&pre_load_cfg,sizeof(LightConfig_def));
            memcpy(&currentCfg, &LightConfig, sizeof(currentCfg));
        }
    }else{
        memset(&currentCfg,0,sizeof(currentCfg));
        LightModelTurn(0,0,0);
    }
    #endif
    LightConfig.fwupgrade = 0;
    systimerAddAppTimer(ligntModelAppTimerCb);
    // adv_pack_init();
    LOG_DEBUG("Light Init end: %d %d %d\n", currentCfg.lightingOn, LightConfig.lightnessLevel, LightConfig.lightnessCTL);

}

int32_t transt_to_period(uint8_t t)
{
    int32_t value = t & 0x3f;

    switch (t & 0xc0)
    {
    case 0x00:
        return value * 10;
    case 0x40:
        return value * 100;
    case 0x80:
        return value * 1000;
    case 0xc0:
        return value * 60000;
    }
}

uint8_t period_to_transt(uint32_t t)
{
    if(t > 600000)
    {
        return (uint8_t)((t/600000)|0xC0);
    }
    else if(t > 10000)
    {
        return (uint8_t)((t/10000)|0x80);
    }
    else if(t > 1000)
    {
        return (uint8_t)((t/1000)|0x40);
    }
    
    return (uint8_t)(t/100);
}

void lightGetStatus(uint8_t *current, uint8_t *targe, uint16_t *transtime)
{
    uint16_t PrePowerTick;
    uint32_t temp;
    
    temp = uint16_to_percentage(currentCfg.lightnessLevel);
    temp = temp<1?1:temp;
    *current = currentCfg.lightingOn?temp:0;
    temp = uint16_to_percentage(LightConfig.lightnessLevel);
    temp = temp<1?1:temp;
    *targe = LightConfig.lightingOn?temp:0;
    if(ctx.PrePowerTick == 0xFFFF){
        PrePowerTick = 0;
    }
    LOG_DEBUG("period:%d  PrePowerTick:%d  tick:%d\n",ctx.period,PrePowerTick,ctx.tick);
    temp = (ctx.period+PrePowerTick)>ctx.tick?(ctx.period+PrePowerTick-ctx.tick)*LIGHT_TIMER_CB_LENGTH/10:0;
    // if((ctx.anim != turn_onoff_procedure) && (ctx.anim != brightness_procedure) && (ctx.anim != ActionDelayFlash_procedure))
    if((ctx.anim != turn_onoff_procedure) && (ctx.anim != brightness_procedure)){
        temp = 0;
    }
    LOG_DEBUG("time: %d\n",temp);
    if(temp > 30000){
        temp = (temp+50)/100;
        *transtime = temp>30000?(30000|0x8000):(temp|0x8000);
    }else{
        *transtime = temp;
    }
}

int32_t turn_onoff_procedure(int32_t tick, int32_t period, int32_t initiate, int32_t final)
{
    static uint16_t initiate_brightness;

    // LOG_VERBOSE("%d %d %d %d\n", tick, period, initiate, final);

    if (tick == 1)
        initiate_brightness = currentCfg.lightnessLevel;
    if (initiate && (!final))
    {
        // turnning off
        if (tick < period)
        {
            if (ani_method > sizeof(ani_table) / sizeof(ani_table[0]))
                ani_method = 0;
            currentCfg.lightnessLevel = ani_table[ani_method](tick, period, initiate_brightness, 1);
        }
        else
        {
            // LOG_VERBOSE("set to 0\n");
            currentCfg.lightingOn = 0;
            memset(&ctx, 0, sizeof(ctx));
        }
    }
    else if ((!initiate) && final)
    {
        // turnning on
        if ((tick == 1) && (period > 1))
        {
            currentCfg.lightingOn = 1;
            currentCfg.lightnessLevel = percentage_to_uint16(1);
            LOG_VERBOSE("turn on duty = %d\n",LightConfig.lightnessLevel);
        }
        else if (tick < period)
        {
            if (ani_method > sizeof(ani_table) / sizeof(ani_table[0]))
                ani_method = 0;
            currentCfg.lightnessLevel = liner_transfer(tick, period, percentage_to_uint16(1) + 1, LightConfig.lightnessLevel);
        }
        else
        {
            currentCfg.lightingOn = 1;
            currentCfg.lightnessLevel = LightConfig.lightnessLevel;
            memset(&ctx, 0, sizeof(ctx));
        }
    }

    LightUpdate();
    return 0;
}

// turn light on/off
void LightModelTurn(int8_t onoff, uint8_t transition, uint16_t delay)
{
#if LIGHTAI == configLIGHTAIWYZEMODE
        StTimeInfo sysClock;
        systimerClock_t *ptrsysClock;
#endif
    if (transition == 0xff)
        transition = transition_time;

    if(wiced_is_timer_in_use(&morsecodeTimer)){
        wiced_stop_timer(&morsecodeTimer);
    }
    morsecodedisplay = WICED_FALSE;

    // if turning then return
    if (ctx.anim == turn_onoff_procedure){
        if ((ctx.final == onoff) && (TurnOnOffDelay.flag == TURNONOFFDELAYSTOP))
            return;
    }

    //if(onoff == TurnOnOffDelay.onoff){
        TurnOnOffDelay.remaintime = 0;
        TurnOnOffDelay.flag = TURNONOFFDELAYSTOP;
        wiced_stop_timer(&delay_count_Timer_500ms);
    // }

    // resolution is 10ms
    if (wiced_is_timer_in_use(&transitionTimer))
    {
        wiced_stop_timer(&transitionTimer);
    }

    if(onoff){
        ctx.PrePowerTick = 0xFFFF;
    }else{
        ctx.PrePowerTick = 0;
    }

#if ANDON_LIGHT_LOG_ENABLE
    //无效时间，存储当前时间
    if(LogConfig.lasttime == 0)
    {
        //存储绝对时间
        llogsaveflag = 0;
        LogConfig.lasttime = sysTimerGetSystemUTCTime();
        LogConfig.logvalue = sysTimerGetSystemUTCTime();
        LogConfig.operatetype = LOG_TIMETYPE;
        //存储时间LOG
        llog_write();

        LogConfig.logvalue = (onoff<<1+0x00);
        LogConfig.operatetype = LOG_ONOFFTYPE;
        //在存储配置时存储log
    }
    //重复操作，不存储log
    else if(LogConfig.logvalue != onoff)
    {
        llogsaveflag = 0;
        LogConfig.logvalue = (onoff<<1+0x00);
        LogConfig.operatetype = LOG_ONOFFTYPE;
        //在存储配置时存储log
    }

    LogConfig.logvalue = (LogConfig.logvalue << 16);
 #endif   
    //if((onoff == 1) && (lighttimeraienable))
    if(onoff == 1)
    {
#if LIGHTAI == configLIGHTAIANDONMODE
        // lightnesslogbuf[3] = LightConfig.lightnessLevel;
        if(AutoBrightnessSet.Item.FlagModelOn)
        {
            uint16_t lightnessAi = 0;
            lightnessAi = AutoAdjustBrightness();
            LOG_DEBUG("lght b: %d\n",LightConfig.lightnessLevel );
            if(lightnessAi)
            {
                LightConfig.lightnessLevel  = percentage_to_uint16(lightnessAi);
            }
            LOG_DEBUG("lght a: %d\n",LightConfig.lightnessLevel );
        }
#elif LIGHTAI == configLIGHTAIWYZEMODE
        uint16_t lightnessAi = 0;
        sysclockToWyzeAIClock(ptrsysClock,ptrsysClock);
        lightnessAi = ai_predict(sysClock);
        if(lightnessAi)
        {
            LightConfig.lightnessLevel  = percentage_to_uint16(lightnessAi);
        }
#endif

#if ANDON_LIGHT_LOG_ENABLE
        LogConfig.logvalue += uint16_to_percentage(LightConfig.lightnessLevel);
#endif
        ctx.period = transt_to_period(transition);
        if(ctx.period > 20)
        {
            uint32_t temp;

            temp = uint16_to_percentage(LightConfig.lightnessLevel);
            temp = (ctx.period-20)*temp/100+20;
            ctx.period = temp;
        }
        ctx.period = ctx.period/LIGHT_TIMER_UINT;
    }
    else
    {
        ctx.period = transt_to_period(transition);
        if(ctx.period > 20){
            uint32_t temp;
            temp = uint16_to_percentage(LightConfig.lightnessLevel);
            temp = (ctx.period-20)*temp/100+20;
            ctx.period = temp;
        }
        ctx.period = ctx.period/LIGHT_TIMER_UINT;
    }
    if (lighttimeraienable){
#if LIGHTAI == configLIGHTAIANDONMODE
        BrightModelLearning(SystemUTCtimer0,onoff?uint16_to_percentage(LightConfig.lightnessLevel):0);
        AutoBrightnessSet.Item.AutoBrightnessSetSave = 1;
#elif LIGHTAI == configLIGHTAIWYZEMODE
        sysclockToWyzeAIClock(ptrsysClock,ptrsysClock);
        ai_train(ptrsysClock,onoff?uint16_to_percentage(LightConfig.lightnessLevel):0);
#endif
    }
    lighttimeraienable = WICED_TRUE;
    // LOG_VERBOSE("Light Turnning: %d transition time: %d\n", (int32_t)onoff, (int32_t)transition);
    int32_t current = currentCfg.lightingOn;
    LightConfig.lightingOn = onoff;
    StoreConfig();

    ctx.anim = turn_onoff_procedure;
    ctx.tick = 1;
    //ctx.period = transt_to_period(transition)/LIGHT_TIMER_UINT;
    if(ctx.period == 0)
    {
        ctx.period = 1;
    }
    // > 5 ? transt_to_period(transition) : 5;
    // if(current == onoff){
    //     current = !onoff;
    // }
    ctx.initiate = current;
    ctx.final = onoff;
    ctx.PreLightnessLevel = LightConfig.lightnessLevel;

    LOG_DEBUG("Trans Init p=%d\n",ctx.period*LIGHT_TIMER_UINT);

    wiced_start_timer(&transitionTimer, LIGHT_TIMER_CB_LENGTH);
    if(LightConfig.bleonly != CONFIG_BLEONLY){
        vendorSendDevStatus();
    }
    if(LightConfig.bleonly != CONFIG_MESHONLY){
        andonServerSendLightStatus();
    }
    advRestartPair();
    
}

void LightModelToggle(int8_t reserved, uint8_t transitiontime, uint16_t delay)
{
    if (transitiontime == 0xff)
        transitiontime = transition_time;

    LightConfig.lightingOn = !currentCfg.lightingOn;
    ctx.PrePowerTick = 0xFFFF;

    // if(LightConfig.lightingOn == TurnOnOffDelay.onoff){
        TurnOnOffDelay.remaintime = 0;
        TurnOnOffDelay.flag = TURNONOFFDELAYSTOP;
        wiced_stop_timer(&delay_count_Timer_500ms);
    // }
    if(wiced_is_timer_in_use(&morsecodeTimer)){
        wiced_stop_timer(&morsecodeTimer);
    }
    morsecodedisplay = WICED_FALSE;

#if ANDON_LIGHT_LOG_ENABLE
    //无效时间，存储当前时间
    if(LogConfig.lasttime == 0)
    {
        llogsaveflag = 0;
        //存储绝对时间
        LogConfig.lasttime = sysTimerGetSystemUTCTime();
        LogConfig.logvalue = sysTimerGetSystemUTCTime();
        LogConfig.operatetype = LOG_TIMETYPE;
        //存储时间LOG
        llog_write();

        LogConfig.logvalue = (LightConfig.lightingOn<<1)+0x01;
        LogConfig.operatetype = LOG_ONOFFTYPE;
        //在存储配置时存储log
    }
    //重复操作，不存储log
    else if(LogConfig.logvalue != LightConfig.lightingOn)
    {
        llogsaveflag = 0;
        LogConfig.logvalue = (LightConfig.lightingOn<<1)+0x01;
        LogConfig.operatetype = LOG_ONOFFTYPE;
        //在存储配置时存储log
    }
#endif

    LOG_VERBOSE("Turnning: %d time: %d\n", (int32_t)LightConfig.lightingOn, (int32_t)transitiontime);
    StoreConfig();

    int32_t a, b;

#if ANDON_LIGHT_LOG_ENABLE    
    LogConfig.logvalue = (LogConfig.logvalue << 16);
#endif
    if (wiced_is_timer_in_use(&transitionTimer))
    {
        wiced_stop_timer(&transitionTimer);
        //wiced_deinit_timer(&transitionTimer);
    }
    
    if (LightConfig.lightingOn)
    {
        uint32_t temp;
        // off -> on
        a = 0;
        b = 1;
#if LIGHTAI == configLIGHTAIANDONMODE
        // lightnesslogbuf[3] = LightConfig.lightnessLevel;
        if(AutoBrightnessSet.Item.FlagModelOn)
        {
            uint16_t lightnessAi = 0;
            lightnessAi = AutoAdjustBrightness();
            LOG_DEBUG("lght b: %d Ai %d\n",LightConfig.lightnessLevel,lightnessAi);
            if(lightnessAi)
            {
                LightConfig.lightnessLevel  = percentage_to_uint16(lightnessAi);
            }
            LOG_DEBUG("lght a: %d\n",LightConfig.lightnessLevel );
        }
#elif LIGHTAI == configLIGHTAIWYZEMODE
        uint16_t lightnessAi = 0;
        sysclockToWyzeAIClock(ptrsysClock,ptrsysClock);
        lightnessAi = ai_predict(sysClock);
        if(lightnessAi)
        {
            LightConfig.lightnessLevel  = percentage_to_uint16(lightnessAi);
        }
#endif
        ctx.period = transt_to_period(transitiontime);
        if(ctx.period > 20){
            temp = uint16_to_percentage(LightConfig.lightnessLevel);
            temp = (ctx.period-20)*temp/100+20;
            ctx.period = temp;
        }
        ctx.period = ctx.period/LIGHT_TIMER_UINT;
#if ANDON_LIGHT_LOG_ENABLE
        LogConfig.logvalue += uint16_to_percentage(LightConfig.lightnessLevel);
#endif
    }
    else
    {
        a = 1;
        b = 0;
        ctx.period = transt_to_period(transitiontime);
        if(ctx.period > 20)
        {
            uint32_t temp;
            temp = uint16_to_percentage(LightConfig.lightnessLevel);
            temp = (ctx.period-20)*temp/100+20;
            ctx.period = temp;
        }
        ctx.period = ctx.period/LIGHT_TIMER_UINT;
        // adv_pair_disable();
    }
    
#if LIGHTAI == configLIGHTAIANDONMODE
    BrightModelLearning(SystemUTCtimer0,LightConfig.lightingOn?uint16_to_percentage(LightConfig.lightnessLevel):0);
    AutoBrightnessSet.Item.AutoBrightnessSetSave = 1;
#elif LIGHTAI == configLIGHTAIWYZEMODE
    sysclockToWyzeAIClock(ptrsysClock,ptrsysClock);
    ai_train(ptrsysClock,LightConfig.lightingOn?uint16_to_percentage(LightConfig.lightnessLevel):0);
#endif


    ctx.anim = turn_onoff_procedure;
    ctx.tick = 1;
    if(ctx.period == 0)
    {
        ctx.period = 1;
    }
    // > 5 ? transt_to_period(transitiontime) : 5;
    ctx.initiate = a;
    ctx.final = b;
    ctx.PreLightnessLevel = LightConfig.lightnessLevel;
    
    LOG_VERBOSE("TransitionTimer Init \n");
    LOG_VERBOSE("TransitionTimer Init Liht Toggletime = %d\n",ctx.period*LIGHT_TIMER_CB_LENGTH);
    
    // resolution is 10ms
    wiced_start_timer(&transitionTimer, LIGHT_TIMER_CB_LENGTH);
    if(LightConfig.bleonly != CONFIG_BLEONLY){
        vendorSendDevStatus();
    }
    if(LightConfig.bleonly != CONFIG_MESHONLY){
        andonServerSendLightStatus();
    }
    
    advRestartPair();
}
//*****************************************************************************
// 函数名称: LightModelToggleForPowerOff
// 函数描述: 电源掉电上电时的开关处理，
// 函数输入: transitiontime：同mesh中的transitiontime定义  
//          delay：同mesh中的delay定义  
//          Powerstata:0 掉电 1：上电
// 函数返回值: 
//*****************************************************************************/
void LightModelToggleForPowerOff(uint8_t transitiontime, uint16_t delay, uint16_t Powerstata)
{
    static uint16_t overnextPowerOn = 0;

    if((ctx.anim == light_flashing) || (ctx.anim == light_sniffer) || (ctx.anim ==light_flashandsniffer)){
        memset(&ctx, 0, sizeof(ctx));
    }

    //电源掉电仅允许关灯并设置跳过下一个上电的动作
    if(Powerstata==0)
    {
        if(currentCfg.lightingOn == 0)
        {
            overnextPowerOn = 0;
            return;
        }
        overnextPowerOn = 1;
    } 
    //电源上电
    else if(Powerstata==1)
    {
        if(overnextPowerOn == 1)
        {
            overnextPowerOn = 0;
            return;
        }
        if(currentCfg.lightingOn == 1)
        {
            return;
        }
    }
    else
    {
        LOG_WARNING("Powerstata err!!!\n");
        return;
    }
    
    if(wiced_is_timer_in_use(&morsecodeTimer)){
        wiced_stop_timer(&morsecodeTimer);
    }
    morsecodedisplay = WICED_FALSE;

    if (transitiontime == 0xff)
        transitiontime = transition_time;
    LightConfig.lightingOn = !currentCfg.lightingOn;
    ctx.PrePowerTick = 0xFFFF;

    // if(LightConfig.lightingOn == TurnOnOffDelay.onoff){
        TurnOnOffDelay.remaintime = 0;
        TurnOnOffDelay.flag = TURNONOFFDELAYSTOP;
        wiced_stop_timer(&delay_count_Timer_500ms);
    // }

#if ANDON_LIGHT_LOG_ENABLE
    //无效时间，存储当前时间
    if(LogConfig.lasttime == 0)
    {
        llogsaveflag = 0;
        //存储绝对时间
        LogConfig.lasttime = sysTimerGetSystemUTCTime();
        LogConfig.logvalue = sysTimerGetSystemUTCTime();
        LogConfig.operatetype = LOG_TIMETYPE;
        //存储时间LOG
        llog_write();

        LogConfig.logvalue = (LightConfig.lightingOn<<1)+0x01;
        LogConfig.operatetype = LOG_ONOFFTYPE;
        //在存储配置时存储log
    }
    //重复操作，不存储log
    else if(LogConfig.logvalue != LightConfig.lightingOn)
    {
        llogsaveflag = 0;
        LogConfig.logvalue = (LightConfig.lightingOn<<1)+0x01;
        LogConfig.operatetype = LOG_ONOFFTYPE;
        //在存储配置时存储log
    }
#endif

    LOG_VERBOSE("Turnning: %d time: %d\n", (int32_t)LightConfig.lightingOn, (int32_t)transitiontime);
    //StoreConfig();
    if (wiced_is_timer_in_use(&transitionTimer))
    {
        wiced_stop_timer(&transitionTimer);
        //wiced_deinit_timer(&transitionTimer);
    }

    int32_t a, b;

#if ANDON_LIGHT_LOG_ENABLE    
    LogConfig.logvalue = (LogConfig.logvalue << 16);
#endif

    if (LightConfig.lightingOn)
    {
        uint32_t temp;
        // off -> on
        a = 0;
        b = 1;

        // adv_pair_enable();
#if LIGHTAI == configLIGHTAIANDONMODE
        // lightnesslogbuf[3] = LightConfig.lightnessLevel;
        if(AutoBrightnessSet.Item.FlagModelOn)
        {
            uint16_t lightnessAi = 0;
            lightnessAi = AutoAdjustBrightness();
            LOG_DEBUG("lght  b: %d LAi %d\n",LightConfig.lightnessLevel,lightnessAi);
            if(lightnessAi)
            {
                LightConfig.lightnessLevel  = percentage_to_uint16(lightnessAi);
            }
            LOG_DEBUG("lght a: %d\n",LightConfig.lightnessLevel );
        }
#elif LIGHTAI == configLIGHTAIWYZEMODE
        uint16_t lightnessAi = 0;
        sysclockToWyzeAIClock(ptrsysClock,ptrsysClock);
        lightnessAi = ai_predict(sysClock);
        if(lightnessAi)
        {
            LightConfig.lightnessLevel  = percentage_to_uint16(lightnessAi);
        }
#endif

#if ANDON_LIGHT_LOG_ENABLE
        LogConfig.logvalue += uint16_to_percentage(LightConfig.lightnessLevel);
#endif
        ctx.period = transt_to_period(transitiontime);
        if(ctx.period > 20)
        {
            temp = uint16_to_percentage(LightConfig.lightnessLevel);
            temp = (ctx.period-20)*temp/100+20;
            ctx.period = temp;
        }
        ctx.period = ctx.period/LIGHT_TIMER_UINT;
        ctx.anim = turn_onoff_procedure;
        ctx.tick = 1;
        if(ctx.period == 0)
        {
            ctx.period = 1;
            currentCfg.lightingOn = 1;
            currentCfg.lightnessLevel = LightConfig.lightnessLevel;
            LightUpdate();
            memset(&ctx, 0, sizeof(ctx));
        }
        else
        {
            // > 5 ? transt_to_period(transitiontime) : 5;
            ctx.initiate = a;
            ctx.final = b;
            wiced_start_timer(&transitionTimer, LIGHT_TIMER_CB_LENGTH);
        }
    }
    else
    {
        a = 1;
        b = 0;
        ctx.period = 0;
        currentCfg.lightingOn = 0;
        LightUpdate();
        memset(&ctx, 0, sizeof(ctx));
        // adv_pair_disable();
    }
    ctx.PreLightnessLevel = LightConfig.lightnessLevel;
    
#if LIGHTAI == configLIGHTAIANDONMODE
    BrightModelLearning(SystemUTCtimer0,LightConfig.lightingOn?uint16_to_percentage(LightConfig.lightnessLevel):0);
    AutoBrightnessSet.Item.AutoBrightnessSetSave = 1;
#elif LIGHTAI == configLIGHTAIWYZEMODE
    sysclockToWyzeAIClock(ptrsysClock,ptrsysClock);
    ai_train(ptrsysClock,LightConfig.lightingOn?uint16_to_percentage(LightConfig.lightnessLevel):0);
#endif

    //StoreConfigImmediately();
    // //设置上电保持掉电状态时，如在灯掉电时调整完毕后立即存储灯状态，开灯时掉电再上电灯不会亮
    // //原因是掉电的时候检测到掉电(自复位开关)，把灯先给关了
    // StoreConfigDelay();  
    if(LightConfig.bleonly != CONFIG_BLEONLY){
        vendorSendDevStatus();
    }
    if(LightConfig.bleonly != CONFIG_MESHONLY){
        andonServerSendLightStatus();
    }
    advRestartPair();
}

int32_t brightness_procedure(int32_t tick, int32_t period, int32_t initiate, int32_t final)
{
    LOG_VERBOSE("tick: %d  period:%d \n", tick,period);
    if(initiate < percentage_to_uint16(1))
    {
        initiate = percentage_to_uint16(1);
    }
    currentCfg.lightnessLevel = liner_transfer(tick, period, initiate, final);
    if (currentCfg.lightnessLevel < 2)
        currentCfg.lightingOn = 0;
    else
        currentCfg.lightingOn = 1;
    LightUpdate();
    return 0;
}

void LightModelSetBrightness(int8_t percetange, uint8_t transitiontime, uint16_t delay)
{
    if (transitiontime == 0xff)
        transitiontime = transition_time;

    if(wiced_is_timer_in_use(&morsecodeTimer)){
        wiced_stop_timer(&morsecodeTimer);
    }
    morsecodedisplay = WICED_FALSE;

    LOG_DEBUG("transitiontime :%02x\n",transitiontime);
    ctx.PrePowerTick = 0xFFFF;
    //off-->on enable paired
    if(LightConfig.lightingOn == 0)
    {
        // adv_pair_enable();
    }

    TurnOnOffDelay.flag = TURNONOFFDELAYSTOP;
    TurnOnOffDelay.remaintime = 0;
    wiced_stop_timer(&delay_count_Timer_500ms);

#if ANDON_LIGHT_LOG_ENABLE
    //无效时间，存储当前时间
    if(LogConfig.lasttime == 0)
    {
        llogsaveflag = 0;
        //存储绝对时间
        LogConfig.lasttime = sysTimerGetSystemUTCTime();
        LogConfig.logvalue = sysTimerGetSystemUTCTime();
        LogConfig.operatetype = LOG_BRIGHTNESSTYPE;
        //存储时间LOG
        llog_write();

        LogConfig.logvalue = percetange;
        LogConfig.operatetype = LOG_BRIGHTNESSTYPE;
        //在存储配置时存储log
    }
    //重复操作
    else 
    {
        llogsaveflag = 0;
        LogConfig.logvalue = percetange;
        LogConfig.operatetype = LOG_BRIGHTNESSTYPE;
        //在存储配置时存储log
    }
#endif

    LightConfig.lightingOn = 1;
    if (percetange == 0)
        percetange = 1;

    if (ctx.anim == brightness_procedure)
    {
        if (ctx.final == percentage_to_uint16(percetange))
            return;
    }

    LightConfig.lightnessLevel = percentage_to_uint16(percetange);
#if LIGHTAI == configLIGHTAIANDONMODE
    BrightModelLearning(SystemUTCtimer0, percetange);
    AutoBrightnessSet.Item.AutoBrightnessSetSave = 1;
#elif LIGHTAI == configLIGHTAIWYZEMODE
    sysclockToWyzeAIClock(ptrsysClock,ptrsysClock);
    ai_train(ptrsysClock, percetange);
#endif
    //LOG_VERBOSE("SetBrightness = %d  lightnessLevel = %d\n",percetange,LightConfig.lightnessLevel);
    StoreConfig();

    if (wiced_is_timer_in_use(&transitionTimer))
    {
        wiced_stop_timer(&transitionTimer);
        //wiced_deinit_timer(&transitionTimer);
    }
    ctx.anim = brightness_procedure;
    ctx.tick = 1;
    // > 5 ? transt_to_period(transitiontime) : 5;
    ctx.period = transt_to_period(transitiontime);
    if (currentCfg.lightingOn)
    {
        ctx.initiate = currentCfg.lightnessLevel;
    }    
    else
    {
        if(ctx.period > 20)
        {
            uint32_t temp;
            temp = uint16_to_percentage(LightConfig.lightnessLevel);
            temp = (ctx.period-20)*temp/100+20;
            ctx.period = temp;
        }
        LightConfig.lightingOn = 1;
        currentCfg.lightingOn = 1;
        ctx.initiate = 0;
    }
    LOG_DEBUG("ctx.period :%d\n",ctx.period*10);
    ctx.period = ctx.period/LIGHT_TIMER_UINT;
    if(ctx.period == 0)
    {
        ctx.period = 1;
    }
    ctx.final = LightConfig.lightnessLevel;
    ctx.PreLightnessLevel = LightConfig.lightnessLevel;
    
    // LOG_VERBOSE("TransitionTimer Init \n");
    
    wiced_start_timer(&transitionTimer, LIGHT_TIMER_CB_LENGTH);
    if(LightConfig.bleonly != CONFIG_BLEONLY){
        vendorSendDevStatus();
    }
    if(LightConfig.bleonly != CONFIG_MESHONLY){
        andonServerSendLightStatus();
    }
    advRestartPair();
}

void LightModelDeltaBrightness(int8_t delta_in, uint8_t transitiontime, uint16_t delay)
{
    int32_t delta_raw = 0;
    uint8_t target_onoff = 1;
    int16_t delta;
    uint16_t deltastep;

    if (transitiontime == 0xff)
        transitiontime = transition_time;
    ctx.PrePowerTick = 0xFFFF;
    
    transitiontime = 0;
    if(morsecodedisplay == WICED_TRUE){
        if(wiced_is_timer_in_use(&morsecodeTimer)){
            wiced_stop_timer(&morsecodeTimer);
        }
        if(!LightConfig.lightingOn){
            if(delta_in<0){
                led_controller_status_update(0, LightConfig.lightnessLevel, LightConfig.lightnessCTL);
            }
        }else{
            led_controller_status_update(1, LightConfig.lightnessLevel, LightConfig.lightnessCTL);
        }
    }
    morsecodedisplay = WICED_FALSE;

    TurnOnOffDelay.flag = TURNONOFFDELAYSTOP;
    TurnOnOffDelay.remaintime = 0;
    wiced_stop_timer(&delay_count_Timer_500ms);

    LOG_DEBUG("Delata: %d  transitiontime: %d\n",delta_in,transitiontime);
    if(delta_in == 0){
        return;
    }

    if(delta_in > 49){
        delta_in = 50;
    }else if(delta_in < -49){
        delta_in = -50;
    }
    if(LightConfig.brightdeltastep < CONFIG_DELTASTEP_MIN){
        LightConfig.brightdeltastep = CONFIG_DELTASTEP_MIN;
    }else if (LightConfig.brightdeltastep > CONFIG_DELTASTEP_MAX){
        LightConfig.brightdeltastep = CONFIG_DELTASTEP_MAX;
    }
    deltastep = 99/((LightConfig.brightdeltastep-1));
    deltastep +=  (99%((LightConfig.brightdeltastep-1))?1:0);
    delta = delta_in * deltastep;
    if(delta > 100)
    {
        delta = 100;
    }
    else if(delta < -100)
    {
        delta = -100;
    }

    if (delta > 0)
    {
        target_onoff = 1;
        if(currentCfg.lightingOn != 0)
        {
            delta_raw = percentage_to_uint16(delta);
            //正在关灯过程中调整亮度，在当前显示亮度基础上进行调整
            //if(LightConfig.lightingOn == 0)
            {
                delta_raw += currentCfg.lightnessLevel;
            }
            // else
            // {
            //     delta_raw += LightConfig.lightnessLevel;
            // }
        }
        else
        {
            // // adv_pair_enable();
            // if(delta != 100)
            // {
            //     //关灯情况下非100%的调整量，调整量减9%
            //     delta_raw -= percentage_to_uint16(9);
            //     if(delta_raw < percentage_to_uint16(1))
            //     {
            //         delta_raw = percentage_to_uint16(1);
            //     }
            // }
            if(delta != 100){
                delta = delta + 1 - deltastep;
            }
            delta_raw = percentage_to_uint16(delta);
            // if(delta_in == 1){
            //     delta_raw = percentage_to_uint16(1);
            // }
        }
        if (delta_raw > 65535)
            delta_raw = 65535;
        LightConfig.lightnessLevel = delta_raw;
        LightConfig.lightingOn = 1;
        //int32_t temp = delta_raw;
        //temp *= transitiontime;
        //temp /= percentage_to_uint16(delta);
        //transition_time = temp;
        if (currentCfg.lightingOn == 0)
        {
            //currentCfg.lightingOn = 1;
            currentCfg.lightnessLevel = 656;
        }

        // if (LightConfig.lightingOn == 0)
        // {
        //     LightConfig.lightingOn = 1;
        //     // LightConfig.lightnessLevel = 656;
        // }
        //LOG_VERBOSE("Delata: %d\n",delta);
    }
    else
    {
        delta_raw = percentage_to_uint16(0 - delta);
        //if ((!currentCfg.lightingOn) || (!LightConfig.lightingOn))
        if ((!LightConfig.lightingOn) && (TurnOnOffDelay.flag == TURNONOFFDELAYSTOP)){    
            return;
        }
        //正在关灯过程中调整亮度，在当前显示亮度基础上进行调整
        //if(LightConfig.lightingOn == 0)
        {
            if(currentCfg.lightnessLevel < (delta_raw+655/2))
            {
                //关灯时，如果当前亮度>1%并且是调整1格关灯，则最低亮度置为1% 
                //如果快速旋转关灯&当前亮度>1%，则最低亮度置为1% 
                //旋转到最后一档亮度&当前亮度>1%，则最低亮度置为1% 
                // if( ((delta_in == -1) || (delta_in == -50) || (currentCfg.lightnessLevel < (delta_raw+percentage_to_uint16(deltastep)))) 
                //     && (uint16_to_percentage(currentCfg.lightnessLevel) > 1))
                if(uint16_to_percentage(currentCfg.lightnessLevel) > 1)
                {
                    LightConfig.lightnessLevel = percentage_to_uint16(1);
                    if(LightConfig.lightingOn == 0){
                        LightConfig.lightingOn = 1;
                    }
                }
                else
                {
                    target_onoff = 0;
                    LightConfig.lightingOn  = 0;
                    // transitiontime = 8;
                }
            }
            else
            {
                LightConfig.lightnessLevel = currentCfg.lightnessLevel - delta_raw;
                if(LightConfig.lightingOn == 0){
                    LightConfig.lightingOn = 1;
                }
            }
        }
        // else
        // {
        //     if(LightConfig.lightnessLevel < (delta_raw-655))
        //     {
        //         //关灯时，如果当前亮度>1%并且是调整1格关灯，则最低亮度置为1%
        //         if((delta == -3) && (uint16_to_percentage(LightConfig.lightnessLevel) > 1))
        //         {
        //             LightConfig.lightnessLevel = percentage_to_uint16(1);
        //         }
        //         else
        //         {
        //             target_onoff = 0;
        //             // adv_pair_disable();
        //             LightConfig.lightingOn  = 0;
        //             transitiontime = 8;
        //         }
        //     }
        //     else
        //     {
        //         LightConfig.lightnessLevel -= delta_raw;
        //     }
        // }
    }
    LOG_DEBUG("Delata LightConfig.lightnessLevel: %d  \n",LightConfig.lightnessLevel);

#if ANDON_LIGHT_LOG_ENABLE
    //无效时间，存储当前时间， 首次发生调整时间未进行放大
    if(LogConfig.lasttime == 0)
    {
        llogsaveflag = 0;
        //存储绝对时间
        LogConfig.lasttime = sysTimerGetSystemUTCTime();
        LogConfig.logvalue = sysTimerGetSystemUTCTime();
        LogConfig.operatetype = LOG_TIMETYPE;
        //存储时间LOG
        llog_write();

        LogConfig.logvalue = (target_onoff?(uint16_to_percentage(LightConfig.lightnessLevel)<<16):0) + transt_to_period(transitiontime);
        LogConfig.operatetype = LOG_DETLATYPE_BRIGHTNESS;
        //在存储配置时存储log
    }
    //重复操作，存储变化量和时间
    else if(LogConfig.operatetype == LOG_DETLATYPE_BRIGHTNESS)
    {
        uint32_t temp = 0;
        llogsaveflag = 0;
        
        LogConfig.logvalue = (target_onoff?(uint16_to_percentage(LightConfig.lightnessLevel)<<16):0);
        temp = LogConfig.logvalue&0xFFFF;
        temp += transt_to_period(transitiontime)*4/5;
        if(temp > 6000)
        {
            temp = 6000;
        }
        LogConfig.logvalue += temp;
        LogConfig.operatetype = LOG_DETLATYPE_BRIGHTNESS;
        //在存储配置时存储log
    }
    else
    {
        llogsaveflag = 0;
        LogConfig.logvalue = (target_onoff?(uint16_to_percentage(LightConfig.lightnessLevel)<<16):0) + transt_to_period(transitiontime);
        LogConfig.operatetype = LOG_DETLATYPE_BRIGHTNESS;
        //在存储配置时存储log
    }
#endif

#if LIGHTAI == configLIGHTAIANDONMODE
    BrightModelLearning(SystemUTCtimer0, target_onoff ? uint16_to_percentage(LightConfig.lightnessLevel):0);
    AutoBrightnessSet.Item.AutoBrightnessSetSave = 1;
#elif LIGHTAI == configLIGHTAIWYZEMODE
    sysclockToWyzeAIClock(ptrsysClock,ptrsysClock);
    ai_train(ptrsysClock, target_onoff ? uint16_to_percentage(LightConfig.lightnessLevel):0);
#endif
    StoreConfig();
    if (wiced_is_timer_in_use(&transitionTimer))
    {
        wiced_stop_timer(&transitionTimer);
        //wiced_deinit_timer(&transitionTimer);
    }

    ctx.anim = brightness_procedure;
    ctx.tick = 1;
    ctx.period = transt_to_period(transitiontime);
    if((currentCfg.lightingOn == 0) || (target_onoff == 0))
    {
        if(ctx.period > 20)
        {
            uint32_t temp;
            temp = uint16_to_percentage(LightConfig.lightnessLevel);
            temp = (ctx.period-20)*temp/100+20;
            ctx.period = temp;
        }
    }
    ctx.period = ctx.period/LIGHT_TIMER_UINT;
    if(ctx.period == 0)
    {
        ctx.period = 1;
    }

    // > 5 ? transt_to_period(transitiontime) : 5;
    if (currentCfg.lightingOn)
        ctx.initiate = currentCfg.lightnessLevel;
    else
    {
        LightConfig.lightingOn = 1;
        currentCfg.lightingOn = 1;
        ctx.initiate = 0;
    }

    // if(LightConfig.lightingOn == TurnOnOffDelay.onoff){
    //     TurnOnOffDelay.flag = TURNONOFFDELAYSTOP;
    //     TurnOnOffDelay.remaintime = 0;
    // }

    ctx.final = target_onoff ? LightConfig.lightnessLevel : 0;
    ctx.PreLightnessLevel = LightConfig.lightnessLevel;
    LOG_DEBUG("ctx.period: %d\n",ctx.period);
    LOG_DEBUG("ctx.period: %d\n",ctx.period*LIGHT_TIMER_UINT);
    LOG_DEBUG("ctx.initiate: %d\n",ctx.initiate);
    LOG_DEBUG("ctx.final: %d\n",ctx.final);
    
    LOG_VERBOSE("TransitionTimer Init \n");

    //wiced_init_timer(&transitionTimer, lightModelCb, 0, WICED_MILLI_SECONDS_PERIODIC_TIMER);
    // resolution is 10ms
    wiced_start_timer(&transitionTimer, LIGHT_TIMER_CB_LENGTH);
    if(LightConfig.bleonly != CONFIG_BLEONLY){
        vendorSendDevStatus();
    }
    if(LightConfig.bleonly != CONFIG_MESHONLY){
        andonServerSendLightStatus();
    }
    advRestartPair();
}

void LightModelSetTemperature(int8_t percetange, uint8_t transitiontime, uint16_t delay)
{
    //TODO: 设置色温
}

void LightModelDeltaTemperature(int8_t percetange, uint8_t transitiontime, uint16_t delay)
{
    //TODO: 变化色温
}


int32_t ActionDelayFlash_procedure(int32_t tick, int32_t period, int32_t initiate, int32_t final)
{
    // turnning off
    if (tick < period)
    {
        period += (300/LIGHT_TIMER_UINT); //增加3s, 周期大于2.5秒，按延迟关灯执行
        currentCfg.lightnessLevel = polyline_transfer3(tick, period, initiate, 1);
    }
    else
    {
        currentCfg.lightnessLevel =  (initiate+final)/3;
        memset(&ctx, 0, sizeof(ctx));
    }
    LightUpdate();
    return 0;
}

void LightModeActionDelayFlash(void)
{
    // if (wiced_is_timer_in_use(&transitionTimer))
    // {
    //     wiced_stop_timer(&transitionTimer);
    //     //wiced_deinit_timer(&transitionTimer);
    // }
    // ctx.anim = ActionDelayFlash_procedure;
    // ctx.tick = 1;
    // // > 5 ? transt_to_period(transitiontime) : 5;
    // ctx.period = transt_to_period(0x03);
    
    // LOG_DEBUG("ctx.period :%d\n",ctx.period*10);
    // ctx.period = ctx.period/LIGHT_TIMER_UINT;
    // if(ctx.period == 0)
    // {
    //     ctx.period = 1;
    // }
    // ctx.initiate = currentCfg.lightnessLevel;
    // ctx.final = 0;
    // ctx.PreLightnessLevel = LightConfig.lightnessLevel;
    // wiced_start_timer(&transitionTimer, LIGHT_TIMER_CB_LENGTH);
    Light_Sniffer1_t snifferinfo;
    snifferinfo.cycle = 100;
    snifferinfo.times = 1;
    snifferinfo.direction = 0;
    snifferinfo.maxlevel = snifferinfo.final = uint16_to_percentage(currentCfg.lightnessLevel);
    snifferinfo.issaved = 0;
    snifferinfo.minlevel = (snifferinfo.final+5)*6/10; 
    if(snifferinfo.minlevel < 1){
        snifferinfo.minlevel = 1;
    }
    LightSniffer1(snifferinfo);
}

void LightModelTurnToggle(int8_t res, uint8_t transitiontime, uint16_t delay)
{
    LightModelToggle(0,0,delay);
}
void LightModelTurnOn(int8_t res, uint8_t transitiontime, uint16_t delay)
{
    LightModelTurn(1,0,delay);
}
void LightModelTurnOff(int8_t res,uint8_t transitiontime, uint16_t delay)
{
    LightModelTurn(0,0,delay);
}

void LightModelTurnOffDelay(int8_t res,uint8_t transitiontime, uint16_t delay)
{
    LOG_DEBUG("LightModelTurnOffDelay........ offdelaytime %d\n",LightConfig.offdelaytime);
    if(LightConfig.offdelayset == 0) {
        return;
    }
    advRestartPair();
    TurnOnOffDelay.onoff = !currentCfg.lightingOn;
    LightModelToggle(0,transitiontime,0xFF);
}

void delay_count_Timer_cb(TIMER_PARAM_TYPE parameter)
{
//未处于倒计时过程中
    if(TurnOnOffDelay.flag == TURNONOFFDELAYRUNING){
        if(TurnOnOffDelay.remaintime){
            TurnOnOffDelay.remaintime--;
            LOG_DEBUG("delay remaintime %d\n",TurnOnOffDelay.remaintime);
            if(1 == TurnOnOffDelay.remaintime)
            TurnOnOffDelay.remaintime--;
            if(0 == TurnOnOffDelay.remaintime){
                if(TurnOnOffDelay.onoff < 2){
                    LightModelTurn(TurnOnOffDelay.onoff,0x0,0);
                }
                TurnOnOffDelay.flag = TURNONOFFDELAYSTOP;
                wiced_stop_timer(&delay_count_Timer_500ms);
            }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               
        }else{
            TurnOnOffDelay.flag = TURNONOFFDELAYSTOP;
        }
    }else{
        TurnOnOffDelay.remaintime = 0;
    }

}

void lightSetDelayOnOffTimer(uint8_t onoff, uint16_t delaytime)
{
    advRestartPair();
    if(TurnOnOffDelay.flag != TURNONOFFDELAYSTOP){
        return;
    }
    TurnOnOffDelay.onoff = onoff;
    if((delaytime > CONFIG_OFFDELAY_MIN) && (delaytime < CONFIG_OFFDELAY_MAX)){
        TurnOnOffDelay.settime = delaytime*4;     
        TurnOnOffDelay.remaintime = delaytime*4;  
        TurnOnOffDelay.flag = TURNONOFFDELAYRUNING;  
        wiced_start_timer(&delay_count_Timer_500ms, 250);
    }else{
        TurnOnOffDelay.settime = CONFIG_DEFAULT_OFFDELAY*4;     
        TurnOnOffDelay.remaintime = CONFIG_DEFAULT_OFFDELAY*4;  
        TurnOnOffDelay.flag = TURNONOFFDELAYRUNING;  
        wiced_start_timer(&delay_count_Timer_500ms, 250);
    }

    if(currentCfg.lightingOn){
        LightModeActionDelayFlash();
    }
    LOG_DEBUG("Turn %d delay %d\n",TurnOnOffDelay.onoff,TurnOnOffDelay.settime);
}

void lightGetDelayOnOffTimer(uint8_t *onoff, uint16_t *settime, uint16_t *remaintime, uint8_t *lightness)
{
    *settime = TurnOnOffDelay.settime/4;
    *lightness = LightConfig.lightingOn?uint16_to_percentage(LightConfig.lightnessLevel):0;
    if(TurnOnOffDelay.flag == TURNONOFFDELAYSTOP){
        TurnOnOffDelay.remaintime == 0;
    }
    *remaintime = TurnOnOffDelay.remaintime/4;
    if(TurnOnOffDelay.remaintime == 0){
        *onoff = 3;
    }else{
        *onoff = TurnOnOffDelay.onoff;
    }
    LOG_DEBUG("Turn %d delay %d onoff %d\n",TurnOnOffDelay.onoff,TurnOnOffDelay.remaintime);
}

void MorseCodeTimerCb(TIMER_PARAM_TYPE parameter)
{
    uint16_t timelength;

    wiced_stop_timer(&morsecodeTimer);
    if(morsecodedisplay == WICED_FALSE){
        return;
    }
    LOG_DEBUG("morsecodemaskdisplaycnt: %02x \n",morsecodemaskdisplaycnt);

    if(morsecodemaskdisplayallcnt == 0){
        return;
    }
    if((morsecodemaskdisplaycnt&0x7F) >= morsecodemaskdisplayallcnt){
        morsecodemaskdisplaycnt = 0;
    }

    if(morsecodemaskdisplaycnt&0x80){
        timelength = MORSESPACE_LENGTH;
        morsecodemaskdisplaycnt &= 0x7F;
        led_controller_status_update(0, percentage_to_uint16(50), LightConfig.lightnessCTL);
    }else{
        switch(morsecodemasklist[morsecodemaskdisplaycnt]){
            case MORSEDI_MASK:
                timelength = MORSEDI_LENGTH;
                led_controller_status_update(1, percentage_to_uint16(50), LightConfig.lightnessCTL);
            break;
            case MORSEDA_MASK:
                timelength = MORSEDA_LENGTH;
                led_controller_status_update(1, percentage_to_uint16(50), LightConfig.lightnessCTL);
            break;
            case MORSECHAR_MASK:
                timelength = MORSECHAR_LENGTH;
                led_controller_status_update(0, percentage_to_uint16(50), LightConfig.lightnessCTL);
            break;
            case MORSEWORD_MASK:
                timelength = MORSEWORD_LENGTH;
                led_controller_status_update(0, percentage_to_uint16(50), LightConfig.lightnessCTL);
            break;
        }
        morsecodemaskdisplaycnt ++;
        if( (morsecodemasklist[morsecodemaskdisplaycnt] != MORSECHAR_MASK) 
            && (morsecodemasklist[morsecodemaskdisplaycnt] != MORSEWORD_MASK) 
            && (morsecodemasklist[morsecodemaskdisplaycnt-1] != MORSECHAR_MASK) 
            && (morsecodemasklist[morsecodemaskdisplaycnt-1] != MORSEWORD_MASK) )
        {
            morsecodemaskdisplaycnt |= 0x80;
        }
    }
    //启动定时器
    wiced_start_timer(&morsecodeTimer,timelength*LIGHT_MORSECODE_T);
}

void LightModelMorseCodeDisplay(int8_t res,uint8_t transitiontime, uint16_t delay)
{
    uint8_t codeindex;
    uint8_t codemaskbit;
    uint16_t timelength;
    uint8_t *codemaskptr;
    
    TurnOnOffDelay.remaintime = 0;
    TurnOnOffDelay.flag = TURNONOFFDELAYSTOP;
    wiced_stop_timer(&delay_count_Timer_500ms);
    advRestartPair();

    memset(&ctx, 0, sizeof(ctx));
    LightConfig.lightnessLevel = currentCfg.lightnessLevel;
    LightConfig.lightingOn = currentCfg.lightingOn;

    LOG_DEBUG("LightMorseDisplay........ CODE: %s\n",morsecodelist);
    //todo 停掉定时器
    if(wiced_is_timer_in_use(&morsecodeTimer)){
        wiced_stop_timer(&morsecodeTimer);
    }
    morsecodedisplay = WICED_TRUE;
    morsecodemaskdisplayallcnt = 0;
    for(uint8_t i=0; i<strlen(morsecodelist)&&i<MORSE_MAX_CHAR;i++){
        if(morsecodelist[i] == ' '){
            morsecodemasklist[morsecodemaskdisplayallcnt++] = MORSECHAR_MASK;
        }else if((morsecodelist[i] > ('A'-1)) && (morsecodelist[i] < ('Z'+1))) {
            codeindex = morsecodelist[i]-'A';
            codemaskbit = Morsecodemask[codeindex][0];
            codemaskptr = (uint8_t*)(&Morsecodemask[codeindex][1]);
            for(uint8_t j=0;j<codemaskbit&&morsecodemaskdisplayallcnt<MORSE_MAX_BIT; j++){
                //morsecodemasklist[j+morsecodemaskdisplayallcnt] = *(codemaskptr+j);
                morsecodemasklist[morsecodemaskdisplayallcnt++] = *(codemaskptr+j);
            }
            if(morsecodemaskdisplayallcnt<MORSE_MAX_BIT){
                morsecodemasklist[morsecodemaskdisplayallcnt++] = MORSECHAR_MASK;
            }
        }else{
            continue;
        }
        if(!(morsecodemaskdisplayallcnt < MORSE_MAX_BIT)){
            morsecodemaskdisplayallcnt = MORSE_MAX_BIT;
            break;
        }
        
    }
    morsecodemasklist[morsecodemaskdisplayallcnt-1] = MORSEWORD_MASK;
    morsecodemaskdisplaycnt = 0;
    
    WICED_BT_TRACE_ARRAY(morsecodemasklist,morsecodemaskdisplayallcnt,"MORSE CODE MASK:\n");

    //点亮灯
    if((morsecodemaskdisplayallcnt == 0) || (morsecodemaskdisplaycnt >= morsecodemaskdisplayallcnt)){
        return;
    }

    if(morsecodemaskdisplaycnt&0x80){
        timelength = MORSESPACE_LENGTH;
        morsecodemaskdisplaycnt &= 0x7F;
        led_controller_status_update(0, percentage_to_uint16(50), LightConfig.lightnessCTL);
    }else{
        switch(morsecodemasklist[morsecodemaskdisplaycnt]){
            case MORSEDI_MASK:
                timelength = MORSEDI_LENGTH;
            break;
            case MORSEDA_MASK:
                timelength = MORSEDA_LENGTH;
            break;
            case MORSECHAR_MASK:
                timelength = MORSECHAR_LENGTH;
            break;
            case MORSEWORD_MASK:
                timelength = MORSEWORD_LENGTH;
            break;
        }
        morsecodemaskdisplaycnt ++;
        if( (morsecodemasklist[morsecodemaskdisplaycnt] != MORSECHAR_MASK) 
            && (morsecodemasklist[morsecodemaskdisplaycnt] != MORSECHAR_MASK) )
        {
            morsecodemaskdisplaycnt |= 0x80;
        }
        led_controller_status_update(1, percentage_to_uint16(50), LightConfig.lightnessCTL);
    }
    //启动定时器
    wiced_start_timer(&morsecodeTimer,timelength*LIGHT_MORSECODE_T);
}

void lightStartAction(uint32_t startAction){
    LOG_DEBUG("startAction %04x",startAction);
    if(LightConfig.offdelayset == 0){
        return;
    }
    if(startAction&lightSTARTTRANSIONTIME){
        LightModelToggle(0,0,0xFF);
    }else{
        return;
    }
}

void lightCancleAction(uint32_t canclemask){

    LOG_DEBUG("canclemask %04x",canclemask);
    
    if(canclemask&lightCANCLETRANSIONTIME){ //取消渐变过程
        if(LightConfig.offdelayset == 0){
            return;
        }
        memset(&ctx, 0, sizeof(ctx));
    }else if(canclemask&lightCANCLECOUNTDOWN){
        TurnOnOffDelay.remaintime = 0; 
        TurnOnOffDelay.flag = TURNONOFFDELAYSTOP; 
        wiced_stop_timer(&delay_count_Timer_500ms);
        return;
    }else{
        return;
    }
    LightConfig.lightnessLevel = currentCfg.lightnessLevel;
    LightConfig.lightingOn = currentCfg.lightingOn;

    if(LightConfig.lightnessLevel < 656)
        LightConfig.lightnessLevel = 656;
#if LIGHTAI == configLIGHTAIANDONMODE
    BrightModelLearning(SystemUTCtimer0, LightConfig.lightingOn ? uint16_to_percentage(LightConfig.lightnessLevel):0);
    AutoBrightnessSet.Item.AutoBrightnessSetSave = 1;
#elif LIGHTAI == configLIGHTAIWYZEMODE
    sysclockToWyzeAIClock(ptrsysClock,ptrsysClock);
    ai_train(ptrsysClock, LightConfig.lightingOn ? uint16_to_percentage(LightConfig.lightnessLevel):0);
#endif
    StoreConfig();

}


int32_t light_flashing(int32_t tick, int32_t period, int32_t initiate, int32_t final)
{
    if (tick % flashing_cycle == 1)
    {
        // #if CHIP_SCHEME == CHIP_DEVKIT
        // extern wiced_platform_led_config_t platform_led[];
        //     wiced_hal_gpio_set_pin_output((uint32_t)*platform_led[WICED_PLATFORM_LED_2].gpio, GPIO_PIN_OUTPUT_LOW);
        // #endif
        currentCfg.lightingOn = !initiate;
        LOG_VERBOSE("light_flashing 1\n");
        LightUpdate();
    }
    else if (tick % flashing_cycle == flashing_cycle / 2)
    {
        // #if CHIP_SCHEME == CHIP_DEVKIT
        // extern wiced_platform_led_config_t platform_led[];
        //     wiced_hal_gpio_set_pin_output((uint32_t)*platform_led[WICED_PLATFORM_LED_2].gpio, GPIO_PIN_OUTPUT_HIGH);
        // #endif
        currentCfg.lightingOn = initiate;
        LOG_VERBOSE("light_flashing 2\n");
        LightUpdate();
    }
    else if(tick == period)
    {
        if(ctx.final != 0)
        {
            currentCfg.lightingOn = 1;
            currentCfg.lightnessLevel = ctx.final;
        }
        else
        {
            currentCfg.lightingOn = LightConfig.lightingOn = 0;
            currentCfg.lightnessLevel = LightConfig.lightnessLevel;
        }
        llogsaveflag = 1;
        StoreConfig();
        memset(&ctx, 0, sizeof(ctx));
        LOG_VERBOSE("light_flashing end\n");
        LightUpdate();
    }
    if(period / flashing_cycle > 0x7EFF)
    {
        ctx.tick = ctx.tick % flashing_cycle + flashing_cycle;
    }
    return 0;
}

void LightFlash(uint16_t cycle, uint16_t times,uint8_t flashbrightness, uint8_t finalbrightness,uint8_t issaved)
{
    if(wiced_is_timer_in_use(&morsecodeTimer)){
        wiced_stop_timer(&morsecodeTimer);
    }
    morsecodedisplay = WICED_FALSE;

    TurnOnOffDelay.remaintime = 0;
    TurnOnOffDelay.flag = TURNONOFFDELAYSTOP;
    wiced_stop_timer(&delay_count_Timer_500ms);
    
    flashing_cycle = cycle/LIGHT_TIMER_UINT;
    if(flashing_cycle < 2)
    {
        flashing_cycle = 2;
    }
    flashing_times = times;
    ctx.PrePowerTick = 0xFFFF;

    if (times)
    {
        if (wiced_is_timer_in_use(&transitionTimer)){
            wiced_stop_timer(&transitionTimer);
            //wiced_deinit_timer(&transitionTimer);
        }
        if(0 == flashbrightness){
            currentCfg.lightnessLevel = LightConfig.lightnessLevel;
        }else{
            currentCfg.lightnessLevel = percentage_to_uint16(flashbrightness);
        }
        ctx.anim = light_flashing;
        ctx.tick = 1;
        ctx.period = flashing_cycle * times;
        ctx.initiate = LightConfig.lightingOn;
        ctx.PreLightnessLevel = currentCfg.lightnessLevel;
        // LightConfig.lightingOn = 1;
        if(0 != finalbrightness)
        {
            //ctx.final = LightConfig.lightnessLevel = percentage_to_uint16(finalbrightness);
            ctx.final = percentage_to_uint16(finalbrightness);
            //StoreConfig();
        }
        else
        {
            //之前如果是开灯
            if(LightConfig.lightingOn == 1)
            {
                ctx.final = LightConfig.lightnessLevel;
            }
            else
            {
                ctx.final = 0;
            }
        }

        LOG_VERBOSE("TransitionTimer Init Liht flash cycle = %d  times = %d\n",flashing_cycle*LIGHT_TIMER_CB_LENGTH,times);
        LOG_VERBOSE("timer start\n");

        
        //wiced_init_timer(&transitionTimer, lightModelCb, 0, WICED_MILLI_SECONDS_PERIODIC_TIMER);
        wiced_start_timer(&transitionTimer, LIGHT_TIMER_CB_LENGTH);
        //StoreConfig();
        if(issaved)
        {
            LightConfig.lightingOn = (ctx.final>654)?1:0;
            LightConfig.lightnessLevel = ctx.final;    
        }
    }
    else
    {
        memset(&ctx, 0, sizeof(ctx));
        if(finalbrightness == 0)
        {
            currentCfg = LightConfig;
            ctx.final  = LightConfig.lightnessLevel;
        }
        else
        {
            currentCfg.lightingOn = 1;
            ctx.final = currentCfg.lightnessLevel = percentage_to_uint16(finalbrightness);
        }
        
        LOG_VERBOSE("LightFlash Deinit \n");
        LightUpdate();
        if(issaved)
        {
            LightConfig.lightingOn = (ctx.final>654)?1:0;
            LightConfig.lightnessLevel = ctx.final; 
            // llogsaveflag = 1; 
            // StoreConfig(); 
        }
    }
}

Light_Sniffer1_t sniffer1set;

int32_t light_sniffer1(int32_t tick, int32_t period, int32_t initiate, int32_t final)
{
    currentCfg.lightingOn = 1;
    if ((tick % sniffer_cycle) <  sniffer_cycle / 2)
    {
        if(sniffer_direction)
        {
            currentCfg.lightnessLevel = liner_transfer(tick%(sniffer_cycle/2)+1, sniffer_cycle/2, sniffer1set.minlevel, sniffer1set.maxlevel);
        }
        else
        {
            currentCfg.lightnessLevel = liner_transfer(tick%(sniffer_cycle/2)+1, sniffer_cycle/2, sniffer1set.maxlevel, sniffer1set.minlevel);
        }
    }
    else
    {
        if(sniffer_direction)
        {
            currentCfg.lightnessLevel = liner_transfer(tick%(sniffer_cycle/2)+1, sniffer_cycle/2, sniffer1set.maxlevel, sniffer1set.minlevel);
        }
        else
        {
            currentCfg.lightnessLevel = liner_transfer(tick%(sniffer_cycle/2)+1, sniffer_cycle/2, sniffer1set.minlevel, sniffer1set.maxlevel);
        }
        
    }
    if(tick >= (period-sniffer_cycle))
    {
        if(sniffer1set.direction == 1)
        {
            if(final == 0)
            {
                //LightConfig.lightingOn = currentCfg.lightingOn = 0;
                currentCfg.lightingOn = 0;
                ctx.tick  = ctx.period; 
            }
            else if(final <= currentCfg.lightnessLevel )
            {
                //LightConfig.lightingOn = 1;
                ctx.tick  = ctx.period;
                //LightConfig.lightnessLevel = ctx.final;
            }
        }
        else
        {
            if((final == 0) && (currentCfg.lightnessLevel < 657))
            {
                //LightConfig.lightingOn = currentCfg.lightingOn = 0;
                currentCfg.lightingOn = 0;
                ctx.tick  = ctx.period; 
            }
            else if(final >= currentCfg.lightnessLevel )
            {
                //LightConfig.lightingOn = 1;
                ctx.tick  = ctx.period;
                //LightConfig.lightnessLevel = ctx.final;
            }
        }
        LOG_DEBUG("lightnessLevel :%d\n",currentCfg.lightnessLevel);
        if(ctx.tick  == ctx.period)
        {
            llogsaveflag = 1;
            StoreConfig();
            memset(&ctx, 0, sizeof(ctx));
        }
    }
    LightUpdate();
    if(period / sniffer_cycle > 0x7EFF)
    {
        ctx.tick = ctx.tick % sniffer_cycle + sniffer_cycle;
    }
    return 0;
}

void LightSniffer1(Light_Sniffer1_t snifferinfo )
{
    if(wiced_is_timer_in_use(&morsecodeTimer)){
        wiced_stop_timer(&morsecodeTimer);
    }
    morsecodedisplay = WICED_FALSE;
    sniffer1set = snifferinfo;
    if(sniffer1set.maxlevel > 100){
        sniffer1set.maxlevel = 100;
    }
    if(sniffer1set.maxlevel<1){
        sniffer1set.maxlevel = 1;
    }
    if(sniffer1set.minlevel > sniffer1set.maxlevel){
        sniffer1set.minlevel = sniffer1set.maxlevel-1; 
    }
    if(sniffer1set.final > sniffer1set.maxlevel){
        sniffer1set.final = sniffer1set.maxlevel;
    }else if(sniffer1set.final < sniffer1set.minlevel){
        sniffer1set.final = sniffer1set.minlevel;
    }
    // TurnOnOffDelay.remaintime = 0;
    // TurnOnOffDelay.flag = TURNONOFFDELAYSTOP;

    sniffer_cycle = sniffer1set.cycle/LIGHT_TIMER_UINT;
    sniffer1set.maxlevel = percentage_to_uint16(sniffer1set.maxlevel);
    sniffer1set.minlevel = percentage_to_uint16(sniffer1set.minlevel);
    sniffer1set.final = percentage_to_uint16(sniffer1set.final);

    //sniffer_times = times;
    if(sniffer_cycle < 2)
    {
        sniffer_cycle = 2;
    }
    ctx.PrePowerTick = 0xFFFF;

    if (sniffer1set.times)
    {
        if (wiced_is_timer_in_use(&transitionTimer))
        {
            wiced_stop_timer(&transitionTimer);
            //wiced_deinit_timer(&transitionTimer);
        }
        currentCfg.lightnessLevel = LightConfig.lightnessLevel;
        ctx.anim = light_sniffer1;
        ctx.tick = 1;
        ctx.period = sniffer_cycle * (sniffer1set.times+1);
        ctx.initiate = LightConfig.lightingOn;
        ctx.final = sniffer1set.final;
        // LightConfig.lightingOn = 1;
        // if(ctx.final)
        // {
        //     LightConfig.lightnessLevel = ctx.final;
        // }
        // else
        // {
        //     LightConfig.lightingOn = 0;
        // }
        LOG_VERBOSE("TransitionTimer Init \n");

        //wiced_init_timer(&transitionTimer, lightModelCb, 0, WICED_MILLI_SECONDS_PERIODIC_TIMER);
        wiced_start_timer(&transitionTimer, LIGHT_TIMER_CB_LENGTH);
    }
    else  //呼吸时循环次数不能为0，为0直接将灯亮度置为之前保存的亮度后返回
    {
        memset(&ctx, 0, sizeof(ctx));
        currentCfg = LightConfig;
        
        LOG_VERBOSE("LightSniffer Deinit \n");
        LightUpdate();
        return;
    }
    if(snifferinfo.issaved)
    {
        if((ctx.final == 0) || (ctx.final < 657)){
            LightConfig.lightingOn = 0;
        }else{
            LightConfig.lightingOn = 1;
            LightConfig.lightnessLevel = ctx.final;
        }
    }
}

int32_t light_sniffer(int32_t tick, int32_t period, int32_t initiate, int32_t final)
{
    currentCfg.lightingOn = 1;
    if ((tick % sniffer_cycle) <  sniffer_cycle / 2)
    {
        if(sniffer_direction)
        {
            currentCfg.lightnessLevel = liner_transfer(tick%(sniffer_cycle/2)+1, sniffer_cycle/2, 656, 65535);
        }
        else
        {
            currentCfg.lightnessLevel = liner_transfer(tick%(sniffer_cycle/2)+1, sniffer_cycle/2, 65535, 656);
        }
    }
    else
    {
        if(sniffer_direction)
        {
            currentCfg.lightnessLevel = liner_transfer(tick%(sniffer_cycle/2)+1, sniffer_cycle/2, 65535, 656);
        }
        else
        {
            currentCfg.lightnessLevel = liner_transfer(tick%(sniffer_cycle/2)+1, sniffer_cycle/2, 656, 65535);
        }
        
    }
    if(tick >= (period-sniffer_cycle))
    {
        if(sniffer_direction == 1)
        {
            if(final == 0)
            {
                //LightConfig.lightingOn = currentCfg.lightingOn = 0;
                currentCfg.lightingOn = 0;
                ctx.tick  = ctx.period; 
            }
            else if(final <= currentCfg.lightnessLevel )
            {
                //LightConfig.lightingOn = 1;
                ctx.tick  = ctx.period;
                //LightConfig.lightnessLevel = ctx.final;
            }
        }
        else
        {
            if((final == 0) && (currentCfg.lightnessLevel < 657))
            {
                //LightConfig.lightingOn = currentCfg.lightingOn = 0;
                currentCfg.lightingOn = 0;
                ctx.tick  = ctx.period; 
            }
            else if(final >= currentCfg.lightnessLevel )
            {
                //LightConfig.lightingOn = 1;
                ctx.tick  = ctx.period;
                //LightConfig.lightnessLevel = ctx.final;
            }
        }
        LOG_DEBUG("lightnessLevel :%d\n",currentCfg.lightnessLevel);
        if(ctx.tick  == ctx.period)
        {
            llogsaveflag = 1;
            StoreConfig();
            memset(&ctx, 0, sizeof(ctx));
        }
    }
    LightUpdate();
    if(period / sniffer_cycle > 0x7EFF)
    {
        ctx.tick = ctx.tick % sniffer_cycle + sniffer_cycle;
    }
    return 0;
}

void LightSniffer(uint16_t cycle, uint16_t times, uint8_t direction,uint8_t final, uint8_t issaved )
{
    if(wiced_is_timer_in_use(&morsecodeTimer)){
        wiced_stop_timer(&morsecodeTimer);
    }
    morsecodedisplay = WICED_FALSE;

    TurnOnOffDelay.remaintime = 0;
    TurnOnOffDelay.flag = TURNONOFFDELAYSTOP;
    wiced_stop_timer(&delay_count_Timer_500ms);

    sniffer_cycle = cycle/LIGHT_TIMER_UINT;
    //sniffer_times = times;
    if(sniffer_cycle < 2)
    {
        sniffer_cycle = 2;
    }
    ctx.PrePowerTick = 0xFFFF;

    if (times)
    {
        if (wiced_is_timer_in_use(&transitionTimer))
        {
            wiced_stop_timer(&transitionTimer);
            //wiced_deinit_timer(&transitionTimer);
        }
        sniffer_direction = direction;
        currentCfg.lightnessLevel = LightConfig.lightnessLevel;
        ctx.anim = light_sniffer;
        ctx.tick = 1;
        ctx.period = sniffer_cycle * (times+1);
        ctx.initiate = LightConfig.lightingOn;
        ctx.final = percentage_to_uint16(final);
        // LightConfig.lightingOn = 1;
        // if(ctx.final)
        // {
        //     LightConfig.lightnessLevel = ctx.final;
        // }
        // else
        // {
        //     LightConfig.lightingOn = 0;
        // }
        LOG_VERBOSE("TransitionTimer Init \n");

        //wiced_init_timer(&transitionTimer, lightModelCb, 0, WICED_MILLI_SECONDS_PERIODIC_TIMER);
        wiced_start_timer(&transitionTimer, LIGHT_TIMER_CB_LENGTH);
    }
    else  //呼吸时循环次数不能为0，为0直接将灯亮度置为之前保存的亮度后返回
    {
        memset(&ctx, 0, sizeof(ctx));
        currentCfg = LightConfig;
        
        LOG_VERBOSE("LightSniffer Deinit \n");
        LightUpdate();
        return;
    }
    if(issaved)
    {
        if((ctx.final == 0) || (ctx.final < 657)){
            LightConfig.lightingOn = 0;
        }else{
            LightConfig.lightingOn = 1;
            LightConfig.lightnessLevel = ctx.final;
        }
    }
}


int32_t light_flashandsniffer(int32_t tick, int32_t period, int32_t initiate, int32_t final)
{
    final = tick % flashandsniffer_allcycle;
    if(final  < flashandsniffer_flashtimes*flashandsniffer_flashcycle)
    {
        if (final % flashandsniffer_flashcycle <  flashandsniffer_flash_fristtime)
        {
            if(final % flashandsniffer_flashcycle == 1)
            {
                LOG_DEBUG("light flash 1!!!\n");
            }
            if(flashandsniffer_flashtfrist == 1)
            {
                currentCfg.lightingOn = 1;
            }
            else
            {
                currentCfg.lightingOn = 0;
            }
        }
        else if(final % flashandsniffer_flashcycle == flashandsniffer_flash_fristtime)
        {
            LOG_DEBUG("light flash 2!!!\n");
            currentCfg.lightingOn = !currentCfg.lightingOn;
        }
    }
    else
    {
        final = final - flashandsniffer_flashtimes*flashandsniffer_flashcycle;
        currentCfg.lightingOn = 1;
        if(final % flashandsniffer_sniffercycle < flashandsniffer_sniffercycle/2)
        {
            if(final % flashandsniffer_sniffercycle == 1)
            {
                LOG_DEBUG("light sniffer 1!!!\n");
            }
            if(flashandsniffer_snifferfrist)
            {
                currentCfg.lightnessLevel = liner_transfer(final%(flashandsniffer_sniffercycle/2)+1, flashandsniffer_sniffercycle/2, 65535, 656);
            }
            else
            {
                currentCfg.lightnessLevel = liner_transfer(final%(flashandsniffer_sniffercycle/2)+1, flashandsniffer_sniffercycle/2, 656, 65535);
            }
        }
        else
        {
            if(final % flashandsniffer_sniffercycle == flashandsniffer_sniffercycle/2)
            {
                LOG_DEBUG("light sniffer 2!!!\n");
            }

            if(flashandsniffer_snifferfrist)
            {
                currentCfg.lightnessLevel = liner_transfer(final%(flashandsniffer_sniffercycle/2)+1, flashandsniffer_sniffercycle/2, 656, 65535);
            }
            else
            {
                currentCfg.lightnessLevel = liner_transfer(final%(flashandsniffer_sniffercycle/2)+1, flashandsniffer_sniffercycle/2, 65535, 656);
            }
        }
    }
    if(tick == period)
    {
        currentCfg.lightnessLevel = LightConfig.lightnessLevel;
    }
    LightUpdate();
    if(period / flashandsniffer_allcycle > 0x7EFF)
    {
        ctx.tick = ctx.tick % flashandsniffer_allcycle+1;
    }
}

//void LightFlashAndSniffer(uint16_t flashcycle, uint16_t times1, uint16_t sniffercycle,uint16_t times2, uint16_t times)
void LightFlashAndSniffer(Light_FlashAndSniffer_t setPara)
{
    if(wiced_is_timer_in_use(&morsecodeTimer)){
        wiced_stop_timer(&morsecodeTimer);
    }
    morsecodedisplay = WICED_FALSE;
    TurnOnOffDelay.remaintime = 0;
    TurnOnOffDelay.flag = TURNONOFFDELAYSTOP;
    wiced_stop_timer(&delay_count_Timer_500ms);
    
    flashandsniffer_flashtimes = setPara.flash_times;
    flashandsniffer_flashtfrist = setPara.flash_first;
    flashandsniffer_flash_fristtime = setPara.flash_fristtime/LIGHT_TIMER_UINT;
    flashandsniffer_sniffertimes = setPara.sniffer_times;
    flashandsniffer_snifferfrist = setPara.sniffer_frist;
    flashandsniffer_flashcycle = setPara.flash_cycle/LIGHT_TIMER_UINT;
    if(flashandsniffer_flashcycle < flashandsniffer_flash_fristtime)
    {
        flashandsniffer_flashcycle = flashandsniffer_flash_fristtime;
    }
    if(flashandsniffer_flashcycle < 2)
    {
        flashandsniffer_flashcycle = 2;
    }
    flashandsniffer_sniffercycle = setPara.sniffercycle/LIGHT_TIMER_UINT;
    if(flashandsniffer_sniffercycle < 2)
    {
        flashandsniffer_sniffercycle = 2;
    }
    flashandsniffer_allcycle = flashandsniffer_flashcycle*setPara.flash_times + flashandsniffer_sniffercycle * setPara.sniffer_times;
    //sniffer_times = times;

    if (wiced_is_timer_in_use(&transitionTimer))
    {
        wiced_stop_timer(&transitionTimer);
        //wiced_deinit_timer(&transitionTimer);
    }
    
    if (setPara.times)
    {
        currentCfg.lightnessLevel = LightConfig.lightnessLevel;
        ctx.anim = light_flashandsniffer;
        ctx.tick = 1;
        ctx.period = flashandsniffer_allcycle * setPara.times;
        ctx.initiate = LightConfig.lightingOn;
        ctx.final = LightConfig.lightingOn;
        
        LOG_VERBOSE("TransitionTimer Init \n");
        //wiced_init_timer(&transitionTimer, lightModelCb, 0, WICED_MILLI_SECONDS_PERIODIC_TIMER);
        wiced_start_timer(&transitionTimer, LIGHT_TIMER_CB_LENGTH);
    }
    else
    {
        memset(&ctx, 0, sizeof(ctx));
        currentCfg = LightConfig;
        
        LOG_VERBOSE("LightFlashAndSniffer Deinit \n");
        LightUpdate();
    }
}






#if (LIGHT == 1) || (LIGHT == 2)

void LightSetOnPoweron(uint8_t lighton)
{
#include "raw_flash.h"

    // extern wiced_platform_gpio_config_t platform_gpio[];
    // extern size_t gpio_count;

    // extern wiced_platform_gpio_config_t platform_led[];
    // extern size_t led_count;


    // uint8_t temp[6];

    // wiced_hal_gpio_select_function(WICED_P26, WICED_GPIO);
    // wiced_hal_gpio_select_function(WICED_P27, WICED_GPIO);

    // for (uint8_t i = 0; i < led_count; i++)
    // {
    //     wiced_hal_gpio_configure_pin(*platform_led[i].gpio, platform_led[i].config, platform_led[i].default_state);
    // }
    // if(0 == flash_app_read_mem(FLASH_ADDR_CONFIG,(uint8_t *)(&pre_load_cfg), sizeof(LightConfig_def)))
    if (sizeof(LightConfig) != flashReadBlockSave(FLASH_ADDR_CONFIG, (uint8_t *)(&LightConfig), sizeof(LightConfig)))
    {
        const LightConfig_def DefaultLightnessConfig = DEFAULT_LIGHTNESS_CONFIG;
        LightConfig = DefaultLightnessConfig;
        // LightConfig.lightnessLevel = 65534;
        // LightConfig.powerOnMode = CONFIG_POWERON_ON;
        // LightConfig.powerOnMode = CONFIG_POWERON_NORMOL;
        // LightConfig.lightingOn = 1;
        lighton = 1;  //读取配置异常，强制把点点亮
    }
    else
    {
        if(LightConfig.lightnessLevel < 656)
            LightConfig.lightnessLevel = 656;
    }
    
    if(LightConfig.fwupgrade == 0 )
    {
        LOG_DEBUG("powerOnMode %d.......\n",LightConfig.powerOnMode);
        if(LightConfig.powerOnMode == CONFIG_POWERON_NORMOL){
            LightConfig.lightingOn = 1;
        }else if(LightConfig.powerOnMode == CONFIG_POWERON_RESETSWITCH){
            LightConfig.lightingOn = 0;
        }
    }
    
    if(lighton)
    {
        LightConfig.lightingOn = 1;
    }
    
    LOG_DEBUG("lightingOn %d.......\n",LightConfig.lightingOn);
    //pre_load_cfg.lightnessLevel = 65535;
    led_controller_initial();
    //    wiced_hal_gpio_configure_pin(WICED_P27, GPIO_OUTPUT_ENABLE, GPIO_PIN_OUTPUT_LOW);
    led_controller_status_update(LightConfig.lightingOn, LightConfig.lightnessLevel, LightConfig.lightnessCTL);

    uint8_t counter = 255;

    flashReadBlockSave(FLASH_ADDR_CNT, &counter, sizeof(counter));
    if(counter)
        counter--;
    flashWriteBlockSave(FLASH_ADDR_CNT, &counter, sizeof(counter));
    LOG_DEBUG("power on end.......\n");
    // if(pre_load_cfg.fwupgrade == 0 )
    // {
    //     //if(pre_load_cfg.powerOnMode != CONFIG_POWERON_HOLD)
    //     {
    //         pre_load_cfg.lightingOn = 1;
    //     }
    // }
    
    // //pre_load_cfg.lightnessLevel = 65535;
    // led_controller_initial();
    // //    wiced_hal_gpio_configure_pin(WICED_P27, GPIO_OUTPUT_ENABLE, GPIO_PIN_OUTPUT_LOW);
    // led_controller_status_update(pre_load_cfg.lightingOn, pre_load_cfg.lightnessLevel, pre_load_cfg.lightnessCTL);
    
}
#endif

void PrintfAutoBrightness(void)
{
#if LOGLEVEL >= LOGLEVEL_VERBOSE
    uint16_t min;
    LOG_VERBOSE("AutoBrightnessnum %d \n",AutoBrightnessSet.Item.AutoBrightnessNum);
    for(uint16_t i=0; i<AutoBrightnessSet.Item.AutoBrightnessNum; i++)
    {
        min = AutoBrightnessSet.Item.AutoBrightnessSet[i][0];
        min = min*10;
        LOG_VERBOSE("AutoBrightness %d  min %d  brightness %d \n", i, min, AutoBrightnessSet.Item.AutoBrightnessSet[i][1]);
    }
#endif
}
