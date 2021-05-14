/**
 * @file app.c Entry of Mesh Lighting Application
 * @author ljy
 * @date 2019-03-01
 */

#include "wiced_bt_ble.h"
#include "wiced_bt_gatt.h"
#include "wiced_bt_mesh_models.h"
#include "wiced_bt_trace.h"
#include "wiced_bt_mesh_app.h"
#include "wiced_bt_mesh_provision.h"

#include "wiced_bt_beacon.h"
#include "wiced_bt_l2c.h"

#include "wiced_hal_cpu_clk.h"

#include "wiced_memory.h"

#include "wiced_rtos.h"


#include "config.h"
#include "storage.h"

#include "pwm_control.h"

#include "log.h"

#include "hci_control_api.h"
#include "vendor.h"

#include "adv_pack.h"
#include "andon_server.h"
#include "WyzeService.h"

#include "light_model.h"

#include "raw_flash.h"
#include "tooling.h"
#include "systimer.h"
#include "wiced_bt_btn_handler.h"
#include "src_provisioner/mesh_application.h"
#include "mylib.h"

#if MESH_START_SYNC != 0
#include "wiced_hidd_lib.h"
#endif


#if (CHECK_POWEROFF_VALUE) && (!CHECK_POWEROFF_INTT)
#include "wiced_hal_adc.h"
wiced_timer_t powerTestTimer;
#define CHANNEL_TO_MEASURE_DC_VOLT        ADC_INPUT_P28
#endif

#define APPRESET_TIMEOUT     3

extern wiced_bt_cfg_settings_t wiced_bt_cfg_settings;

// struct
// {
//     uint16_t lightingOn;
//     uint16_t lightnessLevel;
//     uint16_t lightnessCTL;
// } pre_load_cfg;

extern mesh_app_interrupt_handler_t  mesh_app_interrupt_handler;
extern wiced_platform_button_config_t platform_button[];

// static wiced_timer_t scan_beacon_timer;

extern wiced_bool_t appAndonBleConnectUsed(void);
static void mesh_app_init(wiced_bool_t is_provisioned);

static void mesh_app_attention(uint8_t element_idx, uint8_t time);
static void mesh_vendor_server_send_status(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint16_t data_len);
static void mesh_vendor_server_process_get(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint16_t data_len);
static void mesh_vendor_server_process_data(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint16_t data_len);
wiced_bool_t mesh_vendor_message_handler(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint16_t data_len);

void readTxPowerCallback(void *pdata);

void reset_process(void);

static void pre_initial(void);
static void pre_initial(void)
{
}


extern void (*mesh_reset_timer_cb)(void);
static wiced_timer_t reset_timer;
static uint16_t reset_timer_count = APPRESET_TIMEOUT;
static wiced_bool_t fastadvenable = WICED_TRUE;
static wiced_bool_t appadvenable = WICED_TRUE;
static uint32_t     advstarttime = 0;
static uint8_t      switchflag   = 0;
static  uint8_t     powerOnCnt = 255;

// uint32_t testpowercnt = 0;
// struct{
//     uint32_t powerstata;
//     uint16_t uptimes;
//     uint16_t downtimes;
//     uint32_t changetimes;
// }powerbac = {0};

extern uint8_t mesh_system_id[WICED_BT_MESH_PROPERTY_LEN_DEVICE_SERIAL_NUMBER];
extern uint8_t mesh_fW_ver[WICED_BT_MESH_PROPERTY_LEN_DEVICE_FIRMWARE_REVISION]; 
extern uint8_t mesh_hW_ver[WICED_BT_MESH_PROPERTY_LEN_DEVICE_HARDWARE_REVISION]; 

// static wiced_result_t ret;
// static wiced_timer_t reset_timer;

static void gatt_connection_changed(wiced_bt_gatt_connection_status_t *p_status)
{
    if (p_status->connected)
    {
        // if (wiced_is_timer_in_use(&reset_timer))
        // {
        //     wiced_deinit_timer(&reset_timer);
        // }
        uint8_t i = 0377;
        flashWriteBlockSave(FLASH_ADDR_CNT, &i, sizeof(i));
        mesh_reset_timer_cb = NULL;
    }
}

// wiced_bt_mesh_app_func_table_t wiced_bt_mesh_node_func_table = {
//     mesh_app_init,           // application initialization
//     NULL,                    // Default SDK platform button processing
//     gatt_connection_changed, // GATT connection status
//     NULL,//mesh_app_attention,      // attention processing
//     NULL,                    // notify period set
//     NULL,                    //mesh_app_proc_rx_cmd, // WICED HCI command
//     NULL,                    // LPN sleep
//     NULL     //ResetConfig              // factory reset
// };


void appResetUserSet(void)
{

#include "wiced_hidd_lib.h"
    extern void mesh_app_gatt_is_disconnected(void);
    extern wiced_bool_t clear_flash_for_reset(wiced_bt_mesh_core_config_t *p_config_data,wiced_bt_core_nvram_access_t nvram_access_callback);
    extern void wdog_generate_hw_reset(void);
    extern uint32_t mesh_nvram_access(wiced_bool_t write, int inx, uint8_t* node_info, uint16_t len, wiced_result_t *p_result);

    if(reset_timer_count!=0){
        if(reset_timer_count == 1){
            if(LightConfig.bleonly != CONFIG_MESHONLY){
                andonServerSendResetStatus();
            }
            if(LightConfig.bleonly != CONFIG_BLEONLY){
                vendorSendDevResetStatus();
            }
        }
        reset_timer_count --;
        return;
    }

    mesh_app_gatt_is_disconnected();
    wiced_deinit_timer(&reset_timer);
    //发送设置重置通知
    mesh_reset_timer_cb = NULL;
    LOG_INFO("Factory reset \n");     
    ResetConfig();
    if(WICED_TRUE == clear_flash_for_reset(&mesh_config,mesh_nvram_access)){
        LOG_DEBUG("reset write flash succeed!\n");
    }else{
        LOG_DEBUG("reset write flash fail!\n");
    }
    powerOnCnt = configLIGHTRSTRESTARTCNT;
    flashWriteBlockSave(FLASH_ADDR_CNT, &powerOnCnt, sizeof(powerOnCnt));
    LOG_DEBUG("reset system!!\n");
    wdog_generate_hw_reset();
    while(1); 
}

void appResetUserSetAlert(void)
{
    extern int8_t uint16_to_percentage(uint16_t);

    if(reset_timer_count!=0){
        reset_timer_count --;
        return;
    }
    LOG_DEBUG("Reset User Set Alert!!!\n");
#if (CHECK_POWEROFF_VALUE) && (!CHECK_POWEROFF_INTT)
    wiced_stop_timer(&powerTestTimer);
#endif
    LightFlash(100,3,50,0,0);
    powerOnCnt = configLIGHTRSTCNT;
    flashWriteBlockSave(FLASH_ADDR_CNT, &powerOnCnt, sizeof(powerOnCnt));
    reset_timer_count = 4;
    mesh_reset_timer_cb = appResetUserSet;
    
}

#if (CHECK_POWEROFF_INTT) && (!CHECK_POWEROFF_VALUE)
wiced_timer_t powerCheckIntteruptDisableTimer;
uint16_t powerstata = 0;

extern void mesh_interrupt_handler(void* user_data, uint8_t pin);

//TODO 中断方式未完成相关功能！！！！
void powerCheckIntteruptDisableTimerCb(uint32_t para)
{
    wiced_stop_timer(&powerCheckIntteruptDisableTimer);
    if(wiced_hal_gpio_get_pin_input_status(*platform_button[WICED_PLATFORM_BUTTON_1].gpio != powerstata))
    {
        wiced_platform_register_button_callback(WICED_PLATFORM_BUTTON_1, mesh_interrupt_handler, NULL, GPIO_EN_INT_BOTH_EDGE);
        return;
    }
    if(powerstata == 0)
    {
        // if(0 == wiced_hal_gpio_get_pin_input_status(*platform_button[WICED_PLATFORM_BUTTON_1].gpio))
        {
            LightModelToggleForPowerOff(0x08,0,0);
            StoreConfigImmediately();
            LOG_DEBUG("LightToggle for Power!!!!!\n");
        }
    }
    else
    {
        // if(1 == wiced_hal_gpio_get_pin_input_status(*platform_button[WICED_PLATFORM_BUTTON_1].gpio))
        {
            LightModelToggleForPowerOff(0x08,0,1);
            LOG_DEBUG("LightToggle for Power!!!!!\n");
        }
    }
    wiced_platform_register_button_callback(WICED_PLATFORM_BUTTON_1, mesh_interrupt_handler, NULL, GPIO_EN_INT_BOTH_EDGE);
}

void PowerStataCheckIntterupt(void* user_data, uint8_t pin, uint32_t value)
{
    wiced_platform_register_button_callback(WICED_PLATFORM_BUTTON_1, mesh_interrupt_handler, NULL, GPIO_INTERRUPT_DISABLE);
    if(wiced_is_timer_in_use(&powerCheckIntteruptDisableTimer))
    {
        return;
    }
    if(value != wiced_hal_gpio_get_pin_input_status(*platform_button[WICED_PLATFORM_BUTTON_1].gpio))
    {
        wiced_platform_register_button_callback(WICED_PLATFORM_BUTTON_1, mesh_interrupt_handler, NULL, GPIO_EN_INT_BOTH_EDGE);
        return;
    }
    wiced_start_timer(&powerCheckIntteruptDisableTimer,5);
    if(pin == *platform_button[WICED_PLATFORM_BUTTON_1].gpio)
    {
        powerstata = value;
        // if(value == 0)
        // {
        //     //if(0 == wiced_hal_gpio_get_pin_input_status(pin))
        //     {
        //         LightModelToggleForPowerOff(0x08,0,0);
        //         StoreConfigImmediately();
        //         LOG_DEBUG("LightToggle for Power!!!!!\n");
        //     }
        // }
        // else
        // {
        //     //if(1 == wiced_hal_gpio_get_pin_input_status(pin))
        //     {
        //         LightModelToggleForPowerOff(0x08,0,1);
        //         LOG_DEBUG("LightToggle for Power!!!!!\n");
        //     }
        // }
    }
}
#endif

#if (CHECK_POWEROFF_VALUE) && (!CHECK_POWEROFF_INTT)
void powerTestTimerCb(uint32_t para)
{
    extern void mesh_app_gatt_is_disconnected(void);
    static uint16_t PowerOffCnt = 0;
    static uint16_t PowerDeltaCnt = 0;
    static int16_t lastPowerValue = 0;
    static int16_t PowerValue = 0;
    static uint16_t PowerOffFlag = 0;
    static uint16_t PersonIn = 0;
    static uint16_t PersonOut = 0;
    // static  uint8_t powerOnCnt = 255;
    static uint32_t sec=0;
    
    sec++;

    PowerValue= wiced_hal_adc_read_voltage( CHANNEL_TO_MEASURE_DC_VOLT); 
    // LOG_DEBUG("PowerValue %d!!!!!\n",PowerValue);
    if(lastPowerValue == 0)
        lastPowerValue = PowerValue;
    //if((PowerValue < 2000) || ((lastPowerValue-PowerValue) > 40))
    if( sec>3*3000/40)
    {
        if(wiced_hal_gpio_get_pin_input_status(WICED_P27)){
            PersonIn++;
            if(PersonIn > 2500/40){
                PersonIn = 2500/40;
            }
            //当人体存在2s且灯未点亮时点灯
            if((0 == LightConfig.lightingOn) && (PersonIn==2000/40))
            {
            LightModelTurn(1,0,0);
            }
            if(PersonIn>500/40){
                PersonOut = 0;
            }
        }else{
            PersonOut++;
            if(PersonOut > 15*1000/40){
                PersonOut = 151000/40;
                PersonIn = 0;
            }
            //当人体离开三分钟且灯未关闭时关灯
            if((LightConfig.lightingOn) && (PersonOut == 10*1000/40))
            {
                LightModelTurn(0,0,0);
            }
            if(PersonOut > 5*1000/40){
                PersonIn = 0;
            }
        }
    }

    if(PowerValue < 2000)
    {
        if(PowerValue < 2000)
        {
            PowerOffCnt++;
        }
        PowerValue= wiced_hal_adc_read_voltage( CHANNEL_TO_MEASURE_DC_VOLT); 
        if(PowerValue < 2000)
        {
            PowerOffCnt++;
        }
        // if((lastPowerValue-PowerValue) > 40)
        // {
        //     PowerDeltaCnt++;
        // }
    }
    else 
    {
        //TODO 防错处理
        if(PowerOffFlag == 1)
        {
            // powerbac.powerstata = (powerbac.powerstata<<1)|0x01;
            // powerbac.uptimes ++;
            switchflag = 2;
            //LightModelToggle(0x08,0);
            if(powerOnCnt)
                powerOnCnt--;
            // flash_app_write_mem(FLASH_ADDR_CNT, &powerOnCnt, sizeof(powerOnCnt));
            if(powerOnCnt > configLIGHTRSTCNT)
            {
                if(LightConfig.powerOnMode == CONFIG_POWERON_RESETSWITCH){
                    // LightModelToggleForPowerOff(0x08,0,1);
                    LightModelToggleForPowerOff(0,0,1);
                }else{
                    // LightModelTurn(1,0x08,0);
                    LightModelTurn(1,0,0);
                }
                reset_process();
                StoreConfigDelay(); 
                fastadvenable = WICED_TRUE;
                appadvenable  = WICED_TRUE;
                appAndonBleConnectUsed();
                adv_pair_disable();
                adv_pair_enable();
            }
            else
            {
                if(mesh_reset_timer_cb != appResetUserSet){
                    reset_timer_count = 1;  //掉电时多等一段时间清除复位次数计时
                    mesh_reset_timer_cb = appResetUserSetAlert;
                }
            }
            LOG_DEBUG("LightToggle for Power!!!!!\n");
        }
        PowerOffCnt = 0;
        PowerOffFlag = 0;
        PowerDeltaCnt = 0;
    }
    // if(mylib_abs(lastPowerValue,PowerValue)>200){
    //     powerbac.changetimes++;
    // }
    lastPowerValue = PowerValue;

    if((PowerOffFlag == 0) && ((PowerOffCnt > 1) || (PowerDeltaCnt > 4)))
    {
        PowerOffFlag = 1;
        // powerbac.powerstata = (powerbac.powerstata<<1)&0xFFFFFFFE;
        // powerbac.downtimes ++;
        if(powerOnCnt > configLIGHTRSTCNT)
        {
            if(LightConfig.powerOnMode == CONFIG_POWERON_RESETSWITCH){
                // LightModelToggleForPowerOff(0x08,0,0);
                LightModelToggleForPowerOff(0,0,0);
            }
            reset_process();
            switchflag = 1;
            reset_timer_count = 5*APPRESET_TIMEOUT;  //掉电时多等一段时间清除复位次数计时
            //StoreConfigImmediately();
        }
        LOG_DEBUG("LightToggle for Power!!!!!\n");
    }

    if(PowerOffCnt>50)
    {
        if(mesh_app_gatt_is_connected())
        {
            mesh_app_gatt_is_disconnected();
            LOG_DEBUG("gatt_is_disconnected\n");
        }
        appSetAdvDisable();
        LOG_DEBUG("wiced_bt_stop_advertisements\n");
        PowerOffCnt=0;
    }

    // testpowercnt++;
    // if(testpowercnt == 50){
    //     if(mesh_app_gatt_is_connected()){
    //         wiced_bt_gatt_send_notification(1, HANDLE_ANDON_SERVICE_CHAR_NOTIFY_VAL, sizeof(powerbac), &powerbac);
    //     }
    //     testpowercnt = 0;
    // }

} 
#endif

void appStartDevAdv(void)
{
    // extern LightConfig_def pre_load_cfg;
    // uint8_t devdata[16] = {0,0xFF,0x70,0x08,0x03,0x06};
    // uint16_t node_id = 0;

    // wiced_bt_dev_read_local_addr(devdata+6);
    // // if(mesh_app_node_is_provisioned())
    // if(storageBindkey.bindflag == WICED_TRUE)
    // {
    //     devdata[12] = 0x01;
    // }
    // else
    // {
    //     devdata[12] = 0x00;
    // }
    // devdata[13] = 0;
    // node_id = wiced_bt_mesh_core_get_local_addr();
    // devdata[14] = (uint8_t)(node_id>>8);
    // devdata[15] = (uint8_t)(node_id&0xFF);
    // //memcpy(devdata+4,PRODUCT_CODE,sizeof(PRODUCT_CODE)-1);
    // devdata[0] = 5 + sizeof(wiced_bt_device_address_t) + 1 + 1 + 2;
    // // adv_manuDevAdvStart(devdata,devdata[0]+1);

    uint8_t devdata[18] = {0,0xFF,0x04,0x08,0x03,0x06};

    // wiced_bt_dev_read_local_addr(devdata+4);

    //此处填充的mac需与DID中的mac保持一致
    LOG_DEBUG("mesh_system_id: %s\n",mesh_system_id);
    for(uint8_t i=0;i<12;i+=2)
    {
        devdata[i/2+6] = HexStr2Int8U(mesh_system_id+i+strlen(PRODUCT_CODE)+1);
    }

    // if(mesh_app_node_is_provisioned())
    if(storageBindkey.bindflag == WICED_TRUE){
        devdata[12] = 0x01;
    }else{
        devdata[12] = 0x00;
    }
    devdata[13] = 0x01;
    if(mesh_app_gatt_is_connected()){
        devdata[14] = 0x01;
    }else{
        devdata[14] = 0x00;
    }
    devdata[0] = 14;
    adv_manuDevAdvStart(devdata,devdata[0]+1);
}

wiced_bt_device_address_t remoteaddr;

void appUpdataCommpara(void){
    wiced_bt_l2cap_update_ble_conn_params(remoteaddr, 36, 48, 0, 200);
}

void appBleConnectNotify(wiced_bool_t isconneted,wiced_bt_device_address_t addr)
{
    if(isconneted == WICED_FALSE)
    {
        // appStartDevAdv();
        adv_manuDevAdvStop();
        memset(remoteaddr,0,sizeof(wiced_bt_device_address_t));
        #ifdef DEF_ANDON_GATT
            //断开GATT连接，关闭Andon服务通知
            AndonServiceGattDisConnect();
            WyzeServiceGattDisConnect();
        #endif
    }
    else
    {
        // if(storageBindkey.bindflag == WICED_FALSE){
        //     lightModelTurn(1,0,0);
        // }
        appStartDevAdv();
        memcpy(remoteaddr,addr,sizeof(wiced_bt_device_address_t));
        // adv_manuDevAdvStop();
    }
    
}

static void reset_timer_callback(void)
{
    // wiced_stop_timer(&reset_timer);
    // wiced_hal_write_nvram(WRITE_ADDRESS, sizeof(reset_counter), (uint8_t *)&reset_counter, &ret);
    if(reset_timer_count != 0){
        reset_timer_count--;
    }
    
    //掉电时延迟1s写入上电次数，仅用于自复位开关复位
    if((reset_timer_count == 5*APPRESET_TIMEOUT-1) && (switchflag == 1)){
        switchflag = 0;
        // powerOnCnt = 255;
        // flash_app_read_mem(FLASH_ADDR_CNT, &powerOnCnt, sizeof(powerOnCnt));
        // if(powerOnCnt)
        //     powerOnCnt--;
        flashWriteBlockSave(FLASH_ADDR_CNT, &powerOnCnt, sizeof(powerOnCnt));
    }

    if(reset_timer_count == 0)
    {
        powerOnCnt = 255;
        flashWriteBlockSave(FLASH_ADDR_CNT, &powerOnCnt, sizeof(powerOnCnt));
        LOG_VERBOSE("reset to %d \n", (int)powerOnCnt);
        mesh_reset_timer_cb = NULL;
        wiced_deinit_timer(&reset_timer);
        if(!mesh_app_gatt_is_connected())
        {
            //appStartDevAdv();
            //appAndonBleConnectUsed();
        }
    }
    // wiced_deinit_timer(&reset_timer);
}

void reset_timer_respon(uint32_t para)
{
    if(mesh_reset_timer_cb != NULL){
        (*mesh_reset_timer_cb)();
    }
}

void reset_process(void)
{

    uint8_t temp;
    if(0 == flashReadBlockSave(FLASH_ADDR_CNT, &temp, sizeof(temp)))
    {
        temp = 255;
    }
    LOG_VERBOSE("load %d \n", (int)temp);
    //adv_pair_disable();
#if (LIGHT != 1) && (LIGHT != 2)
    temp--;
    flashWriteBlockSave(FLASH_ADDR_CNT, &temp, sizeof(temp));
#endif

#if MESH_START_SYNC == 0
    if(!(temp > configLIGHTRSTCNT))
    {
        extern uint32_t  mesh_nvram_access(wiced_bool_t write, int inx, uint8_t* node_info, uint16_t len, wiced_result_t *p_result);
        
        extern wiced_platform_led_config_t platform_led[];
        uint32_t current = 0;
        uint8_t deviceUUID[16]={0};
        wiced_result_t ret;
        
        LOG_INFO("Factory reset \n");       
        led_controller_status_update(0, 0, 0);
        wiced_rtos_delay_milliseconds(600, KEEP_THREAD_ACTIVE);
        led_controller_status_update(1, LightConfig.lightnessLevel, 0);
        wiced_rtos_delay_milliseconds(600, KEEP_THREAD_ACTIVE);
        led_controller_status_update(0, 0, 0);
        wiced_rtos_delay_milliseconds(600, KEEP_THREAD_ACTIVE);
        led_controller_status_update(1, LightConfig.lightnessLevel, 0);
        wiced_rtos_delay_milliseconds(600, KEEP_THREAD_ACTIVE);
        led_controller_status_update(0, 0, 0);
        wiced_rtos_delay_milliseconds(600, KEEP_THREAD_ACTIVE);
        led_controller_status_update(1, LightConfig.lightnessLevel, 0);
        ResetConfig();
        wiced_hal_vs_nvram_reset();
        temp = 255;
        flashWriteBlockSave(FLASH_ADDR_CNT, &temp, sizeof(temp));
        wiced_rtos_delay_milliseconds(600, KEEP_THREAD_ACTIVE);

        //int8_t i = 2;
        //uint16_t bts = wiced_hal_write_nvram(NVRAM_REMOTEMAC_POSITION, sizeof(i), &i, &ret);
        //wiced_hal_delete_nvram(NVRAM_AUTOBRIGHTNESS_POSITION, &ret);
        //mesh_nvram_access(WICED_TRUE, NVRAM_ID_LOCAL_UUID, deviceUUID, 0, &ret);
        mesh_application_factory_reset();
        
    }
#endif
#if (LIGHT != 1) && (LIGHT != 2)
    temp--;
    flashWriteBlockSave(FLASH_ADDR_CNT, &temp, sizeof(temp));
#endif
    if(wiced_is_timer_in_use(&reset_timer)){
        wiced_stop_timer(&reset_timer);
    }else{
        wiced_init_timer(&reset_timer,reset_timer_respon,0,WICED_MILLI_SECONDS_PERIODIC_TIMER);
    }
    wiced_start_timer(&reset_timer,1000);
    reset_timer_count = APPRESET_TIMEOUT;
    mesh_reset_timer_cb = reset_timer_callback;
}

void readTxPowerCallback(void *pdata)
{
    wiced_bt_tx_power_result_t *power;

    power = pdata;
    LOG_DEBUG("Ble TX Power %d  3.............\n",power->tx_power);
}

void mesh_node_app_init(wiced_bool_t is_provisioned)
{
    extern int  mesh_core_version_info();
    #if (CHECK_POWEROFF_INTT) && (!CHECK_POWEROFF_VALUE)
        mesh_app_interrupt_handler = PowerStataCheckIntterupt;
        wiced_init_timer(&powerCheckIntteruptDisableTimer,powerCheckIntteruptDisableTimerCb,0,WICED_MILLI_SECONDS_PERIODIC_TIMER);
    #endif 
    // #endif
    wiced_hal_gpio_select_function(WICED_P27,WICED_GPIO);
    wiced_hal_gpio_configure_pin(WICED_P27,( GPIO_INPUT_ENABLE | GPIO_PULL_DOWN ),GPIO_PIN_OUTPUT_LOW);
    wiced_update_cpu_clock(WICED_TRUE, WICED_CPU_CLK_96MHZ);
    LOG_VERBOSE("mesh_app_init is_provisioned: %d\n", is_provisioned);
    
    flashReadBlockSave(FLASH_ADDR_CNT, &powerOnCnt, sizeof(powerOnCnt));

    //LOG_INFO("free memory: %d\n", wiced_get_free_memory());
    sysTimerInit();
    
    LOG_INFO("build time: %s %s\n",__DATE__,__TIME__);
    LightModelInitial(1);
    
    // if(is_provisioned)
    // {
    //     LightConfig.powerOnMode = CONFIG_POWERON_HOLD;
    // }
    adv_pack_init();
    // LightFlash(60,3);
    InitVendor();
    tooling_init();
    // LOG_INFO("local name: %s\n",dev_name);
    LOG_INFO("build time: %s %s\n",__DATE__,__TIME__);
    LOG_INFO("%s: Mesh Core version: %s\n", __func__, mesh_core_version_info());
    
    
    LOG_INFO("PID: %04x\n", MESH_CID);
    LOG_INFO("PID: %04x\n", MESH_PID);
    LOG_INFO("VID: %04x\n", MESH_VID);
    LOG_INFO("Fw: %02x.%02x\n", (uint8_t)((MESH_FWID>>8)&0xFF), (uint16_t)(MESH_FWID&0xFF));
    //LOG_INFO("   " VERSION_DESCRIPTION "\n");
    // while(1);
    #if LOGLEVEL >= LOGLEVEL_INFO 
        wiced_bt_device_address_t madd;
        wiced_bt_dev_read_local_addr(madd);
        WICED_BT_TRACE("local mac: %B\n",madd);
        if(is_provisioned)
        {
            extern void mesh_core_node_info(void);
            extern void mesh_core_sublist_data(void);
            mesh_core_node_info();
            mesh_core_sublist_data();
        }
    #endif
    //adv_pack_init();

    // LOG_VERBOSE("flash init\n");
    //flash_app_init();
    // LOG_VERBOSE("flash read\n");

    // flash_app_read_mem(FLASH_ADDR_CONFIG, (uint8_t *)&pre_load_cfg, sizeof(pre_load_cfg));
    // LOG_VERBOSE("LOAD: %d %d\n", pre_load_cfg.lightnessLevel, pre_load_cfg.lightnessCTL);

    int8_t i;
    wiced_result_t ret;
    //uint16_t bts = wiced_hal_read_nvram(0x220, sizeof(i), &i, &ret);
    // if (bts == sizeof(i))
    // {
    //     wiced_bt_mesh_core_adv_tx_power = i;
    // }
    // else
    // {
    //     wiced_bt_mesh_core_adv_tx_power = 2;
    // }

    LOG_DEBUG("TX Power %d\n", wiced_bt_mesh_core_adv_tx_power);
    // wiced_bt_dev_set_adv_tx_power(-10);
    // // LOG_DEBUG("Ble TX Power %d\n", wiced_bt_ble_read_adv_tx_power());
    // wiced_bt_ble_read_adv_tx_power(readTxPowerCallback);
    
    reset_process();

    #if ANDON_TEST
    extern void storage_save_keyandsn(void);  
    //LOG_VERBOSE("Write xxtea sn and key\n"); 
    PrintfXxteakeyandsn();
    storage_save_keyandsn();
    #endif

    {
        uint8_t len=0,temp=0;
        temp = (uint8_t)(((MESH_FWID>>8)&0xF0)>>4);
        if(temp > 9)
        {
            temp = 9;
        }
        mesh_fW_ver[len++] = temp + 0x30;
        mesh_fW_ver[len++] = '.';
        temp = (uint8_t)((MESH_FWID>>8)&0x0f);
        if(temp > 9)
        {
            temp = 9;
        }
        mesh_fW_ver[len++] = temp + 0x30;
        mesh_fW_ver[len++] = '.';
        temp = (uint8_t)(MESH_FWID&0xFF);
        if(temp > 99){
            mesh_fW_ver[len++] = temp/100 + 0x30;
            mesh_fW_ver[len++] = (temp%100)/10 + 0x30;
            mesh_fW_ver[len++] = temp%10 + 0x30;
        }else if(temp > 9){
            mesh_fW_ver[len++] = temp/10 + 0x30;
            mesh_fW_ver[len++] = temp%10 + 0x30;
        }else{
            mesh_fW_ver[len++] = temp + 0x30;
        }


        len = 0;
        temp = (uint8_t)(((MESH_VID>>8)&0xF0)>>4);
        if(temp > 9)
        {
            temp = 9;
        }
        mesh_hW_ver[len++] = temp + 0x30;
        mesh_hW_ver[len++] = '.';
        temp = (uint8_t)((MESH_VID>>8)&0x0f);
        if(temp > 9)
        {
            temp = 9;
        }
        mesh_hW_ver[len++] = temp + 0x30;
        mesh_hW_ver[len++] = '.';
        temp = (uint8_t)(MESH_VID&0xFF);
        if(temp > 99){
            mesh_hW_ver[len++] = temp/100 + 0x30;
            mesh_hW_ver[len++] = (temp%100)/10 + 0x30;
            mesh_hW_ver[len++] = temp%10 + 0x30;
        }else if(temp > 9){
            mesh_hW_ver[len++] = temp/10 + 0x30;
            mesh_hW_ver[len++] = temp%10 + 0x30;
        }else{
            mesh_hW_ver[len++] = temp + 0x30;
        }
    }

    LOG_DEBUG("mesh_system_id %s \n",mesh_system_id);
    //beacon_set_eddystone_url_advertisement_data();

    #if (CHECK_POWEROFF_VALUE) && (!CHECK_POWEROFF_INTT)
        //wiced_hal_adc_init( );
        //wiced_hal_adc_set_input_range(ADC_RANGE_0_3P6V);
        if(wiced_is_timer_in_use(&powerTestTimer))
        {
            wiced_deinit_timer(&powerTestTimer);
        }
        wiced_init_timer(&powerTestTimer,powerTestTimerCb,0,WICED_MILLI_SECONDS_PERIODIC_TIMER);
        wiced_start_timer(&powerTestTimer,40);
    #endif
}


uint8_t* appGetDevName(void)
{
#if PRESS_TEST == 2
    static uint8_t dev_name[22]="Switch ";
    memcpy(dev_name+7,__DATE__,sizeof(__DATE__));
#elif CHIP_SCHEME == CHIP_DEVKIT
    static uint8_t dev_name[22]="zhw ";
    memcpy(dev_name+4,__DATE__,sizeof(__DATE__));
#else
    static uint8_t dev_name[22] = PRODUCT_CODE;
    #if ANDON_TEST
    dev_name[strlen(dev_name)] = ' ';
    memcpy(dev_name+strlen(dev_name)+1,__DATE__,sizeof(__DATE__));
    #endif
#endif

    uint8_t *pdata;
    pdata = (uint8_t *)wiced_bt_get_buffer(FLASH_XXTEA_SN_LEN);
    if(pdata != NULL)
    {
        memset(mesh_system_id,0,WICED_BT_MESH_PROPERTY_LEN_DEVICE_SERIAL_NUMBER);
        if( FLASH_XXTEA_SN_LEN == storage_read_sn(pdata)){
            //did构成为PRODUCT_CODE+'_'+MAC
            // memcpy(mesh_system_id,pdata+strlen(PRODUCT_CODE)+1,12);
            memcpy(mesh_system_id,pdata,FLASH_XXTEA_ID_LEN);
            memcpy(dev_name,mesh_system_id+strlen(PRODUCT_CODE)+1,12);
            dev_name[12] = 0;
        }else{
            memset(mesh_system_id,0,FLASH_XXTEA_ID_LEN);
        }
    }
    wiced_bt_free_buffer(pdata);

    wiced_bt_cfg_settings.device_name = dev_name;
    return dev_name;
}

// const uint8_t pack1[]={
// 0x54,0x85,0x10,0x40,0x1c,0x8e,0x5f,0x0f,0xf3,0x20,0xfd,0x73,0x90,0x4c,0x44,0x77, 
// 0x0a,0x63,0x60,0xa5,0x4a,0xe4,0x6c,0x84,0xff,0x86,0xb0,0x88,0x98,0x05,0x75,0x8b, 
// 0x6f,0x82,0x18,0x0d,0x0a,0xd9,0x9d,0x0d,0x88,0xb3,0xed,0xa8,0x43,0xd6,0x18,0xb9, 
// 0xb6,0x4c,0xb7,0x3f,0x79,0xef,0x36,0xad,0x64,0x54,0x3a,0x05,0x92,0x2d,0x21,0xeb, 
// 0xd5,0x55,0xd1,0xdf 
// };

// const uint8_t pack2[]={
// 0x55,0x85,0x11,0x1c,0x01,0x6f,0x1d,0xc9,0x36,0xe1,0xd5,0xa8,0xd9,0xcc,0x85,0xbc, 
// 0x67,0xa8,0xe4,0xd4,0xac,0xd1,0x25,0xb9,0x75,0x45,0x36,0xc3,0x49,0x65,0xfc,0x4b, 
// 0x31,0x88,0x7a,0xf1
// };

void appSetAdvFastEnable(void)
{
    fastadvenable = WICED_TRUE;
    appadvenable  = WICED_TRUE;
}

void appSetAdvReEnable(void)
{
    if((advstarttime+60*15) < sysTimerGetSystemRunTime())
    {
        appSetAdvFastEnable();
    }
    advstarttime = sysTimerGetSystemRunTime();
}

void appSetAdvDisable(void)
{
    appadvenable = WICED_FALSE;
    adv_manuDevAdvStop();
    wiced_bt_start_advertisements( BTM_BLE_ADVERT_OFF, BLE_ADDR_RANDOM, NULL );
}

wiced_bool_t appAndonBleConnectUsed(void)
{
    static wiced_bool_t initflag = WICED_TRUE;

    if(LightConfig.bleonly != CONFIG_MESHONLY)
    {
        extern void provisionerSetDevName(void);
        wiced_bt_ble_advert_elem_t adv_elem[2];
        uint8_t *dev_name;
        uint8_t num_elem = 0;
        uint8_t buf[2];
        uint8_t flag = BTM_BLE_GENERAL_DISCOVERABLE_FLAG | BTM_BLE_BREDR_NOT_SUPPORTED;
        uint8_t hello_service_uuid[LEN_UUID_128] = { UUID_ANDON_SERVICE };
        
        if(initflag == WICED_TRUE)
        {
            // extern wiced_bt_gatt_status_t WyzeServerHandle(uint16_t conn_id, wiced_bt_gatt_write_t * p_data);

            // wiced_bt_gatt_write_t p_data;
            // p_data.p_val = (uint8_t*)pack1;
            // p_data.val_len = sizeof(pack1);
            // WyzeServerHandle(0,&p_data);
            // p_data.p_val = (uint8_t*)pack2;
            // p_data.val_len = sizeof(pack2);
            // WyzeServerHandle(0,&p_data);

			if(LightConfig.bleonly == CONFIG_BLEONLY)
            {
                // wiced_bt_dev_set_adv_tx_power(4);
				mesh_node_app_init(WICED_FALSE);
			}
        }

        {
            static uint8_t devdata[16] = {0x70,0x08,0x03,0x06};
            // static uint8_t devdata[16] = {0x70,0x08,0x01,0x02};
            uint16_t node_id = 0;

            // wiced_bt_dev_read_local_addr(devdata+4);

            //此处填充的mac需与DID中的mac保持一致
            LOG_DEBUG("mesh_system_id: %s\n",mesh_system_id);
            for(uint8_t i=0;i<12;i+=2)
            {
                devdata[i/2+4] = HexStr2Int8U(mesh_system_id+i+strlen(PRODUCT_CODE)+1);
            }

            // if(mesh_app_node_is_provisioned())
            if(storageBindkey.bindflag == WICED_TRUE){
                devdata[10] = 0x01;
            }else{
                devdata[10] = 0x00;
            }
            devdata[11] = 0x01;
            // node_id = wiced_bt_mesh_core_get_local_addr();
            devdata[12] = (uint8_t)(MESH_FWID>>8);
            devdata[13] = (uint8_t)(MESH_FWID&0xFF);
            devdata[14] = 0x01;
            adv_elem[num_elem].advert_type  = BTM_BLE_ADVERT_TYPE_FLAG;
            adv_elem[num_elem].len          = sizeof(uint8_t);
            adv_elem[num_elem].p_data       = &flag;
            num_elem++;
            adv_elem[num_elem].advert_type  = BTM_BLE_ADVERT_TYPE_MANUFACTURER;
            adv_elem[num_elem].len          = 15;
            adv_elem[num_elem].p_data       = devdata;
            num_elem++;
        }

        // advStartAndonService(num_elem, adv_elem); 
        wiced_bt_ble_set_raw_advertisement_data(num_elem, adv_elem);
        
        num_elem = 0;
        adv_elem[num_elem].advert_type = BTM_BLE_ADVERT_TYPE_NAME_COMPLETE;
        //adv_elem[num_elem].len = (uint16_t)strlen((const char *)mywiced_bt_cfg_settings.device_name);
        //adv_elem[num_elem].p_data = mywiced_bt_cfg_settings.device_name;
        adv_elem[num_elem].len = (uint16_t)strlen((const char *)wiced_bt_cfg_settings.device_name);
        adv_elem[num_elem].p_data = wiced_bt_cfg_settings.device_name ;
                
        num_elem++;

        adv_elem[num_elem].advert_type = BTM_BLE_ADVERT_TYPE_APPEARANCE;
        adv_elem[num_elem].len = 2;
        buf[0] = (uint8_t)wiced_bt_cfg_settings.gatt_cfg.appearance;
        buf[1] = (uint8_t)(wiced_bt_cfg_settings.gatt_cfg.appearance >> 8);
        adv_elem[num_elem].p_data = buf;
        num_elem++;

        LOG_DEBUG("Start BLE Advertising\n");
        wiced_bt_ble_set_raw_scan_response_data(num_elem, adv_elem);

        if((initflag == WICED_TRUE) || (fastadvenable == WICED_TRUE)){
            fastadvenable = WICED_FALSE;
            wiced_bt_start_advertisements( BTM_BLE_ADVERT_UNDIRECTED_HIGH, BLE_ADDR_RANDOM, NULL );
            advstarttime = sysTimerGetSystemRunTime();
            LOG_DEBUG("Start Time: %d,  SystemRunTime: %d\n", advstarttime, sysTimerGetSystemRunTime());
        }else{
            if(appadvenable){
                if(storageBindkey.bindflag == WICED_FALSE){
                    LOG_DEBUG("Start Time: %d,  SystemRunTime: %d\n", advstarttime, sysTimerGetSystemRunTime());
                    if((advstarttime+60*15) > sysTimerGetSystemRunTime()){
                        wiced_bt_start_advertisements( BTM_BLE_ADVERT_UNDIRECTED_LOW, BLE_ADDR_RANDOM, NULL );
                    }
                }else{
                    wiced_bt_start_advertisements( BTM_BLE_ADVERT_UNDIRECTED_LOW, BLE_ADDR_RANDOM, NULL );
                }
            }
        }
        
        initflag = WICED_FALSE;
        if(LightConfig.bleonly == CONFIG_BLEONLY){
            return WICED_TRUE;
        }
    }
    return WICED_FALSE;

}

// int last_onoff = 1, last_level, last_ctl;
static void mesh_app_attention(uint8_t element_idx, uint8_t time)
{
    LOG_VERBOSE("dimmable light attention:%d sec\n", time);

    if (time)
    {
        // last_onoff = LightConfig.lightingOn;
        // last_level = LightConfig.lightnessLevel;
        // last_ctl = LightConfig.lightnessCTL;
        LightFlash(60, time, 30, 0, 0);
    }
    else
    {
        // LOG_VERBOSE("change to %d %d %d\n", (int)last_onoff, (int)last_level, (int)last_ctl);
        // LightModelTurn(last_onoff, last_level, last_ctl);
        currentCfg.lightingOn = 1;
        currentCfg.lightnessLevel = LightConfig.lightnessLevel;
        currentCfg.lightnessCTL = LightConfig.lightnessCTL;
        led_controller_status_update(currentCfg.lightingOn, currentCfg.lightnessLevel, currentCfg.lightnessCTL);
    }
}

// static void scan_beacon_timer_cb(TIMER_PARAM_TYPE parameter)
// {
//     wiced_stop_timer(&scan_beacon_timer);
//     wiced_deinit_timer(&scan_beacon_timer);
//     LOG_VERBOSE("scan end...\n");
//     scanning = 0;
// }

uint8_t my_on_off_tid = 0;
/*
 * This function is called when core receives a valid message for the define Vendor
 * Model (MESH_VENDOR_COMPANY_ID/MESH_VENDOR_MODEL_ID) combination.  The function shall return TRUE if it
 * was able to process the message, and FALSE if the message is unknown.  In the latter case the core
 * will call other registered models.
 */
//wiced_bool_t mesh_vmesh_vendor_message_handlerendor_server_message_handler(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint16_t data_len)
wiced_bool_t mesh_andon_vendor_message_handler(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint16_t data_len)
{
#if 0
    LOG_VERBOSE("mesh_vendor_server_message_handler: opcode:%x model_id:%x\n", p_event->opcode, p_event->model_id);

    LOG_VERBOSE("key index: %d\n", p_event->app_key_idx);
    LOG_VERBOSE("company id: %d\n", p_event->company_id);
    LOG_VERBOSE("data length: %d\n", data_len);
    LOG_VERBOSE("model_id: %d\n", p_event->model_id);
    LOG_VERBOSE("opcode: %d\n", p_event->opcode);

    WICED_BT_TRACE_ARRAY(p_data,data_len,"");
#endif
    volatile uint64_t tick;
    //uint32_t time;
    uint32_t sec;
    uint32_t ms;
    static int event_count = 0;
   
    tick = wiced_bt_mesh_core_get_tick_count();
    sec  = (tick / 1000) % 1000;
    ms  = tick % 1000;

    LOG_INFO("op:%X cid:%04x mid:0x%X, src:%X dst:%X reply:%d len:%d\n", 
            p_event->opcode, p_event->company_id, p_event->model_id, p_event->src, p_event->dst, p_event->reply, data_len);
    #if LIGHT == 2
    dump_Pool_Info();
    #endif
    event_count++;
    
    if(LightConfig.bleonly == CONFIG_BLEONLY)
    {
        return WICED_FALSE;
    }

    // 0xffff model_id means request to check if that opcode belongs to that model
    if (p_event->model_id == 0xffff)
    {
        switch (p_event->opcode)
        {
        case MESH_VENDOR_OPCODE_GET:
        case MESH_VENDOR_OPCODE_SET:
        case MESH_VENDOR_OPCODE_SET_UNACKED:
        case MESH_VENDOR_OPCODE_STATUS:
            break;
        default:
            return WICED_FALSE;
        }
        return WICED_TRUE;
    }
    // if(p_event->src != wiced_bt_mesh_core_get_local_addr())
    // {
    //     LOG_VERBOSE("%03d.%03d [%d]: opcode = 0x%X, model_id = 0x%X, src = 0x%X, reply = %d, len = %d\n", sec, ms, p_data[1], p_event->opcode, p_event->model_id, p_event->src, p_event->reply, data_len);
    //     event_count++;
    // }
    LOG_DEBUG("mesh_andon_vendor_message_handler_start\n");
    switch (p_event->opcode)
    {
    case MESH_VENDOR_OPCODE_GET:
    case MESH_VENDOR_OPCODE_SET:
    case MESH_VENDOR_OPCODE_SET_UNACKED:
    case MESH_VENDOR_OPCODE_STATUS:
        mesh_vendor_server_process_data(p_event, p_data, data_len);
        break;

    default:
        wiced_bt_mesh_release_event(p_event);
        return WICED_FALSE;
    }
    tick = wiced_bt_mesh_core_get_tick_count();
    sec  = (tick / 1000) % 1000;
    ms  = tick % 1000;
    #if LIGHT == 2
    dump_Pool_Info();
    #endif
    
    LOG_DEBUG("mesh_andon_vendor_message_handler_stop\n");
    LOG_INFO("%03d.%03d [%d]: done\n", sec, ms, event_count - 1);
    LOG_INFO("\n");
    return WICED_TRUE;
}

/*
 * Scene Store Handler.  If the model need to be a part of a scene, store the data in the provided buffer.
 */
uint16_t mesh_vendor_server_scene_store_handler(uint8_t element_idx, uint8_t *p_buffer, uint16_t buffer_len)
{
    // return number of the bytes stored in the buffer.
    return 0;
}

/*
 * Scene Store Handler.  If the model need to be a part of a scene, restore the data from the provided buffer.
 */
uint16_t mesh_vendor_server_scene_recall_handler(uint8_t element_idx, uint8_t *p_buffer, uint16_t buffer_len, uint32_t transition_time, uint32_t delay)
{
    // return number of the bytes read from the buffer.
    return 0;
}

void mesh_vendor_server_process_data(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint16_t data_len)
{
    // if (p_event->src == wiced_bt_mesh_core_get_local_addr())
    // {
    //     wiced_bt_mesh_release_event(p_event);
    //     return;
    // }

    OnVendorPktArrive(p_event, p_data, data_len);
    // if (p_event->reply)
    // {
    //     //replyAck(p_event, p_data, data_len);
    //     //p_event->reply = 0;
    //     // return the data that we received in the command.  Real app can send anything it wants.
    //     mesh_vendor_server_send_status(wiced_bt_mesh_create_reply_event(p_event), p_data, data_len);
    //     //wiced_bt_mesh_release_event(p_event);
    // }
    // else
    {
        wiced_bt_mesh_release_event(p_event);
    }
}

extern void send_callback(wiced_bt_mesh_event_t *p_event);
/*
 * Send Vendor Data status message to the Client
 */
void mesh_vendor_server_send_status(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint16_t data_len)
{
    p_event->opcode = MESH_VENDOR_OPCODE_STATUS;
    p_event->reply = 0;
    wiced_bt_mesh_core_send(p_event, p_data, data_len, send_callback);
}
