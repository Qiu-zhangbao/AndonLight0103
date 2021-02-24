//******************************************************************************
//*
//* 文 件 名 : tooling.c
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
#include "stdint.h"
#include "light_model.h"
#include "adv_pack.h"
#include "AES.h"
#include "wiced_hal_nvram.h"

#include "log.h"
#include "tooling.h"
#include "storage.h"
#include "wiced_memory.h"

///*****************************************************************************
///*                         宏定义区
///*****************************************************************************

#define DEFAULT_BURNIN_TIME                     40     //单位min, 默认的老化时间

#define TOOLINGTIMECYCLE                        1      //单位s
#define TOOLINGIN_STEP1_TIMEOUT                 10/TOOLINGTIMECYCLE  
#define TOOLING_BURNIN_STEP2_TIMEOUT            60/TOOLINGTIMECYCLE 
#define TOOLING_BURNIN_STEP3_TIMEOUT            2400/TOOLINGTIMECYCLE 

//#define TOOLINGIN_POWER_TIMEOUT                 15/TOOLINGTIMECYCLE

#define TOOL_MAX_RSSICNT                        15
#define TOOL_MIN_RSSI                           -70

///*****************************************************************************
///*                         Strcut结构变量定义区
///*****************************************************************************

///*****************************************************************************
///*                         函数声明区
///*****************************************************************************
extern void appSetAdvReEnable(void);
extern void appSetAdvDisable(void);
void tooling_task(void);
///*****************************************************************************
///*                         常量定义区
///*****************************************************************************
const uint8_t productCode[] = PRODUCT_CODE;

///*****************************************************************************
///*                         外部全局变量定义区
///*****************************************************************************

///*****************************************************************************
///*                         文件内部全局变量定义区
///*****************************************************************************
static tooling_burnin_t tooling_burnin_data={
    .item = {
        .burningFlag      = TOOLING_BURNIN_IDLE,
        .burninMin        = 0,
        .burninMax        = DEFAULT_BURNIN_TIME
    }
};

wiced_timer_t toolingtimer;
uint16_t      tooling_cnt;
uint16_t      toolrssi[TOOL_MAX_RSSICNT] ={0};
uint16_t      toolrssicnt = 0;
uint16_t      toolafterflag = 0;
int16_t       toolrssiset = -80;
uint16_t      toolpowerflag = 0;
uint16_t      toolupgradeflag = 0;


Light_FlashAndSniffer_t tool_lightshowpara;                      //

///*****************************************************************************
///*                         函数实现区
///*****************************************************************************    
void ToolingTimerCb(uint32_t params)
{
#include "llog.h"

    tooling_task();
    if(tooling_cnt > 0)
        tooling_cnt --;
    if(LightConfig.Lightontime > 1700)
    {
        LightConfig.Lightontime = 0;
        LightConfig_def LightConfig1 = DEFAULT_LIGHTNESS_CONFIG;
        LightConfig = LightConfig1;
        llogsaveflag = 1;
        StoreConfig();
    }
}

//*****************************************************************************
// 函数名称: ToolDIDWriteVerify
// 函数描述: 校验DID是否写入
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
wiced_bool_t ToolDIDWriteVerify(void)
{
    uint32_t *data;
    uint16_t data_len;
    wiced_bool_t result = WICED_TRUE;

    data = (uint32_t*)wiced_bt_get_buffer(FLASH_XXTEA_SN_LEN);
    memset(data,0,FLASH_XXTEA_SN_LEN);
    wiced_hal_wdog_restart();
    data_len = storage_read_sn((uint8_t *)data);
    if(0 != memcmp((uint8_t *)data,productCode,strlen(productCode)))
    {
        result = WICED_FALSE;
    }
    while((data_len > 3) && (result == WICED_TRUE))
    {
        if((*data == 0x00000000) || (*data == 0xFFFFFFFF))
        {
            result = WICED_FALSE;
            break;
        }
        data_len -= 4;
    }
    wiced_bt_free_buffer(data);
    return result;
    
}
//*****************************************************************************
// 函数名称: tooling_task
// 函数描述: 工装处理
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
void tooling_task(void)
{
    //已经完成了老化测试
    if(TOOLING_BURNIN_DONE == tooling_burnin_data.item.burningFlag)
    {
        return;
    }

    switch(tooling_burnin_data.item.burningFlag)
    {
        case TOOLING_BURNIN_IDLE:
        {
            return;
        }
        case TOOLING_BURNIN_STEP1:
        {
            if(0 == tooling_cnt)
            {
                //0.25s/0.25闪烁
                LOG_VERBOSE("tooling burin step1 err!!!\n");
                tooling_burnin_data.item.burningFlag = TOOLING_BURNIN_STEP1_ERR;
                // tool_lightshowpara.flash_first = 1;
                // tool_lightshowpara.flash_fristtime = 25;
                // tool_lightshowpara.flash_cycle = 50;
                // tool_lightshowpara.flash_times = 2;
                // tool_lightshowpara.sniffer_frist = 0;
                // tool_lightshowpara.sniffercycle = 300;
                // tool_lightshowpara.sniffer_times = 1;
                // tool_lightshowpara.times = 0x7FFF;    
                // LightFlashAndSniffer(tool_lightshowpara);
                LightFlash(50,0x7FFF,100,0,0);
            }
            return;
        }
        case TOOLING_BURNIN_STEP1_ERR:
        {
            return;
        }
        case TOOLING_BURNIN_STEP2:
        {
            if(0 == tooling_cnt)
            {
                //set max lightness
                LOG_VERBOSE("tooling burin step3 start\n");
                LightConfig_def LightConfig1 = DEFAULT_LIGHTNESS_CONFIG;
                LightConfig = LightConfig1;
                //停止闪烁
                LightFlash(0,0,0,0,0);
                tooling_burnin_data.item.burningFlag = TOOLING_BURNIN_STEP3;
                if(tooling_burnin_data.item.burninMin < tooling_burnin_data.item.burninMax)
                {
                    //tooling_cnt = (tooling_burnin_data.item.burninMax - tooling_burnin_data.item.burninMin)*60/TOOLINGTIMECYCLE;
                    tooling_cnt = tooling_burnin_data.item.burninMax*60/TOOLINGTIMECYCLE;
                }
            }
            return;
        }
        case TOOLING_BURNIN_STEP3:
        {
            // uint16_t len_res;
            // wiced_result_t p_result;

            // if(0 == tooling_cnt)
            // {
            //     tooling_burnin_data.item.burningFlag = TOOLING_BURNIN_DONE;
            //     len_res = wiced_hal_write_nvram(NVRAM_TOOLING_POSITION, sizeof(tooling_burnin_t),
            //                                         tooling_burnin_data.burnin_array, &p_result);
            //     if((len_res != sizeof(tooling_burnin_t)) || (p_result != WICED_SUCCESS))  
            //     {
            //         //0.25s/0.25s亮灭1次--1.5s/1.5s呼吸2次 一直循环下去
            //         LOG_VERBOSE("tooling burin step3 write nvram err!!!\n");
            //         LightFlashAndSniffer(50,1,300,2,0x7FFF);
            //     } 
            //     else
            //     {
            //         //set min lightness
            //         LightConfig_def LightConfig1 = {
            //             .lightingOn = 1,               
            //             .lightnessLevel = 656,       
            //             .lightnessCTL = 4500,          
            //             .Lightontime  = 0              
            //         };
            //         LOG_VERBOSE("tooling burin step3 done\n");
            //         LightConfig = LightConfig1;
            //         LightFlash(0,0);
            //     }                           
            //     wiced_deinit_timer(&toolingtimer);
            // }
            // else if(tooling_cnt%(60/TOOLINGTIMECYCLE) == 1)
            // {
            //     tooling_burnin_data.item.burninMin++;
            //     len_res = wiced_hal_write_nvram(NVRAM_TOOLING_POSITION, sizeof(tooling_burnin_t),
            //                                         tooling_burnin_data.burnin_array, &p_result);
            //     if((len_res != sizeof(tooling_burnin_t)) || (p_result != WICED_SUCCESS))  
            //     {
            //         //TODO 此处是否有写FLASH异常的提示？？
            //     }                 
            // }
            return;
        }
    }
}

//*****************************************************************************
// 函数名称: tooling_burninTest 
// 函数描述: 工装识别及接收信号强度判断
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
uint8_t tooling_burninTest(wiced_bt_ble_scan_results_t *p_scan_result)
{
    static wiced_bt_device_address_t tool_mac={0};
    int16_t rssiNum = 0;
    uint16_t i,j;
    
    rssiNum = p_scan_result->rssi;
    //接收到信号强度大于-80dbm，记录工装MAC，后续仅接收此工装消息，以有效判别接收的信号强度
    if(0 == memcmp("\x00\x00\x00\x00\x00",tool_mac,sizeof(wiced_bt_device_address_t)))
    {
        //if(rssiNum > -95) 
        //仅有当设备的信号强度大于检测强度时，锁定工装的MAC
        if(rssiNum > toolrssiset)
        {
            #if LOGLEVEL >= LOGLEVEL_VERBOSE
            WICED_BT_TRACE("%s %d tooling mac: %B\n", __func__,__LINE__, p_scan_result->remote_bd_addr);
            #endif
            memcpy(tool_mac, p_scan_result->remote_bd_addr,sizeof(wiced_bt_device_address_t));
        }
        else
        {
            return 0;
        }
    }
    //非工装的MAC，不做处理
    if(0 != memcmp(tool_mac,p_scan_result->remote_bd_addr,sizeof(wiced_bt_device_address_t)))
    {
        #if LOGLEVEL >= LOGLEVEL_VERBOSE
        WICED_BT_TRACE("tool mac:%B  received mac :%B\n",tool_mac,p_scan_result->remote_bd_addr);
        #endif
        return 0;
    }
    toolrssi[toolrssicnt] = rssiNum;
    
    //正向查找
    for(i=0; i<toolrssicnt; i++)
    {
        if(rssiNum < toolrssi[i]) 
        {
            break;
        }
    }
    //反向查找
    for(j=toolrssicnt; j>i; j--)
    {
        toolrssi[j] = toolrssi[j-1];
    }
    toolrssi[i] = rssiNum;

    toolrssicnt++;
    if(toolrssicnt > (TOOL_MAX_RSSICNT-1))
    {
        //分别去除最大和最小的3个值
        for(i=3; i<(TOOL_MAX_RSSICNT-3); i++) 
        {
            rssiNum+=toolrssi[i];
        }
        rssiNum = rssiNum/(TOOL_MAX_RSSICNT-6);
        LOG_VERBOSE("tooling rssi: %d\n",rssiNum);
        if(rssiNum >= toolrssiset)  
        {
            return 0xFF;
        }
        else
        {
            memset(tool_mac, 0, sizeof(wiced_bt_device_address_t));
        }
        toolrssicnt = 0;
    }
    return 0;
}

//*****************************************************************************
// 函数名称: tooling_init
// 函数描述: 工装处理初始化
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
void tooling_init(void)
{
    wiced_result_t ret;
    
    
    toolafterflag = 0;
    //此处从NVRAM中读取老化完成状态  TODO 分配工装标识的NVRAM ID
    //if(sizeof(tooling_burnin_t) != wiced_hal_read_nvram(NVRAM_TOOLING_POSITION, sizeof(tooling_burnin_t), tooling_burnin_data.burnin_array, &ret))
    {
        tooling_burnin_data.item.burningFlag = TOOLING_BURNIN_IDLE;
        tooling_burnin_data.item.burninMin   = 0;
        tooling_burnin_data.item.burninMax   = DEFAULT_BURNIN_TIME;
        return;
    }
    // toolafterflag = 0;
    // //正在老化过程中，继续老化过程
    // if(tooling_burnin_data.item.burningFlag == TOOLING_BURNIN_STEP3)
    // {
    //     tooling_burnin_data.item.burningFlag = TOOLING_BURNIN_STEP2;
    //     if(wiced_is_timer_in_use(&toolingtimer))
    //     {
    //         wiced_deinit_timer(&toolingtimer);
    //     }
    //     wiced_init_timer(&toolingtimer,ToolingTimerCb,0,WICED_SECONDS_PERIODIC_TIMER);
    //     wiced_start_timer(&toolingtimer,1);
    //     //设置闪烁时间
    //     tooling_cnt = 5/TOOLINGTIMECYCLE;
    //     LOG_VERBOSE("tooling burin step3 continue： runtime = %d min\n", tooling_burnin_data.item.burninMin);
    //     //set flash 0.5s/0.5s
    //     LightFlash(100,1000);
    // }
    // else if(tooling_burnin_data.item.burningFlag != TOOLING_BURNIN_DONE)
    // {
    //     tooling_burnin_data.item.burningFlag  = TOOLING_BURNIN_IDLE;
    //     tooling_burnin_data.item.burninMin   = 0;
    //     tooling_burnin_data.item.burninMax   = DEFAULT_BURNIN_TIME;
    // }
}



//*****************************************************************************
// 函数名称: tooling_handle
// 函数描述: 接收工装数据的处理
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
void tooling_handle(wiced_bt_ble_scan_results_t *p_scan_result,uint16_t cmd, uint8_t *data, uint8_t len)
{
    extern wiced_bool_t clear_flash_for_reset(wiced_bt_mesh_core_config_t *p_config_data,wiced_bt_core_nvram_access_t nvram_access_callback);
    extern uint32_t mesh_nvram_access(wiced_bool_t write, int inx, uint8_t* node_info, uint16_t len, wiced_result_t *p_result);
    
    if(cmd != ADV_CMD_TOOLING)  //非工装指令，直接返回
        return;

    //启动老化测试 不再处理工装指令
    if(tooling_burnin_data.item.burningFlag > TOOLING_BURNIN_STEP1_ERR){ 
        return; 
    }
    switch(data[0])
    {
        case ADV_TOOLING_BURNIN:
            switch(tooling_burnin_data.item.burningFlag)
            {
                case TOOLING_BURNIN_IDLE:
                    if((toolafterflag != 0) || (0 != toolpowerflag))
                    {
                        return;
                    }
                    LightConfig_def LightConfig1 = DEFAULT_LIGHTNESS_CONFIG;
                    LightConfig = LightConfig1;
                    LightConfig.Lightontime = 0;
                    LightConfig.fristpair   = CONFIG_UNPAIR;
					// StoreConfig();
                    ResetToolConfig();
                    clear_flash_for_reset(&mesh_config,mesh_nvram_access);
                    //停止闪烁
                    LightFlash(0,0,100,100,0);
                    if(wiced_is_timer_in_use(&toolingtimer))
                    {
                        wiced_deinit_timer(&toolingtimer);
                    }
                    LOG_VERBOSE("tooling burn_in start\n");
                    if(0 == memcmp((uint8_t *)(data+2),productCode+3,sizeof(productCode)-1-3))
                    {
                        toolrssiset = (int8_t)data[1];
                        tooling_burnin_data.item.burningFlag = TOOLING_BURNIN_STEP1;
                        wiced_init_timer(&toolingtimer,ToolingTimerCb,0,WICED_SECONDS_PERIODIC_TIMER);
                        wiced_start_timer(&toolingtimer,TOOLINGTIMECYCLE);
                        tooling_burninTest(p_scan_result);
                        tooling_cnt = TOOLINGIN_STEP1_TIMEOUT;
                    }
                    // else
                    // {
                    //     LOG_VERBOSE("productCode err!!!!\n");
                    //     LightFlash(120,0x7FFF,30,30,0);  //检测code码异常 一直不停的闪烁下去
                    //     tooling_burnin_data.item.burningFlag = TOOLING_BURNIN_STEP1_ERR;
                    // }
                break;
                case TOOLING_BURNIN_STEP1:
                    LOG_VERBOSE("22222......!!!!\n");
                    if(0xFF == tooling_burninTest(p_scan_result))
                    {
                        //set flash 1s/1s
                        LOG_VERBOSE("tooling burin step1 done\n");
                        // if((data[2] > 0) && (data[2] < 145))
                        // {
                        //     uint16_t toolburnintime;
                        //     toolburnintime = data[2];
                        //     toolburnintime = toolburnintime*10;
                        //     tooling_burnin_data.item.burninMax = toolburnintime;
                        // }
                        if(WICED_FALSE == ToolDIDWriteVerify())  //DID写入校验失败
                        {
                            LOG_VERBOSE("ToolDIDWriteVerify err!!!\n");
                            LightFlash(0,0,30,30,0);
                            tooling_burnin_data.item.burningFlag = TOOLING_BURNIN_STEP1_ERR;
                        }
                        else
                        {
                            tooling_cnt = TOOLING_BURNIN_STEP2_TIMEOUT;
                            //清零运行时间
                            LightConfig.Lightontime = 0;
                            StoreConfig();
                            //3s亮--1.5s/1.5s呼吸一次 维持1min
                            //LightFlashAndSniffer(300,1,300,1,10);
                            tool_lightshowpara.flash_first = 0;
                            tool_lightshowpara.flash_fristtime = 300;
                            tool_lightshowpara.flash_cycle = 300;
                            tool_lightshowpara.flash_times = 1;
                            tool_lightshowpara.sniffer_frist = 0;
                            tool_lightshowpara.sniffercycle = 300;
                            tool_lightshowpara.sniffer_times = 1;
                            tool_lightshowpara.times = 10;    
                            LightFlashAndSniffer(tool_lightshowpara);
                            tooling_burnin_data.item.burningFlag = TOOLING_BURNIN_STEP2;
                        }
                    }
                break;
            }
        break;
        case ADV_TOOLING_AFTERBURN:
            switch(toolafterflag)
            {
                case 0:
                    if((tooling_burnin_data.item.burningFlag != TOOLING_BURNIN_IDLE) || (0 !=toolpowerflag))
                    {
                        return;
                    }
                    LOG_VERBOSE("tool burn_after start\n");
                    tooling_cnt = TOOLINGIN_STEP1_TIMEOUT;
                    if(wiced_is_timer_in_use(&toolingtimer))
                    {
                        wiced_deinit_timer(&toolingtimer);
                    }
                    toolrssiset = (int8_t)data[1];
                    wiced_init_timer(&toolingtimer,ToolingTimerCb,0,WICED_SECONDS_PERIODIC_TIMER);
                    wiced_start_timer(&toolingtimer,TOOLINGTIMECYCLE);
                    tooling_burninTest(p_scan_result);
                    toolafterflag = 1;
                break;
                case 1:
                    if(0xFF == tooling_burninTest(p_scan_result))
                    {
                        //set sniffer 1s/1s
                        LOG_VERBOSE("tool burn_after done\n");
                        tooling_cnt = 0;
                        toolafterflag = 2;
                        LightSniffer(300,0x7FFF,0,90,0);
                    }
                    else if(0 == tooling_cnt)
                    {
                        //异常提示 快速亮灭0.25/0.25 2次 呼吸1.5s/1.5s 一次 一直循环
                        LightFlash(50,0x7FFF,100,0,0);
                        toolafterflag = 2;
                        LOG_VERBOSE("tool burn_after rssi pool\n");
                    }
                    
                    break;
                case 2:
                    break;
            }
        break;
        case ADV_TOOLING_POWER_TEST:
            switch(toolpowerflag)
            {
                case 0:
                    if((tooling_burnin_data.item.burningFlag != TOOLING_BURNIN_IDLE) || (0 != toolafterflag))
                    {
                        return;
                    }
                    tooling_cnt = TOOLINGIN_STEP1_TIMEOUT;
                    if(wiced_is_timer_in_use(&toolingtimer))
                    {
                        wiced_deinit_timer(&toolingtimer);
                    }
                    LOG_VERBOSE("tool power test start\n");
                    toolrssiset = (int8_t)data[1];
                    wiced_init_timer(&toolingtimer,ToolingTimerCb,0,WICED_SECONDS_PERIODIC_TIMER);
                    wiced_start_timer(&toolingtimer,TOOLINGTIMECYCLE);
                    tooling_burninTest(p_scan_result);
                    toolpowerflag = 1;
                break;
                case 1:
                    if(0xFF == tooling_burninTest(p_scan_result))
                    {
                        //set sniffer 1s/1s
                         LightConfig_def LightConfig1 = {
                        .lightingOn = 1,               
                        .lightnessLevel = 65535,       
                        .lightnessCTL = 4500,          
                        .Lightontime  = 0              
                        };
                        LOG_VERBOSE("tooling burin step3 done\n");
                        LightConfig = LightConfig1;
                        LightFlash(0,0,0,0,0);
                        tooling_cnt = 2/TOOLINGTIMECYCLE;
                        LOG_VERBOSE("tool power test rssi ok\n");
                        toolpowerflag = 2;
               
                    }
                    else if(0 == tooling_cnt)
                    {
                        //异常提示 快速亮灭0.25/0.25 2次 呼吸1.5s/1.5s 一次 一直循环
                        LightFlash(50,0x7FFF,100,0,0);
                        LOG_VERBOSE("tool power test rssi pool\n");
                        toolpowerflag = 4;
                    }
                break;
                case 2:
                    if(0 == tooling_cnt)
                    {
                        LightConfig.lightingOn = 0;
                        LightConfig.Lightontime = 0;
                        tooling_cnt = 3/TOOLINGTIMECYCLE;
                        LightFlash(0,0,0,0,0);
                        LOG_VERBOSE("tool power test lowest power\n");
                        toolpowerflag = 3;
                    }
                    break;
                case 3:
                    if(0 == tooling_cnt)
                    {
                        LightConfig.lightingOn = 1;
                        LightSniffer(200,0x7FFF,0,90,0);
                        LOG_VERBOSE("tool power test sniffer\n");
                        toolpowerflag = 4;
                    }
                break;
                case 4:
                break;
            }
        break;
        case ADV_TOOLING_UPGRADE_VER:  //工厂升级时的版本信息
            //不是本型号机型适配的工装
            if(0 != memcmp((uint8_t *)(data+3),productCode+3,sizeof(productCode)-1-3)){
                return;
            }

            uint16_t toolfwver;

            toolfwver = data[1];
            toolfwver = toolfwver*256 + data[2];
            if(toolfwver == MESH_FWID){  //版本相同，禁止ADV
                // if(LightConfig.lightingOn){
                //     LightModelTurnOff(0,0,0);
                // }
                if(toolupgradeflag == 0){  //版本相同时,以2s为周期呼吸，亮度循环0--50%
                    LightSniffer(200,0x7FFF,0,50,0);
                    toolupgradeflag = 1;
                }
                appSetAdvDisable();
            }
            else{   //版本不同, 使能ADV广播, 同时亮度100%
                if(LightConfig.lightingOn == 0){ 
                    LightConfig.lightnessLevel = 65535;
                    LightModelTurnOn(0,0,0);
                }
                appSetAdvReEnable();
            }
        break;
    }
}

//*****************************************************************************
// 函数名称: toolSmtCheck
// 函数描述: SMT联通性检测
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
void toolSmtCheck(void)
{
#include "wiced_rtos.h"
#include "wiced_hal_wdog.h"


    //设置除触发按键之外的GPIO为输出
    wiced_hal_gpio_select_function(WICED_P28,WICED_GPIO);
    wiced_hal_gpio_configure_pin(WICED_P28, GPIO_OUTPUT_ENABLE, GPIO_PIN_OUTPUT_HIGH);
    wiced_hal_gpio_select_function(WICED_P29,WICED_GPIO);
    wiced_hal_gpio_configure_pin(WICED_P29, GPIO_OUTPUT_ENABLE, GPIO_PIN_OUTPUT_LOW);
    wiced_hal_gpio_select_function(WICED_P26,WICED_GPIO);
    wiced_hal_gpio_configure_pin(WICED_P26, GPIO_OUTPUT_ENABLE, GPIO_PIN_OUTPUT_HIGH);
    wiced_hal_gpio_select_function(WICED_P27,WICED_GPIO);
    wiced_hal_gpio_configure_pin(WICED_P27, GPIO_OUTPUT_ENABLE, GPIO_PIN_OUTPUT_LOW);
    wiced_hal_gpio_select_function(WICED_P01,WICED_GPIO);
    wiced_hal_gpio_configure_pin(WICED_P01, GPIO_OUTPUT_ENABLE, GPIO_PIN_OUTPUT_LOW);
    wiced_hal_gpio_select_function(WICED_P00,WICED_GPIO);
    wiced_hal_gpio_configure_pin(WICED_P00, GPIO_OUTPUT_ENABLE, GPIO_PIN_OUTPUT_HIGH);
    wiced_hal_gpio_select_function(WICED_P08,WICED_GPIO);
    wiced_hal_gpio_configure_pin(WICED_P08, GPIO_OUTPUT_ENABLE, GPIO_PIN_OUTPUT_LOW);
    wiced_hal_gpio_select_function(WICED_P03,WICED_GPIO);
    wiced_hal_gpio_configure_pin(WICED_P03, GPIO_OUTPUT_ENABLE, GPIO_PIN_OUTPUT_HIGH);
    wiced_hal_gpio_select_function(WICED_P02,WICED_GPIO);
    wiced_hal_gpio_configure_pin(WICED_P02, GPIO_OUTPUT_ENABLE, GPIO_PIN_OUTPUT_LOW);
    
    if(WICED_FALSE == ToolDIDWriteVerify())
    {
        wiced_hal_wdog_disable();
        while(1);
    }
    while(1)
    {
        wiced_hal_wdog_restart();
        wiced_rtos_delay_milliseconds(600, KEEP_THREAD_ACTIVE);
        wiced_hal_gpio_set_pin_output(WICED_P28,GPIO_PIN_OUTPUT_LOW);
        wiced_hal_gpio_set_pin_output(WICED_P29,GPIO_PIN_OUTPUT_HIGH);
        wiced_hal_gpio_set_pin_output(WICED_P26,GPIO_PIN_OUTPUT_LOW);
        wiced_hal_gpio_set_pin_output(WICED_P27,GPIO_PIN_OUTPUT_HIGH);
        wiced_hal_gpio_set_pin_output(WICED_P01,GPIO_PIN_OUTPUT_HIGH);
        wiced_hal_gpio_set_pin_output(WICED_P00,GPIO_PIN_OUTPUT_LOW);
        wiced_hal_gpio_set_pin_output(WICED_P08,GPIO_PIN_OUTPUT_HIGH);
        wiced_hal_gpio_set_pin_output(WICED_P03,GPIO_PIN_OUTPUT_LOW);
        wiced_hal_gpio_set_pin_output(WICED_P02,GPIO_PIN_OUTPUT_HIGH);
        wiced_rtos_delay_milliseconds(600, KEEP_THREAD_ACTIVE);
        wiced_hal_gpio_set_pin_output(WICED_P28,GPIO_PIN_OUTPUT_HIGH);
        wiced_hal_gpio_set_pin_output(WICED_P29,GPIO_PIN_OUTPUT_LOW);
        wiced_hal_gpio_set_pin_output(WICED_P26,GPIO_PIN_OUTPUT_HIGH);
        wiced_hal_gpio_set_pin_output(WICED_P27,GPIO_PIN_OUTPUT_LOW);
        wiced_hal_gpio_set_pin_output(WICED_P01,GPIO_PIN_OUTPUT_LOW);
        wiced_hal_gpio_set_pin_output(WICED_P00,GPIO_PIN_OUTPUT_HIGH);
        wiced_hal_gpio_set_pin_output(WICED_P08,GPIO_PIN_OUTPUT_LOW);
        wiced_hal_gpio_set_pin_output(WICED_P03,GPIO_PIN_OUTPUT_HIGH);
        wiced_hal_gpio_set_pin_output(WICED_P02,GPIO_PIN_OUTPUT_LOW);
    } 

}

