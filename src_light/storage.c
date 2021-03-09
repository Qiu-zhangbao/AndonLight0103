/**
 * @file storage.c
 * @author ljy
 * @date 2019-03-03
 * 
 *  NVRAM存储
 * 
 */

#include "storage.h"
#include "config.h"
#include "systimer.h"
#include "light_model.h"
#include "wiced_bt_types.h"
#include "wiced_hal_nvram.h"
#include "raw_flash.h"

#if LIGHTAI == configLIGHTAIANDONMODE
#include "schedule_learn.h"
#endif

#include "log.h"
#include "llog.h"

const LightConfig_def DefaultLightnessConfig = DEFAULT_LIGHTNESS_CONFIG;

storageBindkey_def storageBindkey={0};

static wiced_timer_t store_bindkey_timer;

extern void (*store_cache_timer_cb)(void);
static uint16_t store_delay = 0;

void storageCheckConfigValue(void)
{
    if(LightConfig.offdelayset > 1){
        LightConfig.offdelayset = CONFIG_DEFAULT_DELAYSET;
    }
    if(LightConfig.offdelayset){
        if((LightConfig.offdelaytime < CONFIG_OFFDELAY_MIN) || (LightConfig.offdelaytime > CONFIG_OFFDELAY_MAX)){
            LightConfig.offdelaytime = CONFIG_DEFAULT_OFFDELAY;
        }
        if((LightConfig.ondelaytime < CONFIG_ONDELAY_MIN) || (LightConfig.ondelaytime > CONFIG_ONDELAY_MAX)){
            LightConfig.ondelaytime = CONFIG_DEFAULT_OFFDELAY;
        }
    }
    if((LightConfig.brightdeltastep < CONFIG_DELTASTEP_MIN) || (LightConfig.brightdeltastep > CONFIG_DELTASTEP_MAX)){
        LightConfig.brightdeltastep = CONFIG_DEFAULT_STEP;
    }
    if((LightConfig.powerOnMode > 1)){
        LightConfig.powerOnMode = CONFIG_DEFAULT_POWERONMode;
    }
    if(LightConfig.lightnessLevel < 656){
        LightConfig.lightnessLevel = 656;
    }
}

static void real_store_config_cb(void)
{
    extern void deb_print_buf_use();
    wiced_result_t temp;

    // if (wiced_is_timer_in_use(&store_cache_timer))
    // {
    //     wiced_deinit_timer(&store_cache_timer);
    // }
    if(store_delay)
    {
        store_delay--;
    }
    else
    {
        store_cache_timer_cb = NULL;
        return;
    }
    if(store_delay == 3)
    {
#if ANDON_LIGHT_LOG_ENABLE
        llog_write();
#endif
    }
    else if(store_delay == 2)
    {
        deb_print_buf_use();
        LightConfig.Lightruntime = sysTimerGetSystemRunTime();
        LightConfig.LightUTCtime = sysTimerGetSystemUTCTime();
        LightConfig.Lightzone    = sysTimerGetSystemTimeZone();
        storageCheckConfigValue();
        LOG_DEBUG("store config \n");
        if (sizeof(LightConfig) != flashWriteBlockSave(FLASH_ADDR_CONFIG, (uint8_t *)(&LightConfig), sizeof(LightConfig)))
        {
            LOG_WARNING("store config failed.\n");
        } 
        LightConfig.fwupgrade = 0;
        deb_print_buf_use();
        LOG_DEBUG("store config ok \n");
#if LIGHTAI == configLIGHTAIANDONMODE
    }
    else if(store_delay == 0)
    {
        deb_print_buf_use();
        if (sizeof(AutoBrightnessSet_def) != flashWriteBlockSave(FLASH_ADDR_AUTOBRIGHTNESS, 
                                (uint8_t *)(&AutoBrightnessSet), sizeof(AutoBrightnessSet_def)))
        {
            LOG_WARNING("store AutoBrightness failed.\n");
        }
        store_cache_timer_cb = NULL;
        deb_print_buf_use();
        LOG_DEBUG("store AutoBrightness ok \n");
    }
#else
        store_cache_timer_cb = NULL;
    }
#endif
}

void StoreConfig(void)
{
    // if (wiced_is_timer_in_use(&store_cache_timer))
    // {
    //     wiced_stop_timer(&store_cache_timer);
    // }
    // else
    // {
    //     wiced_init_timer(&store_cache_timer, real_store_config_cb, 0, WICED_MILLI_SECONDS_TIMER);
    // }
    // wiced_start_timer(&store_cache_timer, 1000);
    store_cache_timer_cb = real_store_config_cb;
    store_delay = 5;
}


void StoreConfigDelay(void)
{
    // if (wiced_is_timer_in_use(&store_cache_timer))
    // {
    //     wiced_stop_timer(&store_cache_timer);
    // }
    // else
    // {
    //     wiced_init_timer(&store_cache_timer, real_store_config_cb, 0, WICED_MILLI_SECONDS_TIMER);
    // }
    // wiced_start_timer(&store_cache_timer, 1000);
    store_cache_timer_cb = real_store_config_cb;
    store_delay = 15;
}



wiced_bool_t StoreSaveUserTimer(uint8_t *timer, uint8_t len)
{
    if(len > LENPERBLOCK){
        return WICED_FALSE;
    } 
    if (len != flashWriteBlockSave(FLASH_ADDR_USERTIMER, timer, len)){
        if(len != flash_write_erase(FLASH_ADDR_USERTIMER, timer, len, WICED_TRUE)){
            LOG_WARNING("store Timer failed.\n");
            return WICED_FALSE;
        }
    } 
    return WICED_TRUE;
}


wiced_bool_t StoreBindKey(uint8_t *key, uint8_t len)
{
    if(len > stargeBINDKEYMAXLEN){
        return WICED_FALSE;
    } 
    memset(storageBindkey.bindkey,0,stargeBINDKEYMAXLEN);
    if(len == 0){
        storageBindkey.bindflag = WICED_FALSE;
    }else{
        storageBindkey.bindflag = WICED_TRUE;
        memcpy(storageBindkey.bindkey,key,len);
    }
    storageBindkey.bindkeylen = len;
    if (sizeof(storageBindkey) != flashWriteBlockSave(FLASH_ADDR_BINDKEY, 
                                    (uint8_t *)(&storageBindkey), sizeof(storageBindkey))){
        if(sizeof(storageBindkey) != flash_write_erase(FLASH_ADDR_BINDKEY, 
                                       (uint8_t *)(&storageBindkey), sizeof(storageBindkey), WICED_TRUE)){
            LOG_WARNING("store bindkey failed.\n");
            return WICED_FALSE;
        }
    } 
    return WICED_TRUE;
}

void store_bindkey_timer_cb(uint32_t para)
{
    wiced_deinit_timer(&store_bindkey_timer);
    StoreBindKey(storageBindkey.bindkey,storageBindkey.bindkeylen);
}


void StoreBindKeyDelay(uint8_t *key, uint8_t len)
{
    memset(storageBindkey.bindkey,0,stargeBINDKEYMAXLEN);
    if(len == 0){
        storageBindkey.bindflag = WICED_FALSE;
    }else{
        storageBindkey.bindflag = WICED_TRUE;
        memcpy(storageBindkey.bindkey,key,len);
    }
    storageBindkey.bindkeylen = len;
    if (wiced_is_timer_in_use(&store_bindkey_timer))
    {
        wiced_deinit_timer(&store_bindkey_timer);
    }
    wiced_init_timer(&store_bindkey_timer,store_bindkey_timer_cb,0,WICED_SECONDS_PERIODIC_TIMER);
    wiced_start_timer(&store_bindkey_timer,3);
    
}


void StoreConfigImmediately(void)
{
    // store_delay = 2;
    // real_store_config_cb();
    wiced_hal_wdog_restart();
    LightConfig.Lightruntime = sysTimerGetSystemRunTime();
    LightConfig.LightUTCtime = sysTimerGetSystemUTCTime();
    LightConfig.Lightzone    = sysTimerGetSystemTimeZone();
    if (sizeof(LightConfig) != flashWriteBlockSave(FLASH_ADDR_CONFIG, (uint8_t *)(&LightConfig), sizeof(LightConfig))){
        if(sizeof(LightConfig) != flash_write_erase(FLASH_ADDR_CONFIG, (uint8_t *)(&LightConfig), sizeof(LightConfig),WICED_TRUE)){
            LOG_WARNING("store config failed.\n");
        }
    } 
    wiced_hal_wdog_restart();
    if (sizeof(lightAutoOnOffTimer) != flashWriteBlockSave(FLASH_ADDR_USERTIMER, (uint8_t *)(&lightAutoOnOffTimer), sizeof(lightAutoOnOffTimer))){
        if(sizeof(lightAutoOnOffTimer) != flash_write_erase(FLASH_ADDR_USERTIMER, (uint8_t *)(&lightAutoOnOffTimer), sizeof(lightAutoOnOffTimer),WICED_TRUE)){
            LOG_WARNING("store config failed.\n");
        }
    } 
    wiced_hal_wdog_restart();
#if LIGHTAI == configLIGHTAIANDONMODE
    if (sizeof(AutoBrightnessSet_def) != flashWriteBlockSave(FLASH_ADDR_AUTOBRIGHTNESS, 
                                (uint8_t *)(&AutoBrightnessSet), sizeof(AutoBrightnessSet_def))){
        if(sizeof(AutoBrightnessSet_def) != flash_write_erase(FLASH_ADDR_AUTOBRIGHTNESS, 
                                (uint8_t *)(&AutoBrightnessSet), sizeof(AutoBrightnessSet_def),WICED_TRUE)){
            LOG_WARNING("store AutoBrightness failed.\n");
        }
    }
#endif 
}
void storage_save_upgradeflag(void)
{
    LightConfig.fwupgrade = 1;
    // store_delay = 2;
    // real_store_config_cb();
    StoreConfigImmediately();
}

#if ANDON_TEST
#ifndef DID_NEW_TYPE
static uint8_t ACC_SN1[] = "JA_SL10_20031100012020031100000001";
#else
// static uint8_t ACC_SN1[] = "JA_SL10_7C78B20C0000,cbecc597da630d3573fda59feafe6205";
#if CHIP_SCHEME == CHIP_DEVKIT
// static uint8_t ACC_SN1[] = "JA_SL10_7C78B20C0001,43325c6703f0c24544bef48ff19559c4";
static uint8_t ACC_SN1[] = "JA_SL10_7C78B20C0227,36b8b8873ace527f69d4307b892b70b6";
// static uint8_t ACC_SN1[] = "JA_SL10_7C78B20C0213,e362f42e4764b973b086fcd3ad61bd6a";
#else
// static uint8_t ACC_SN1[] = "JA_SL10_7C78B20C0002,08de22895b2bd75c995b3f8d0b96076c";
// static uint8_t ACC_SN1[] = "JA_SL10_7C78B20C020F,fe0b7ee7430195f836fca7eba65ae0c6";
static uint8_t ACC_SN1[] = "JA_SL10_7C78B20C021E,a83effbc2e07fa40789484a5113747a4";
#endif

// static uint8_t ACC_SN1[] = "CO_TH1_firmware_test,CO_TH1_firmware_test";
#endif
#endif
uint16_t storage_read_sn(uint8_t *data)
{
    LOG_DEBUG("sn addr:%08x\n",FLASH_ADDR_XXTEA_SN);
    if(0 == flash_app_read_mem(FLASH_ADDR_XXTEA_SN,data,FLASH_XXTEA_SN_LEN))
    {
        memset(data,0xFF,FLASH_XXTEA_SN_LEN);
        LOG_WARNING("storage_read_sn failed.\n");
        #if ANDON_TEST
            memcpy(data,ACC_SN1,FLASH_XXTEA_SN_LEN);
        #endif
    }
    return FLASH_XXTEA_SN_LEN;
}


#if ANDON_TEST

void storage_save_keyandsn(void)
{
    if(0 == flash_app_write_mem(FLASH_ADDR_XXTEA_SN,ACC_SN1,FLASH_XXTEA_SN_LEN))
    {
        LOG_WARNING("storage_save_sn failed.\n");
    }
    PrintfXxteakeyandsn();
}

void PrintfXxteakeyandsn(void)
{
    uint8_t data[FLASH_XXTEA_SN_LEN+1]={0};
    
    storage_read_sn(data);
    WICED_BT_TRACE_ARRAY(data,FLASH_XXTEA_SN_LEN,"\nRead Did: ");
    LOG_DEBUG("Read Did:%s\n", data);
}

#endif

uint8_t LoadConfig(void)
{
    extern void deb_print_buf_use();
    deb_print_buf_use();
    wiced_result_t temp;
    int i = 5;

    //??bindkey
    memset((uint8_t *)(&storageBindkey), 0, sizeof(storageBindkey));
    if(sizeof(storageBindkey) != flashReadBlockSave(FLASH_ADDR_BINDKEY, (uint8_t *)(&storageBindkey), sizeof(storageBindkey)))
    {
        storageBindkey.bindflag = WICED_FALSE;
        storageBindkey.bindkeylen = 0;
    }
    
    memset((uint8_t *)(&lightAutoOnOffTimer), 0, sizeof(lightAutoOnOffTimer));
    if(sizeof(lightAutoOnOffTimer) != flashReadBlockSave(FLASH_ADDR_USERTIMER, (uint8_t *)(&lightAutoOnOffTimer), sizeof(lightAutoOnOffTimer)))
    {
        lightAutoOnOffTimer.num = 0;
    }

    if (sizeof(LightConfig) == flashReadBlockSave(FLASH_ADDR_CONFIG, (uint8_t *)(&LightConfig), sizeof(LightConfig)))
    {
        if(((LightConfig.Lightontime != 0xFFFFFFFF) && (LightConfig.offdelayset != 0xFF)))
        {
            storageCheckConfigValue();
            goto LOAD_OK_LIGHTCONFIG;
        }
    } 
    LightConfig = DefaultLightnessConfig;
    LOG_WARNING("load config failed. use default config. PoweronMode %d \n",LightConfig.powerOnMode);
#if LIGHTAI == configLIGHTAIANDONMODE 
LOAD_OK_LIGHTCONFIG:
    AutoBrightnessSet.Item.AutoBrightnessSetSave = 0;
    //if (sizeof(AutoBrightnessSet_def) == wiced_hal_read_nvram(NVRAM_AUTOBRIGHTNESS_POSITION, sizeof(AutoBrightnessSet_def), (uint8_t *)&AutoBrightnessSet, &temp))
    if (sizeof(AutoBrightnessSet_def) == flashReadBlockSave(FLASH_ADDR_AUTOBRIGHTNESS,
                                             (uint8_t *)(&AutoBrightnessSet), sizeof(AutoBrightnessSet_def)))
    {
        if(AutoBrightnessSet.Item.AutoBrightnessNum <= LIGHT_AUTOBRIGHTNESSMAXNUM)
        {
            goto LOAD_OK;
        }
    }
    AutoBrightnessSet.Item.AutoBrightnessSetSave = 0;
LOAD_AUTOBRIGHTNESSERR:
    AutoBrightnessSet = DEFAULT_AUTOLIGHTNESS_CONFIG;
    LOG_WARNING("load config failed. use default config.\n");
    return 0;
LOAD_OK:
    // LOG_DEBUG("load config ok.\n");
    return 1;
#else
    return 0;
LOAD_OK_LIGHTCONFIG:    
    return 1;
#endif

}



extern void advReSetRemoteCtr(void);

void ResetConfig(void)
{
    uint32_t temp1;
    uint8_t temp2;
    LOG_DEBUG("Reset Factory \n");
    temp1 = LightConfig.Lightontime;
    temp2 = LightConfig.fristpair;
    LightConfig = DefaultLightnessConfig;
    LightConfig.Lightontime = temp1;
    LightConfig.fristpair   = temp2;
    // StoreConfig();
#if LIGHTAI == configLIGHTAIANDONMODE
    memset(&AutoBrightnessSet,0,sizeof(AutoBrightnessSet_def));
    AutoBrightnessSet.Item.AutoBrightnessSetSave = 1;
#endif
    memset((uint8_t *)(&lightAutoOnOffTimer),0,sizeof(lightAutoOnOffTimer));
    //llogsaveflag = 1;
    // store_delay = 3;
    
    wiced_hal_wdog_restart();
    StoreBindKey(NULL,0);
    advReSetRemoteCtr();
    StoreConfigImmediately();
}

void ResetToolConfig(void)
{
    uint32_t temp1;
    uint8_t temp2;
    LOG_DEBUG("Reset Tool Factory \n");
    LightConfig = DefaultLightnessConfig;
    // StoreConfig();
#if LIGHTAI == configLIGHTAIANDONMODE
    memset(&AutoBrightnessSet,0,sizeof(AutoBrightnessSet_def));
    AutoBrightnessSet.Item.AutoBrightnessSetSave = 1;
#endif
    
    // wiced_hal_wdog_restart();
    memset((uint8_t *)(&storageBindkey), 0, sizeof(storageBindkey));
    if(sizeof(storageBindkey) != flash_write_erase(FLASH_ADDR_BINDKEY, (uint8_t *)(&storageBindkey), sizeof(storageBindkey), WICED_TRUE))
    {
        LOG_WARNING("store bindkey failed.\n");
    }
    
    // wiced_hal_wdog_restart();
    memset((uint8_t *)(&lightAutoOnOffTimer), 0, sizeof(lightAutoOnOffTimer));
    if(sizeof(lightAutoOnOffTimer) != flash_write_erase(FLASH_ADDR_USERTIMER, (uint8_t *)(&lightAutoOnOffTimer), sizeof(lightAutoOnOffTimer), WICED_TRUE))
    {
        LOG_WARNING("store bindkey failed.\n");
    }
    
    // wiced_hal_wdog_restart();
    advReSetRemoteCtr();
    if(sizeof(LightConfig) != flash_write_erase(FLASH_ADDR_CONFIG, (uint8_t *)(&LightConfig), sizeof(LightConfig),WICED_TRUE))
    {
        LOG_WARNING("store config failed.\n");
    }
    
    // wiced_hal_wdog_restart();
#if LIGHTAI == configLIGHTAIANDONMODE
    if(sizeof(AutoBrightnessSet_def) != flash_write_erase(FLASH_ADDR_AUTOBRIGHTNESS, 
                                (uint8_t *)(&AutoBrightnessSet), sizeof(AutoBrightnessSet_def),WICED_TRUE))
    {
        LOG_WARNING("store AutoBrightness failed.\n");
    }
#endif
}

