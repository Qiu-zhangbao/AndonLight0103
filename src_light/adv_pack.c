/*
 * Copyright 2018, Cypress Semiconductor Corporation or a subsidiary of Cypress Semiconductor
 * Corporation. All rights reserved. This software, including source code, documentation and
 * related materials ("Software"), is owned by Cypress Semiconductor  Corporation or one of its
 * subsidiaries ("Cypress") and is protected by and subject to worldwide patent protection 
 * (United States and foreign), United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license agreement accompanying the
 * software package from which you obtained this Software ("EULA"). If no EULA applies, Cypress
 * hereby grants you a personal, nonexclusive, non-transferable license to  copy, modify, and
 * compile the Software source code solely for use in connection with Cypress's  integrated circuit
 * products. Any reproduction, modification, translation, compilation,  or representation of this
 * Software except as specified above is prohibited without the express written permission of
 * Cypress. Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO  WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING,  BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE. Cypress reserves the right to make changes to the
 * Software without notice. Cypress does not assume any liability arising out of the application
 * or use of the Software or any product or circuit  described in the Software. Cypress does
 * not authorize its products for use in any products where a malfunction or failure of the
 * Cypress product may reasonably be expected to result  in significant property damage, injury
 * or death ("High Risk Product"). By including Cypress's product in a High Risk Product, the
 * manufacturer of such system or application assumes  all risk of such use and in doing so agrees
 * to indemnify Cypress against all liability.
 */

/** @file
 *
 * This file shows how to handle device LPN + SDS sleep implementation
 */
#include "wiced_bt_ble.h"
#include "wiced_bt_trace.h"
#include "wiced_hal_gpio.h"
#include "wiced_hal_wdog.h"
#include "wiced_timer.h"
#include "wiced_memory.h"
#include "wiced_bt_mesh_models.h"
#ifdef CYW20706A2
#include "wiced_bt_app_hal_common.h"
#else
#include "wiced_platform.h"
#endif

#include "wiced_bt_cfg.h"
#include "adv_cmd_decl.h"
#include "adv_pack.h"
#include "AES.h"

#include "wiced_hal_nvram.h"
#include "pwm_control.h"
#include "storage.h"

#include "light_model.h"

#include "log.h"
#include "raw_flash.h"
#include "wiced_bt_mesh_self_config.h"
#include "systimer.h"
#include "LightPackage.h"

//#define WICED_LOG_DEBUG(...) LOG_VERBOSE(__VA_ARGS__)
//#define ADV_RECE_FIFO_LEN 50
//#define ADV_SEND_FIFO_LEN 20
//#define ADV_PACK_LEN 26

#define adv_fifo_iptr_plus(a, b) ((a + b) % ADV_RECE_FIFO_LEN)
#define adv_fifo_optr_plus(a, b) ((a + b) % ADV_SEND_FIFO_LEN)
#define adv_send_seq_plus(a, b) ((a + b) & 0xFFFFFF)

//#define ADV_CMD_TTL            0         //
#define ADV_CMD_RETRYTIMES       10

#define ADV_SENDIDLE 0x00
#define ADV_SENDING 0x01
#define ADV_SEND_WAITING 0x02

#define advFreeBuffer(p)       do{              \
    wiced_bt_free_buffer(p);                    \
    p = NULL;                                   \
}while(0)


#define  ADV_CMDRPL_LEN                        (40)
#define  ADV_CMD_TIMEOUT                       (10)
#define  ADVSENDDATA_TIME                      (3)

#define  ADVSENDSTATUS_IDLE                     0
#define  ADVSENDSTATUS_DELAY                    1
#define  ADVSENDSTATUS_DOING                    2

#define  AES_ENABLE                            (1)
#define  AUTO_PAIRED_ENABLE                    (0)
/******************************************************
 *          Constants
 ******************************************************/

/******************************************************
 *          Structures
 ******************************************************/
typedef union {
    struct
    {
        uint8_t adv_len;
        uint8_t adv_type;
        uint16_t company_id;
        uint8_t manu_name[3];
        uint8_t ttl;
        uint8_t crc_check[2];
        uint8_t seq;
        uint8_t segment;
        wiced_bt_device_address_t remote_mac;
        uint8_t remote_cmd[2];
        uint8_t cmd_load[8];
    } item;
    uint8_t adv_array[ADV_PACK_LEN];
} adv_rece_data_t;

typedef union {
    struct
    {
        uint8_t crc_check[2];
        uint8_t seq;
        uint8_t segment;
        wiced_bt_device_address_t remote_mac;
    } item;
    uint8_t adv_array[10];
} adv_rece_rpl_t;

typedef struct {
    wiced_bt_device_address_t remote_mac;
    uint8_t  seq;
    uint32_t arrival_time;
} adv_cmdrpl_t;

typedef union {
    struct
    {
        uint8_t adv_len;
        uint8_t adv_type;
        uint16_t company_id;
        uint8_t manu_name[3];
        uint8_t ttl;
        uint8_t crc_check[2];
        uint8_t seq;
        uint8_t segment;
        wiced_bt_device_address_t local_mac;
        uint8_t local_cmd[2];
        uint8_t cmd_load[8];
        uint8_t count;
        uint8_t delay;
        uint32_t sendtime;
    } item;
    uint8_t adv_array[ADV_PACK_LEN + 1];
} adv_send_data_t;

typedef struct
{
    uint16_t cmd;
    void *cback;
} adv_receive_model_t;


/******************************************************
 *          Function Prototypes
 ******************************************************/
//extern void wiced_bt_btn_led_onoff(uint8_t onoff);
#define wiced_bt_btn_led_onoff(x) \
    {                             \
    }

static void mesh_pair_timer_callback(uint32_t arg);
wiced_bool_t adv_recevier_handle(wiced_bt_ble_scan_results_t *p_scan_result, uint8_t *p_adv_data);
void adv_manuadv_send_handle(uint32_t params);
void adv_paircmd_handle(wiced_bt_device_address_t remote_mac, uint16_t cmd, uint8_t *p_adv_data, int8_t len);
//void adv_printf_message(wiced_bt_device_address_t remote_mac, uint16_t cmd, uint8_t *p_adv_data, uint8_t len);
void adv_reset_handle(wiced_bt_device_address_t remote_mac, uint16_t cmd, uint8_t *p_adv_data, uint8_t len);
void adv_delta_brightness(wiced_bt_device_address_t remote_mac, uint16_t cmd, uint8_t *data, uint8_t len);
void adv_set_onoff(wiced_bt_device_address_t remote_mac, uint16_t cmd, uint8_t *data, uint8_t len);
void adv_toggle_onoff(wiced_bt_device_address_t remote_mac, uint16_t cmd, uint8_t *data, uint8_t len);
void adv_remoteactionrespon(wiced_bt_device_address_t remote_mac, uint16_t cmd, uint8_t *data, uint8_t len);
void PrintfRemote(void);
void advReSetRemoteCtr(void);
void adv_manuadv_send(uint8_t *data, uint8_t len);

extern void tooling_handle(wiced_bt_ble_scan_results_t *p_scan_result,uint16_t cmd, uint8_t *data, uint8_t len);
extern wiced_bool_t mesh_app_node_is_provisioned(void);

/******************************************************
 *          Variables Definitions
 ******************************************************/
// static adv_rece_data_t adv_fifo_in[ADV_RECE_FIFO_LEN] = {0};
// static adv_send_data_t adv_fifo_out[ADV_SEND_FIFO_LEN] = {0};
static adv_rece_rpl_t *adv_fifo_in = NULL;
static adv_send_data_t *adv_fifo_out = NULL;
static adv_cmdrpl_t    *adv_cmdrpl = NULL;
static uint16_t adv_fifo_in_iptr = 0;
static uint16_t adv_fifo_out_optr = 0;
static uint16_t adv_fifo_out_iptr = 0;
static uint16_t adv_send_enable = ADVSENDSTATUS_IDLE;
static uint32_t adv_send_seq = 0;
static uint32_t advSendPairAck = 0;

//static wiced_bt_device_address_t   adv_remotectr_mac = {0};

extern int16_t stored_level;

adv_receive_model_t adv_receive_models[] =
{
    {ADV_CMD_PARI, adv_paircmd_handle},
    {ADV_CMD_RESET, adv_reset_handle},
    {ADV_CMD_TOOLING, tooling_handle},
    {VENDOR_DELTABRIGHTNESS_CMD, adv_delta_brightness},
    {VENDOR_SETONOFF_CMD, adv_set_onoff},
    {VENDOR_SETTOGGLEONOFF_CMD, adv_toggle_onoff},
    {VENDOR_SETREMOTEACTION_CMD, adv_remoteactionrespon},
};

typedef wiced_bool_t (wiced_bt_ble_userscan_result_cback_t) (wiced_bt_ble_scan_results_t *p_scan_result, uint8_t *p_adv_data);
extern wiced_bt_ble_userscan_result_cback_t *adv_manufacturer_handler;

const uint8_t aes128key[16] = "com.jiuan.SLight";
#if CHIP_SCHEME == CHIP_DEVKIT
const uint8_t adv_send_data_head[] = {ADV_PACK_LEN - 1, BTM_BLE_ADVERT_TYPE_MANUFACTURER, 0x04,0x08,'Q', 'Z', 'B'};
// const uint8_t adv_send_data_head[] = {ADV_PACK_LEN - 1, BTM_BLE_ADVERT_TYPE_MANUFACTURER, 0x04,0x08,'W', 'y', 'z'};
#else
const uint8_t adv_send_data_head[] = {ADV_PACK_LEN - 1, BTM_BLE_ADVERT_TYPE_MANUFACTURER, 0x04,0x08,'W', 'y', 'z'};
#endif

wiced_bt_ble_multi_adv_params_t adv_pairadv_params =
 {
    .adv_int_min = 80,                                                                      /**< Minimum adv interval */
    .adv_int_max = 96,                                                                      /**< Maximum adv interval */
    .adv_type = MULTI_ADVERT_NONCONNECTABLE_EVENT,                                          /**< Adv event type */
    .channel_map = BTM_BLE_DEFAULT_ADVERT_CHNL_MAP,                                         /**< Adv channel map */
    .adv_filter_policy = BTM_BLE_ADVERT_FILTER_WHITELIST_CONNECTION_REQ_WHITELIST_SCAN_REQ, /**< Advertising filter policy */
    .adv_tx_power = MULTI_ADV_TX_POWER_MAX,                                                 /**< Adv tx power */
    .peer_bd_addr = {0},                                                                    /**< Peer Device address */
    .peer_addr_type = BLE_ADDR_RANDOM,                                                      /**< Peer LE Address type */
    .own_bd_addr = {0},                                                                     /**< Own LE address */
    .own_addr_type = BLE_ADDR_RANDOM                                                        /**< Own LE Address type */
};

wiced_bt_ble_multi_adv_params_t adv_manudevadv_params =
 {
    .adv_int_min = 800,                                                                      /**< Minimum adv interval */
    .adv_int_max = 960,                                                                      /**< Maximum adv interval */
    .adv_type = MULTI_ADVERT_NONCONNECTABLE_EVENT,                                          /**< Adv event type */
    .channel_map = BTM_BLE_DEFAULT_ADVERT_CHNL_MAP,                                         /**< Adv channel map */
    .adv_filter_policy = BTM_BLE_ADVERT_FILTER_WHITELIST_CONNECTION_REQ_WHITELIST_SCAN_REQ, /**< Advertising filter policy */
    .adv_tx_power = MULTI_ADV_TX_POWER_MAX,                                                 /**< Adv tx power */
    // .adv_tx_power = -4,                                                                     /**< Adv tx power */
    .peer_bd_addr = {0},                                                                    /**< Peer Device address */
    .peer_addr_type = BLE_ADDR_RANDOM,                                                      /**< Peer LE Address type */
    .own_bd_addr = {0},                                                                     /**< Own LE address */
    .own_addr_type = BLE_ADDR_RANDOM                                                        /**< Own LE Address type */
};

// wiced_bt_ble_multi_adv_params_t adv_andonserviceadv_params =
//  {
//     .adv_int_min = 320,                                                                     /**< Minimum adv interval */
//     .adv_int_max = 400,                                                                     /**< Maximum adv interval */
//     .adv_type = MULTI_ADVERT_CONNECTABLE_UNDIRECT_EVENT,                                            /**< Adv event type */
//     .channel_map = BTM_BLE_DEFAULT_ADVERT_CHNL_MAP,                                         /**< Adv channel map */
//     .adv_filter_policy = BTM_BLE_ADVERT_FILTER_WHITELIST_CONNECTION_REQ_WHITELIST_SCAN_REQ, /**< Advertising filter policy */
//     .adv_tx_power = MULTI_ADV_TX_POWER_MAX,                                                 /**< Adv tx power */
//     .peer_bd_addr = {0},                                                                    /**< Peer Device address */
//     .peer_addr_type = BLE_ADDR_RANDOM,                                                      /**< Peer LE Address type */
//     .own_bd_addr = {0},                                                                     /**< Own LE address */
//     .own_addr_type = BLE_ADDR_RANDOM                                                        /**< Own LE Address type */
// };


adv_pair_handler_t adv_pair_params =
{
    .adv_pair_enable = WICED_FALSE,
    .adv_pair_runcount = 0,
    .adv_pair_select = WICED_FALSE,
    //此结构体以下部分会被写入到NVRAM中，每次上电初始化的元素不要放在此行以下
    .adv_remotectr_mac = {0},
    .adv_remotectr_pairindex = 0
};

void adv_delta_brightness(wiced_bt_device_address_t remote_mac, uint16_t cmd, uint8_t *data, uint8_t len)
{
    LightModelDeltaBrightness(((int8_t *)data)[0], data[1], data[2]);
}

void adv_set_onoff(wiced_bt_device_address_t remote_mac, uint16_t cmd, uint8_t *data, uint8_t len)
{
    // LightModelTurn(data[0], data[1], data[2]);
    LightModelTurn(data[0], 0, data[2]);
}

void adv_toggle_onoff(wiced_bt_device_address_t remote_mac, uint16_t cmd, uint8_t *data, uint8_t len)
{
    LightModelToggle(0,data[0], data[1]);
}

void adv_remoteactionrespon(wiced_bt_device_address_t remote_mac, uint16_t cmd, uint8_t *data, uint8_t len)
{
    packageReply reply;
    uint8_t *p_data;

    //此处做一个协议格式转换
    LOG_DEBUG("adv_remoteactionrespon \n");
    p_data = wiced_bt_get_buffer(7);
    memcpy(p_data+4,data,len);
    p_data[0] = 0;
    p_data[1] = 0;
    p_data[2] = (cmd>>8)&0xff;
    p_data[3] = cmd&0xff;
    p_data[4] = data[0];
    p_data[5] = data[1];
    p_data[6] = data[2];
    reply = lightpackUnpackMsg(p_data,7);
    advFreeBuffer(p_data);
    if(reply.p_data){
        advFreeBuffer(reply.p_data);
    }
}
/******************************************************
 *               Function Definitions
 ******************************************************/

uint16_t CrcCalc(uint8_t *data, uint16_t length)
{
    uint16_t i;
    uint8_t j;
    union {
        uint16_t CRCX;
        uint8_t CRCY[2];
    } CRC;

    CRC.CRCX = 0xFFFF;
    for (i = 0; i < length; i++)
    {
        CRC.CRCY[0] = (CRC.CRCY[0] ^ data[i]);
        for (j = 0; j < 8; j++)
        {
            if ((CRC.CRCX & 0X0001) == 1)
                CRC.CRCX = (CRC.CRCX >> 1) ^ 0X1021;
            else
                CRC.CRCX >>= 1;
        }
    }
    return CRC.CRCX;
}

static void mesh_generate_random(uint8_t* random, uint8_t len)
{
#include "wiced_hal_rand.h"
    uint32_t r;
    uint8_t l;
    while (len)
    {
        r = wiced_hal_rand_gen_num();
        l = len > 4 ? 4 : len;
        memcpy(random, &r, l);
        len -= l;
        random += l;
    }
}

static void mesh_pair_timer_callback(uint32_t arg)
{
    volatile uint64_t tick;
    //uint32_t time;
    uint32_t sec;
    uint32_t ms;

    tick = wiced_bt_mesh_core_get_tick_count();
    sec  = (tick / 1000) % 1000;
    ms  = tick % 1000;
    if(((sec%5) == 1) && (ms == 0))
    {
        LOG_DEBUG("-1....ADV send start\n");
    }
    if (ADVSENDSTATUS_DELAY == adv_send_enable)
    {
        LOG_DEBUG("0....ADV send start\n");
        if(adv_fifo_out[adv_fifo_out_optr].item.delay == 0){
            adv_send_enable = ADVSENDSTATUS_DOING;
            LOG_DEBUG("1....ADV send start\n");
            adv_manuadv_send(adv_fifo_out[adv_fifo_out_optr].adv_array, ADV_PACK_LEN);
        }else{
            adv_fifo_out[adv_fifo_out_optr].item.delay--;
        }
    }
    //未处于配网过程中
    if (WICED_FALSE == adv_pair_params.adv_pair_enable )
    {
        return;
    }
    if (adv_pair_params.adv_pair_runcount == 0)
    {
        if(WICED_TRUE == adv_pair_params.adv_pair_select)
        {
            LOG_DEBUG("Light stop flash!!!\n");
            LightFlash(0, 0, 0, 0, 0);
        }
        adv_pair_disable();
        return;
    }
    adv_pair_params.adv_pair_runcount--;

    tick = wiced_bt_mesh_core_get_tick_count();
    sec  = (tick / 1000) % 1000;
    ms  = tick % 1000;

}

void adv_manuDevAdvStop(void)
{
    LOG_DEBUG("stop nonconnect adv.......\n");
    wiced_start_multi_advertisements(MULTI_ADVERT_STOP, ADV_MANUDEVADV_INDEX);
}

/*
 *  manufacturer adv message
 */
void adv_manuDevAdvStart(uint8_t *data, uint8_t len)
{
    wiced_result_t wiced_result;

    wiced_start_multi_advertisements(MULTI_ADVERT_STOP, ADV_MANUDEVADV_INDEX);

    wiced_result = wiced_set_multi_advertisement_data(data, len, ADV_MANUDEVADV_INDEX);
    if (WICED_BT_SUCCESS != wiced_result)
    {
        return;
    }

    wiced_bt_dev_read_local_addr(adv_manudevadv_params.own_bd_addr);
    wiced_result = wiced_set_multi_advertisement_params(ADV_MANUDEVADV_INDEX, &adv_manudevadv_params);
    if (WICED_BT_SUCCESS != wiced_result)
    {
        return;
    }

    wiced_result = wiced_bt_notify_multi_advertisement_packet_transmissions(ADV_MANUDEVADV_INDEX, NULL, 8);
    if (TRUE != wiced_result)
    {
        return;
    }

    wiced_result = wiced_start_multi_advertisements(MULTI_ADVERT_START, ADV_MANUDEVADV_INDEX);
    if (WICED_BT_SUCCESS != wiced_result)
    {
        return;
    }
}
/*
 *  manufacturer adv message
 */
void adv_manuadv_send(uint8_t *data, uint8_t len)
{
    wiced_result_t wiced_result;

    wiced_start_multi_advertisements(MULTI_ADVERT_STOP, ADV_PAIRADV_INDEX);

    wiced_result = wiced_set_multi_advertisement_data(data, len, ADV_PAIRADV_INDEX);
    if (WICED_BT_SUCCESS != wiced_result)
    {
        return;
    }

    wiced_bt_dev_read_local_addr(adv_pairadv_params.own_bd_addr);
    wiced_result = wiced_set_multi_advertisement_params(ADV_PAIRADV_INDEX, &adv_pairadv_params);
    if (WICED_BT_SUCCESS != wiced_result)
    {
        return;
    }

    wiced_result = wiced_bt_notify_multi_advertisement_packet_transmissions(ADV_PAIRADV_INDEX, adv_manuadv_send_handle, 8);
    if (TRUE != wiced_result)
    {
        return;
    }

    wiced_result = wiced_start_multi_advertisements(MULTI_ADVERT_START, ADV_PAIRADV_INDEX);
    if (WICED_BT_SUCCESS != wiced_result)
    {
        return;
    }
}

void adv_send_Scheduling(void)
{
    if (adv_fifo_out_iptr != adv_fifo_out_optr)
    {
        if (ADVSENDSTATUS_IDLE == adv_send_enable)
        {
            //adv_manuadv_send(adv_fifo_out[adv_fifo_out_optr].adv_array, ADV_PACK_LEN);
            while(adv_fifo_out_iptr != adv_fifo_out_optr)
            {
                if(adv_fifo_out[adv_fifo_out_optr].item.sendtime + ADVSENDDATA_TIME > sysTimerGetSystemRunTime())
                {
                    break;
                }
                adv_fifo_out_optr = adv_fifo_optr_plus(adv_fifo_out_optr, 1);
            }
            if (adv_fifo_out_iptr == adv_fifo_out_optr)
            {
                wiced_start_multi_advertisements(MULTI_ADVERT_STOP, ADV_PAIRADV_INDEX);
                adv_send_enable = ADVSENDSTATUS_IDLE;
                return;
            }
            if(adv_fifo_out[adv_fifo_out_optr].item.delay)
            {
                adv_send_enable = ADVSENDSTATUS_DELAY;
            }
            else
            {
                adv_send_enable = ADVSENDSTATUS_DOING;
                adv_manuadv_send(adv_fifo_out[adv_fifo_out_optr].adv_array, ADV_PACK_LEN);
            }
        }
    }
    else
    {
        //还在发送过程中，说明发送缓存满了
        if(ADVSENDSTATUS_IDLE != adv_send_enable)
        {
            memset(adv_fifo_out[adv_fifo_out_optr].adv_array,0,sizeof(adv_send_data_t));
            adv_fifo_out_optr = adv_fifo_optr_plus(adv_fifo_out_optr, 1);
            while(adv_fifo_out_iptr != adv_fifo_out_optr)
            {
                if(adv_fifo_out[adv_fifo_out_optr].item.sendtime + ADVSENDDATA_TIME > sysTimerGetSystemRunTime())
                {
                    break;
                }
                adv_fifo_out_optr = adv_fifo_optr_plus(adv_fifo_out_optr, 1);
            }
            if (adv_fifo_out_iptr == adv_fifo_out_optr)
            {
                wiced_start_multi_advertisements(MULTI_ADVERT_STOP, ADV_PAIRADV_INDEX);
                adv_send_enable = ADVSENDSTATUS_IDLE;
                return;
            }
            //adv_manuadv_send(adv_fifo_out[adv_fifo_out_optr].adv_array, ADV_PACK_LEN);
            if(adv_fifo_out[adv_fifo_out_optr].item.delay)
            {
                adv_send_enable = ADVSENDSTATUS_DELAY;
            }
            else
            {
                adv_send_enable = ADVSENDSTATUS_DOING;
                adv_manuadv_send(adv_fifo_out[adv_fifo_out_optr].adv_array, ADV_PACK_LEN);
            }
        }
        else //发送完成
        {
            wiced_start_multi_advertisements(MULTI_ADVERT_STOP, ADV_PAIRADV_INDEX);
            adv_send_enable = ADVSENDSTATUS_IDLE;
        }
    }
}
/*
 * adv send event handle
 */
void adv_manuadv_send_handle(uint32_t params)
{
    uint16_t j = 0;

    // Note!!!!!   防止SDK运行异常,意外抛出不使用这个广播的发送状态回调，
    //但是这么做是有风险的，手册里说的是此处返回是结果，并不会和ADV_MANUADV_INDEX关联 实际测试时关联的  params= ADV_MANUADV_INDEX*256 + 状态
    // if (WICED_BT_ADV_NOTIFICATION_DONE == (params & 0xFF))
    // {
    //     LOG_DEBUG("send_handle = 0x%04x\n", params);
    // }
    if (ADV_PAIRADV_INDEX != params / 256){
        return;
    }
    if ((WICED_BT_ADV_NOTIFICATION_DONE == (params & 0xFF)) && (ADVSENDSTATUS_DOING == adv_send_enable)){
        //仅在主动发送完成后将发送指针移动，防止SDK运行异常,意外抛出不使用这个广播的发送状态回调
        //这种防错机制比上面的要靠谱
        if (adv_fifo_out[adv_fifo_out_optr].item.count > 1){
            adv_fifo_out[adv_fifo_out_optr].item.count--;
        }else{
            memset(adv_fifo_out[adv_fifo_out_optr].adv_array,0,sizeof(adv_send_data_t));
            adv_fifo_out_optr = adv_fifo_optr_plus(adv_fifo_out_optr, 1);
            adv_send_enable = ADVSENDSTATUS_IDLE;
            adv_send_Scheduling();
        }
    }
}

//*****************************************************************************
// 函数名称: appendAdvCmdRpl
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
wiced_bool_t appendAdvCmdRpl(wiced_bt_device_address_t remote_mac, uint8_t seq)
{
    uint32_t systimer;
    int slot = 0;
    // LOG_VERBOSE("hash: %x\n", temp.hash);
    systimer = sysTimerGetSystemRunTime();
    for (int i = 0; i < ADV_CMDRPL_LEN; i++){
        if(0 == memcmp(remote_mac,adv_cmdrpl[i].remote_mac,sizeof(wiced_bt_device_address_t))) {
            if(seq > adv_cmdrpl[i].seq) {                                     //Seq每次发送自增
                adv_cmdrpl[i].seq = seq;
                adv_cmdrpl[i].arrival_time = systimer;
                return TRUE;
            }else{
                if(adv_cmdrpl[i].arrival_time + ADV_CMD_TIMEOUT < systimer){  //数据接收超时
                    adv_cmdrpl[i].seq = seq;
                    adv_cmdrpl[i].arrival_time = systimer;
                    return TRUE;
                }
                else if((seq < 0x20) && (adv_cmdrpl[i].seq > 0xE0)){          //seq在ADV_CMD_TIMEOUT内不会突变
                    adv_cmdrpl[i].seq = seq;
                    adv_cmdrpl[i].arrival_time = systimer;
                    return TRUE;
                }else{
                    return FALSE;
                }
            }
        }
        if (adv_cmdrpl[i].arrival_time < adv_cmdrpl[slot].arrival_time){
            slot = i;
        }
    }

    memcpy(adv_cmdrpl[slot].remote_mac,remote_mac,sizeof(wiced_bt_device_address_t));
    adv_cmdrpl[slot].seq = seq;
    adv_cmdrpl[slot].arrival_time = systimer;
    return TRUE;
}


//wiced_bt_mesh_event_t advtomeshp_event;
//uint8_t     advtomesh_data[12];
//uint8_t     advtomesh_datalen;
/*
 * recevier manufacturer adv message handle
 */
wiced_bool_t adv_recevier_handle(wiced_bt_ble_scan_results_t *p_scan_result, uint8_t *p_adv_data)
{
    adv_rece_data_t adv_data;
    wiced_bt_device_address_t own_bd_addr;
    uint16_t rece_cmd;

    uint16_t rece_crc;
    uint32_t seed;
    extern wiced_platform_led_config_t platform_led[];    
   
    //数据头不匹配，不做解析
    if (0 != memcmp(p_adv_data, adv_send_data_head, sizeof(adv_send_data_head)))
    {
        return WICED_FALSE;
    }
    for (uint16_t i = 0; i < ADV_RECE_FIFO_LEN; i++) //检验数据是否已存储在缓冲中
    {
        //数据比对时 不对ttl进行比对
        if (0 == memcmp(p_adv_data + sizeof(adv_send_data_head)+1, adv_fifo_in+i, sizeof(adv_rece_rpl_t)))
        {
            return WICED_TRUE;
        }
    }
     
    //copy data
    memcpy(adv_data.adv_array, p_adv_data, ADV_PACK_LEN);
    memcpy(adv_fifo_out[adv_fifo_out_iptr].adv_array, p_adv_data, ADV_PACK_LEN);
    //memcpy(adv_fifo_in[adv_fifo_in_iptr].adv_array, p_adv_data, ADV_PACK_LEN);
    memcpy(adv_fifo_in+adv_fifo_in_iptr, p_adv_data + sizeof(adv_send_data_head)+1, sizeof(adv_rece_rpl_t));

    //check and decode
    //decode
#if AES_ENABLE
    AES_Decrypt(adv_data.item.remote_mac, adv_data.item.remote_mac, 16, aes128key);
#endif
    //校验
    rece_crc = adv_data.item.crc_check[0];
    rece_crc = rece_crc * 256 + adv_data.item.crc_check[1];
    if (rece_crc != CrcCalc(adv_data.adv_array + sizeof(adv_send_data_head)+3, 
                            sizeof(adv_data.adv_array) - (sizeof(adv_send_data_head)+3))){
        return WICED_TRUE;
    }

    // //上电1s内不接收广播，以避免上电时会接收到掉电前最后一次的消息
    // if(SystemUTCtimer < 1)
    // {
    //     // #if(CHIP_SCHEME == CHIP_DEVKIT)
    //     //     wiced_hal_gpio_set_pin_output((uint32_t)*platform_led[WICED_PLATFORM_LED_2].gpio, GPIO_PIN_OUTPUT_LOW);
    //     // #endif
        
    //     return WICED_FALSE;
    // }

    //去混淆
    seed = adv_data.item.seq;
    for (int i = sizeof(adv_send_data_head)+4; i < sizeof(adv_data.adv_array); i++){
        seed = 214013 * seed + 2531011;
        adv_data.adv_array[i] ^= (seed >> 16) & 0xff;
    }
    wiced_bt_dev_read_local_addr(own_bd_addr);
    //receiver own message
    if (0 == memcmp(adv_data.item.remote_mac, own_bd_addr, sizeof(wiced_bt_device_address_t))){
        return WICED_TRUE;
    }
    
    if(WICED_FALSE == appendAdvCmdRpl(adv_data.item.remote_mac,adv_data.item.seq)){
        return WICED_TRUE;
    }

    #if LOGLEVEL >= LOGLEVEL_DEBUG
    WICED_BT_TRACE_ARRAY(adv_data.adv_array,ADV_PACK_LEN,"\r\nRece ADV MANUFACTURER: ");
    #endif

    //adv_data.item.ttl = 0;  //遥控器不转发数据
    if (adv_data.item.ttl > 0){
        //relay
        //memcpy(adv_fifo_out[adv_fifo_out_iptr].adv_array, adv_fifo_in[adv_fifo_in_iptr].adv_array, ADV_PACK_LEN);
        adv_fifo_out[adv_fifo_out_iptr].item.ttl -= 1;
        adv_fifo_out[adv_fifo_out_iptr].item.count = ADV_CMD_RETRYTIMES;
        adv_fifo_out[adv_fifo_out_iptr].item.delay = own_bd_addr[5]%(50/PAIRED_TIMER_LEN);
        adv_fifo_out[adv_fifo_out_iptr].item.sendtime = sysTimerGetSystemRunTime();
        //adv_manuadv_send(adv_fifo_out[adv_fifo_optr].adv_array,ADV_PACK_LEN);
        adv_fifo_out_iptr = adv_fifo_optr_plus(adv_fifo_out_iptr, 1);
        adv_send_Scheduling();
    }

    adv_fifo_in_iptr = adv_fifo_iptr_plus(adv_fifo_in_iptr, 1);
    rece_cmd = adv_data.item.remote_cmd[0];
    rece_cmd = rece_cmd * 256 + adv_data.item.remote_cmd[1];
    for (uint8_t i = 0; i < sizeof(adv_receive_models) / sizeof(adv_receive_model_t); i++)
    {
        if (adv_receive_models[i].cmd == rece_cmd)
        {
            wiced_ble_adv_cmd_cback_t cback;
            cback = adv_receive_models[i].cback;
            if (cback == NULL)
            {
                return WICED_TRUE;
            }
            //非配对指令
            switch(rece_cmd)
            {
                case ADV_CMD_PARI:
                {
                    cback(adv_data.item.remote_mac, rece_cmd, adv_data.item.cmd_load, p_scan_result->rssi);
                    return WICED_TRUE;
                    break;
                }
                case ADV_CMD_TOOLING:
                {
                    cback(p_scan_result, rece_cmd, adv_data.item.cmd_load, sizeof(adv_data.item.cmd_load));
                    return WICED_TRUE;
                    break;
                }
                case ADV_CMD_RESET:
                {
                    cback(adv_data.item.remote_mac, rece_cmd, adv_data.item.cmd_load, sizeof(adv_data.item.cmd_load));
                    return WICED_TRUE;
                    break;
                }
                default:
                {
#if AUTO_PAIRED_ENABLE
                    //出厂后第一次上电 只要接收到遥控器控制指令，直接绑定
                    if((CONFIG_UNPAIR == LightConfig.fristpair) && (adv_pair_params.adv_remotectr_pairsum == 0))
                    {
                        if (0 == memcmp(adv_pair_params.adv_remotectr_mac[0],
                                        "\x00\x00\x00\x00\x00", sizeof(wiced_bt_device_address_t)))
                        {
                            uint16_t len_res, len_write;
                            //出厂后首次使用遥控器操作这个灯，自动绑定
                            adv_pair_params.adv_remotectr_pairindex = 1;
                            adv_pair_params.adv_remotectr_pairsum   = 1;
                            memcpy(adv_pair_params.adv_remotectr_mac[0], adv_data.item.remote_mac, sizeof(wiced_bt_device_address_t));
                            //TODO  将配对的遥控器mac 及pairindex 存到NVRAM中
                            len_write = sizeof(wiced_bt_device_address_t) * REMOTE_MAX_NUM + 2;
                            len_res = flash_write_erase(FLASH_ADDR_CTRREMOTE, 
                                                        (uint8_t *)(adv_pair_params.adv_remotectr_mac), len_write, WICED_TRUE);
                            if(len_write == len_res){
                                LightConfig.fristpair =  CONFIG_PAIRED;
                            }else{
                                WICED_LOG_ERROR("Save SFlash error!!!\r\n");
                            }
                        }
                    }
                    else
                    {
                        LOG_DEBUG("fristpair = %x, adv_remotectr_pairsum= %d\n",LightConfig.fristpair,adv_pair_params.adv_remotectr_pairsum);
                    }
#endif // AUTO_PAIRED_ENABLE
                    for (uint8_t index = 0; index < REMOTE_MAX_NUM; index++){
                        if (0 == memcmp(adv_pair_params.adv_remotectr_mac[index],
                                        adv_data.item.remote_mac, sizeof(wiced_bt_device_address_t))){
                            cback(adv_data.item.remote_mac, rece_cmd, adv_data.item.cmd_load, sizeof(adv_data.item.cmd_load));
                            return WICED_TRUE;
                        }
                    }
                    return WICED_TRUE;
                }
            }
           
        }
    }
    return WICED_TRUE;
}

void adv_vendor_send_cmd(uint16_t user_cmd, uint8_t *pack_load, uint8_t len, uint8_t ttl)
{
    uint16_t length = 0;
    uint32_t seed;
    uint16_t crc_send;
    wiced_bt_device_address_t own_bd_addr;
  
    memcpy(adv_fifo_out[adv_fifo_out_iptr].adv_array, adv_send_data_head, sizeof(adv_send_data_head));
    //adv_fifo_out[adv_fifo_out_iptr].item.seq[0] = (uint8_t)((adv_send_seq>>8)&0xFF);
    adv_fifo_out[adv_fifo_out_iptr].item.seq = (uint8_t)(adv_send_seq & 0xFF);
    adv_fifo_out[adv_fifo_out_iptr].item.segment = 0;
    adv_send_seq = adv_send_seq_plus(adv_send_seq, 1);
    adv_fifo_out[adv_fifo_out_iptr].item.ttl = ttl;

    wiced_bt_dev_read_local_addr(own_bd_addr);
    memcpy(adv_fifo_out[adv_fifo_out_iptr].item.local_mac, own_bd_addr, sizeof(wiced_bt_device_address_t));

    adv_fifo_out[adv_fifo_out_iptr].item.local_cmd[0] = (uint8_t)((user_cmd >> 8) & 0xFF);
    adv_fifo_out[adv_fifo_out_iptr].item.local_cmd[1] = (uint8_t)(user_cmd);
    //TODO 暂不做分包处理
    if (len > sizeof(adv_fifo_out[adv_fifo_out_iptr].item.cmd_load))
        len = sizeof(adv_fifo_out[adv_fifo_out_iptr].item.cmd_load);
    memcpy(adv_fifo_out[adv_fifo_out_iptr].item.cmd_load, pack_load, len);
    mesh_generate_random(adv_fifo_out[adv_fifo_out_iptr].item.cmd_load + len,
                         sizeof(adv_fifo_out[adv_fifo_out_iptr].item.cmd_load) - len);
    //数据混淆处理
    seed = adv_fifo_out[adv_fifo_out_iptr].item.seq;
    for (int i = sizeof(adv_send_data_head)+4; i < (sizeof(adv_fifo_out[adv_fifo_out_iptr].adv_array) - 1); i++)
    {
        seed = 214013 * seed + 2531011;
        adv_fifo_out[adv_fifo_out_iptr].adv_array[i] ^= (seed >> 16) & 0xff;
    }
    //校验
    crc_send = CrcCalc(adv_fifo_out[adv_fifo_out_iptr].adv_array + sizeof(adv_send_data_head)+3, 
                       sizeof(adv_fifo_out[adv_fifo_out_iptr].adv_array) - (sizeof(adv_send_data_head)+4));
    adv_fifo_out[adv_fifo_out_iptr].item.crc_check[0] = (uint8_t)(crc_send >> 8);
    adv_fifo_out[adv_fifo_out_iptr].item.crc_check[1] = (uint8_t)(crc_send);

    //AES加密
#if AES_ENABLE
    AES_Encrypt(adv_fifo_out[adv_fifo_out_iptr].item.local_mac, adv_fifo_out[adv_fifo_out_iptr].item.local_mac, 16, aes128key);
#endif
    adv_fifo_out[adv_fifo_out_iptr].item.count = ADV_CMD_RETRYTIMES;
    adv_fifo_out[adv_fifo_out_iptr].item.delay = 0;
    adv_fifo_out[adv_fifo_out_iptr].item.sendtime = sysTimerGetSystemRunTime();
    adv_fifo_out_iptr = adv_fifo_optr_plus(adv_fifo_out_iptr, 1);
    adv_send_Scheduling();
}

void adv_paircmd_handle(wiced_bt_device_address_t remote_mac, uint16_t cmd, uint8_t *pdata, int8_t rssi)
{
    wiced_bt_device_address_t own_bd_addr;
    uint8_t index = 0;
    uint8_t index1 = 0;
    mesh_btn_pairload_t *p_adv_data;

    p_adv_data = (mesh_btn_pairload_t *)pdata;
    if(cmd != ADV_CMD_PARI) 
    {
        return;
    }
    
    if(ADV_PARILOAD_RESET == p_adv_data->pair_stata)
    {
        extern void wdog_generate_hw_reset(void);
        extern void mesh_app_gatt_is_disconnected(void);
        extern wiced_bool_t clear_flash_for_reset(wiced_bt_mesh_core_config_t *p_config_data,wiced_bt_core_nvram_access_t nvram_access_callback);
        extern uint32_t mesh_nvram_access(wiced_bool_t write, int inx, uint8_t* node_info, uint16_t len, wiced_result_t *p_result);

        //恢复原始出厂设置
        LOG_DEBUG("0................\n");
        //LightFlash(120, 3, 30, 30, 1);
        mesh_app_gatt_is_disconnected();
        LightConfig.fristpair = CONFIG_UNPAIR;
        ResetToolConfig();
        clear_flash_for_reset(&mesh_config,mesh_nvram_access);
        wdog_generate_hw_reset();
        while(1);
    }

    if(adv_pair_params.adv_pair_enable == WICED_FALSE)
    {
        adv_pair_params.adv_pair_wakeup = WICED_FALSE;
        return;
    }

    // adv_pair_params.adv_pair_runcount = 0;
    for (index = 0; index < REMOTE_MAX_NUM; index++)
    {
        if (0 == memcmp(adv_pair_params.adv_remotectr_mac[index], remote_mac, sizeof(wiced_bt_device_address_t)))
        {
            break;
        }
    }

    if (index != REMOTE_MAX_NUM)  //已绑定的遥控器，仅重置绑定超时时间
    {
        if(ADV_PARILOAD_SELECT == p_adv_data->pair_stata)
        {
            //不是自己的mac，停止闪烁
            if (0 == memcmp(p_adv_data->dst_mac, "\x00\x00\x00\x00\x00", sizeof(wiced_bt_device_address_t)))
            {
                // LightFlash(0, 0, 0, 0, 0);
                adv_pair_disable();
                adv_pair_enable();
            }
            else
            {
                adv_pair_params.adv_pair_runcount = 60 * 60 * 1000 / PAIRED_TIMER_LEN;
            }
        }
        return;
    }
    
    // //唤醒指令，开启配网模式  更改为上电使能配对 无需指令唤醒 zhw20201104
    // if((p_adv_data->pair_stata == ADV_PARILOAD_WAKEUP) && (WICED_FALSE == adv_pair_params.adv_pair_enable))
    // {
    //     adv_pair_enable();
    //     return;
    // }

    //唤醒指令，灯亮度调整为30% zhw20201104
    if(p_adv_data->pair_stata == ADV_PARILOAD_WAKEUP)
    {
        LOG_DEBUG("wake up!!!!!!!!!!!!!!!!\n");
        LightFlash(0, 0,30,30,0);
        adv_pair_params.adv_pair_wakeup = WICED_TRUE;
        advSendPairAck = 0;
        return;
    }

    if(WICED_FALSE == adv_pair_params.adv_pair_wakeup)
    {
        advSendPairAck = 0;
        return;
    }

    //获取本地MAC
    wiced_bt_dev_read_local_addr(own_bd_addr);    
   
    //确认绑定指令
    if (p_adv_data->pair_stata == ADV_PARILOAD_PAIRED)
    {
        //不是给本机的
        if (0 != memcmp(p_adv_data->dst_mac, own_bd_addr, sizeof(wiced_bt_device_address_t)))
        {
            return;
        }
        else //本机被选中
        {
            adv_pair_params.adv_pair_select = WICED_TRUE;
            adv_pair_params.adv_pair_wakeup = WICED_FALSE;
            //adv_pair_params.adv_pair_enable = WICED_FALSE;
            currentCfg.lightingOn = 1;
            currentCfg.lightnessLevel = 65535;
            //LightFlash(60, 3, 100, 100,1);
            LightConfig.fristpair =  CONFIG_PAIRED;
            LightSniffer(160,2,1,70,1);
            //TODO 回复遥控器确认响应
            p_adv_data->pair_stata = ADV_PARILOAD_SPAIREDACK;
            memcpy(p_adv_data->dst_mac, remote_mac, sizeof(wiced_bt_device_address_t));
            adv_vendor_send_cmd(ADV_CMD_PARI, (uint8_t *)p_adv_data, sizeof(mesh_btn_pairload_t), 0);
            if (index == REMOTE_MAX_NUM)
            {
                uint16_t len_res, len_write;
                wiced_result_t p_result = 0;
                uint8_t *remote_mac_ptr;
                uint8_t *remotectr_mac_ptr;

                LOG_DEBUG("Adv paired done, remote mac: %B \n", remote_mac);
                
                remote_mac_ptr = (uint8_t *)remote_mac;

                for(index = 0; index < REMOTE_MAX_NUM; index++)
                {
                    remotectr_mac_ptr = (uint8_t *)(adv_pair_params.adv_remotectr_mac[index]);
                    if(0 == memcmp(remotectr_mac_ptr+1,remote_mac_ptr+1,sizeof(wiced_bt_device_address_t)-1))
                    {
                        if(((*remote_mac_ptr)&0x3F) == ((*remotectr_mac_ptr)&0x3F))
                        {
                            break;
                        }
                    }
                }
                if (index == REMOTE_MAX_NUM)
                {
                    index = adv_pair_params.adv_remotectr_pairindex;
                    adv_pair_params.adv_remotectr_pairindex = (adv_pair_params.adv_remotectr_pairindex + 1) % REMOTE_MAX_NUM;
                    adv_pair_params.adv_remotectr_pairsum++;
                    if(adv_pair_params.adv_remotectr_pairsum > REMOTE_MAX_NUM)
                    {
                        adv_pair_params.adv_remotectr_pairsum = REMOTE_MAX_NUM;
                    }
                }
                memcpy(adv_pair_params.adv_remotectr_mac[index], remote_mac, sizeof(wiced_bt_device_address_t));
                //TODO  将配对的遥控器mac 及pairindex 存到NVRAM中
                len_write = sizeof(wiced_bt_device_address_t) * REMOTE_MAX_NUM + 2;
                // len_res = wiced_hal_write_nvram(NVRAM_REMOTEMAC_POSITION, len_write,
                //                                 (uint8_t *)(adv_pair_params.adv_remotectr_mac), &p_result);
                
                len_res = flash_write_erase(FLASH_ADDR_CTRREMOTE, 
                                            (uint8_t *)(adv_pair_params.adv_remotectr_mac), len_write, WICED_TRUE);
                //if ((len_res != len_write) || (p_result != 0))
                if(len_res != len_write)
                {
                    //打印异常信息
                    WICED_LOG_ERROR("Read NVRAM error!!!\r\n");
                }
            }
            return;
        }
    }
    //启动配对指令
    else if (ADV_PARILOAD_DOPAIR == p_adv_data->pair_stata) 
    {
        // 更改为上电使能配对 无需指令唤醒 zhw20201104
        // if(adv_pair_params.adv_pair_enable == WICED_FALSE)
        // {
        //     adv_pair_enable();
        //     return;
        // }
        if(advSendPairAck == 0)
        {
            adv_pair_params.adv_pair_runcount = 60 * 60 * 1000 / PAIRED_TIMER_LEN;
        }
        if(adv_pair_params.adv_pair_select == WICED_TRUE)
        {
            return;
        }
        //前三次直接回复，后面按照MAC，均分到5个时隙，不考虑丢包收不到的情况
        if((advSendPairAck < 3) || ((advSendPairAck%5) == (own_bd_addr[5]%5)))
        {
            LOG_DEBUG("Adv paired start, remote mac %B \n", remote_mac);
            //LightModelSetBrightness(30,0x48,0);
            p_adv_data->pair_stata = ADV_PARILOAD_PAIRACK;
            memcpy(p_adv_data->dst_mac, remote_mac, sizeof(wiced_bt_device_address_t));
            adv_vendor_send_cmd(ADV_CMD_PARI, (uint8_t *)p_adv_data, sizeof(mesh_btn_pairload_t), 0);
        }
        advSendPairAck++;
        return;
    }
    //选灯指令
    else if (ADV_PARILOAD_SELECT == p_adv_data->pair_stata)
    {
        //不是自己的mac，停止闪烁
        if (0 != memcmp(p_adv_data->dst_mac, own_bd_addr, sizeof(wiced_bt_device_address_t)))
        {
            if (0 == memcmp(p_adv_data->dst_mac, "\x00\x00\x00\x00\x00", sizeof(wiced_bt_device_address_t)))
            {
                LightFlash(0, 0, 0, 0, 0);
                adv_pair_disable();
                adv_pair_enable();
                // adv_pair_params.adv_pair_runcount = 15 * 60 * 1000 / PAIRED_TIMER_LEN;
            }
            else
            {
                LightFlash(0, 0, 30, 30, 0);
                adv_pair_params.adv_pair_runcount = 60 * 60 * 1000 / PAIRED_TIMER_LEN;
            }
            return;
        }
        if(WICED_TRUE == adv_pair_params.adv_pair_select)
        {
            return;
        }
        LOG_DEBUG("Adv paired blink, remote mac %B \n", remote_mac);
        LightFlash(60, 0x7FFF,30,0,0);
    }
}
/*
 * adv init
 */
void adv_pack_init(void)
{
    //TODO 读取存储的遥控器MAC
    adv_manufacturer_handler = adv_recevier_handle;
    if(adv_fifo_in == NULL)
    {
        adv_fifo_in = (adv_rece_rpl_t *)wiced_memory_permanent_allocate(sizeof(adv_rece_rpl_t) * ADV_RECE_FIFO_LEN);
    }
    if(adv_fifo_out == NULL)
    {
        adv_fifo_out = (adv_send_data_t *)wiced_memory_permanent_allocate(sizeof(adv_send_data_t) * ADV_SEND_FIFO_LEN);
    }
    if(adv_cmdrpl == NULL)
    {
        adv_cmdrpl = (adv_cmdrpl_t *)wiced_memory_permanent_allocate(sizeof(adv_cmdrpl_t) * ADV_CMDRPL_LEN);
    }
    // LOG_DEBUG("adv_manufacturer_handler = %d\n",adv_manufacturer_handler);
    adv_fifo_in_iptr = 0;
    adv_fifo_out_iptr = 0;
    adv_fifo_out_optr = 0;
    adv_send_enable = WICED_FALSE;
#if AES_ENABLE
    AES_Init(aes128key);
#endif
    adv_pair_params.adv_pair_enable = WICED_FALSE;
    adv_pair_params.adv_pair_select = WICED_FALSE;

    adv_pair_enable();
    // adv_pair_params.adv_pair_runcount = 15 * 60 * 1000 / PAIRED_TIMER_LEN;
    if (wiced_is_timer_in_use(&adv_pair_params.adv_pair_timer))
    {
        wiced_deinit_timer(&adv_pair_params.adv_pair_timer);
    }
    wiced_init_timer(&adv_pair_params.adv_pair_timer, &mesh_pair_timer_callback, 0, WICED_MILLI_SECONDS_PERIODIC_TIMER);
    wiced_start_timer(&adv_pair_params.adv_pair_timer, PAIRED_TIMER_LEN); //3min

    // PrintfRemote();
}

void adv_pair_enable(void)
{
    //未启动配对且未入网
    //if ((WICED_FALSE == adv_pair_params.adv_pair_enable) && (WICED_TRUE != mesh_app_node_is_provisioned()))
    LOG_DEBUG("enable andon pair\n");
    if (WICED_FALSE == adv_pair_params.adv_pair_enable) 
    {
        uint16_t len_res;
        uint16_t len_read;
        wiced_result_t p_result = 0;
        adv_manufacturer_handler = adv_recevier_handle;
        //adv_fifo_in_iptr = 0;
        //adv_fifo_out_iptr = 0;
        //adv_fifo_out_optr = 0;
        //adv_send_enable = WICED_FALSE;
        //memset(adv_fifo_in,0,sizeof(adv_rece_data_t)*ADV_RECE_FIFO_LEN);
        //memset(adv_fifo_out,0,sizeof(adv_send_data_t)*ADV_SEND_FIFO_LEN);
        adv_pair_params.adv_pair_enable = WICED_TRUE;
        // adv_pair_params.adv_pair_runcount = 0;
        adv_pair_params.adv_pair_select = WICED_FALSE;
        //TODO 从NVRAM中读取存储的遥控器MAC 及绑定序列
        len_read = sizeof(wiced_bt_device_address_t) * REMOTE_MAX_NUM + 2;
        memset((uint8_t *)adv_pair_params.adv_remotectr_mac,0,len_read);
        // len_res = wiced_hal_read_nvram(NVRAM_REMOTEMAC_POSITION, len_read,
        //                                (uint8_t *)(adv_pair_params.adv_remotectr_mac), &p_result);
        len_res = flash_app_read_mem(FLASH_ADDR_CTRREMOTE, (uint8_t *)(adv_pair_params.adv_remotectr_mac), len_read);
        if ((len_res != len_read) || (p_result != 0) 
             || (adv_pair_params.adv_remotectr_pairindex == 0xFF) 
             || (adv_pair_params.adv_remotectr_pairsum == 0xFF))
        {
            //打印异常信息
            WICED_LOG_ERROR("Read NVRAM error!!!\r\n");
            // memset(adv_pair_params.adv_remotectr_mac, 0, sizeof(wiced_bt_device_address_t) * REMOTE_MAX_NUM);
            // adv_pair_params.adv_remotectr_pairindex = 0;
            advReSetRemoteCtr();
        }
        LOG_DEBUG("adv_remotectr_pairindex: %d \n", adv_pair_params.adv_remotectr_pairindex);
        for (uint8_t i = 0; i < REMOTE_MAX_NUM; i++)
        {
            #if LOGLEVEL >= LOGLEVEL_VERBOSE
            LOG_VERBOSE("%s %s Remote mac %d: %B\n", __func__,__LINE__, i, adv_pair_params.adv_remotectr_mac[i]);
            #endif
        }
        // if (wiced_is_timer_in_use(&adv_pair_params.adv_pair_timer))
        // {
        //     wiced_deinit_timer(&adv_pair_params.adv_pair_timer);
        // }
        // wiced_init_timer(&adv_pair_params.adv_pair_timer, &mesh_pair_timer_callback, 0, WICED_MILLI_SECONDS_PERIODIC_TIMER);
        // wiced_start_timer(&adv_pair_params.adv_pair_timer, PAIRED_TIMER_LEN); //3min
    }
    // adv_pair_params.adv_pair_runcount = 0;
    advSendPairAck = 0;
    adv_pair_params.adv_pair_runcount = 15 * 60 * 1000 / PAIRED_TIMER_LEN;
}

void adv_pair_disable(void)
{
    LOG_DEBUG("disable andon pair\n");
    adv_pair_params.adv_pair_enable = WICED_FALSE;
    adv_pair_params.adv_pair_select = WICED_FALSE;
    adv_pair_params.adv_pair_wakeup = WICED_FALSE;
    // if(wiced_is_timer_in_use(&adv_pair_params.adv_pair_timer))
    // {
    //     wiced_stop_timer(&adv_pair_params.adv_pair_timer);
    // }
    advSendPairAck = 0;
}

void advReSetRemoteCtr(void)
{
    uint16_t len_res, len_write;
    wiced_result_t p_result = 0;
    
    LOG_VERBOSE("Reset Remote Ctr\n");
    adv_pair_params.adv_remotectr_pairindex = 0;
    adv_pair_params.adv_remotectr_pairsum = 0;
    memset(adv_pair_params.adv_remotectr_mac, 0, sizeof(wiced_bt_device_address_t) * REMOTE_MAX_NUM);
    //TODO 从NVRAM中读取存储的遥控器MAC 及绑定序列
    len_write = sizeof(wiced_bt_device_address_t) * REMOTE_MAX_NUM + 2;
    len_res = flash_app_write_mem(FLASH_ADDR_CTRREMOTE, (uint8_t *)(adv_pair_params.adv_remotectr_mac), len_write);
    // //memset(adv_pair_params.adv_remotectr_mac, 0, len_write);
    // len_res = wiced_hal_write_nvram(NVRAM_REMOTEMAC_POSITION, len_write,
    //                                 (uint8_t *)(adv_pair_params.adv_remotectr_mac), &p_result);
    if ((len_res != len_write) || (p_result != 0))
    {
        LOG_ERROR("Reset Remote error!!!\n");
    }
    else
    {
        LOG_VERBOSE("Reset Remote ok!!!\n");
    }
}

void adv_reset_handle(wiced_bt_device_address_t remote_mac, uint16_t cmd, uint8_t *data, uint8_t len)
{
    uint16_t len_res, len_write;
    uint16_t reset_index = 0;
    wiced_result_t p_result = 0;
    wiced_bt_device_address_t adv_remotectr_mac[REMOTE_MAX_NUM] = {0};
    
    if (cmd != ADV_CMD_RESET)
    {
        return;
    }
    
    memcpy(adv_remotectr_mac,adv_pair_params.adv_remotectr_mac,sizeof(wiced_bt_device_address_t)*REMOTE_MAX_NUM);

    for(reset_index=0; reset_index<REMOTE_MAX_NUM; reset_index++)
    {
        //找到为0的位置还没找到要删除的MAC，则后面无需再找了
        if(0 == memcmp(adv_pair_params.adv_remotectr_mac+reset_index, \
                       "\x00\x00\x00\x00\x00", sizeof(wiced_bt_device_address_t)))
        {
            LOG_DEBUG("Adv reset, remote mac: %B not found\n", remote_mac);
            return;
        }
        if(0 == memcmp(adv_pair_params.adv_remotectr_mac+reset_index, remote_mac, sizeof(wiced_bt_device_address_t)))
        {
            break;
        }
    }
    
    //找完了也没找到需删除的MAC
    if(reset_index == REMOTE_MAX_NUM)
    {
        LOG_DEBUG("Adv reset, remote mac: %B not found\n", remote_mac);
        return;
    }
    LOG_DEBUG( "adv_remotectr_pairsum : %d adv_remotectr_pairindex : %d  \n",
               adv_pair_params.adv_remotectr_pairsum, adv_pair_params.adv_remotectr_pairindex );
    //删除remote_mac
    uint16_t index = 0;
    if(adv_pair_params.adv_remotectr_pairindex < adv_pair_params.adv_remotectr_pairsum)
    {
        for(uint16_t i=0; i<adv_pair_params.adv_remotectr_pairsum; i++)
        {
            if(((adv_pair_params.adv_remotectr_pairindex+i)%REMOTE_MAX_NUM) == reset_index)
            {
                index = 1;
                continue;
            }
            LOG_DEBUG("i : %d\n", i);
            memcpy(adv_pair_params.adv_remotectr_mac[i-index],
                   adv_remotectr_mac[(adv_pair_params.adv_remotectr_pairindex+i)%REMOTE_MAX_NUM],
                   sizeof(wiced_bt_device_address_t));
        }
        adv_pair_params.adv_remotectr_pairindex = adv_pair_params.adv_remotectr_pairsum-1;
        memcpy(adv_pair_params.adv_remotectr_mac[adv_pair_params.adv_remotectr_pairindex],
               "\x00\x00\x00\x00\x00", sizeof(wiced_bt_device_address_t));
    }
    else
    {
        for(uint16_t i=0; i<adv_pair_params.adv_remotectr_pairsum; i++)
        {
            //if(((adv_pair_params.adv_remotectr_pairindex+i)%REMOTE_MAX_NUM) == reset_index)
            if(i == reset_index)
            {
                index = 1;
                continue;
            }
            LOG_DEBUG("i : %d\n", i);
            memcpy(adv_pair_params.adv_remotectr_mac[i-index],
                   adv_remotectr_mac[i],
                   sizeof(wiced_bt_device_address_t));
        }
        adv_pair_params.adv_remotectr_pairindex -= 1;
        memcpy(adv_pair_params.adv_remotectr_mac[adv_pair_params.adv_remotectr_pairindex],
               "\x00\x00\x00\x00\x00", sizeof(wiced_bt_device_address_t));
    }
    adv_pair_params.adv_remotectr_pairsum -= 1;
    LOG_DEBUG("Adv reset, reset_index %d, remote mac: %B \n", reset_index, remote_mac);
    LOG_DEBUG( "adv_remotectr_pairsum : %d adv_remotectr_pairindex : %d  \n",
               adv_pair_params.adv_remotectr_pairsum, adv_pair_params.adv_remotectr_pairindex );
    //adv_pair_params.adv_remotectr_pairindex = 0;
    len_write = sizeof(wiced_bt_device_address_t) * REMOTE_MAX_NUM + 2;
    //memset(adv_pair_params.adv_remotectr_mac, 0, len_write);
    // len_res = wiced_hal_write_nvram(NVRAM_REMOTEMAC_POSITION, len_write,
    //                                 (uint8_t *)(adv_pair_params.adv_remotectr_mac), &p_result);
    len_res = flash_app_write_mem(FLASH_ADDR_CTRREMOTE, (uint8_t *)(adv_pair_params.adv_remotectr_mac), len_write);
    if ((len_res != len_write) || (p_result != 0))
    {
        // WICED_LOG_ERROR("Read NVRAM error!!!\r\n");
    }
    adv_pair_disable();
    adv_pair_enable();
}
// {
// void adv_printf_message(wiced_bt_device_address_t remote_mac, uint16_t cmd, uint8_t *data, uint8_t len)
//     //绑定的MAC不一致，不做处理
//     //if(0 != memcmp(adv_pair_params.adv_remotectr_mac,remote_mac,sizeof(wiced_bt_device_address_t)))
//     //{
//     //    return;
//     // }
//     WICED_BT_TRACE("%s %s Adv rece Cmd 0x%04X  Form %B   \r\n", __func__,__LINE__, cmd, remote_mac);
//     for (uint8_t i = 0; i < len; i++)
//     {
//         if ((i % 16 == 0) && (i != 0))
//         {
//             WICED_BT_TRACE("\n");
//         }
//         WICED_BT_TRACE("0x%02X ", data[i]);
//     }
//     WICED_BT_TRACE("\n");
// }

wiced_bool_t advpackispairing(void)
{
    if(adv_pair_params.adv_pair_wakeup == WICED_TRUE){
        return WICED_TRUE;
    }else{
        return WICED_FALSE;
    }
}


void advRestartPair(void)
{
    if(adv_pair_params.adv_pair_enable == WICED_FALSE){
        return;
    }
    if(adv_pair_params.adv_pair_wakeup == WICED_TRUE){
        adv_pair_disable();
    }
}

uint16_t adv_packReadRemoteMac(uint8_t *data)
{
    uint16_t len = 0;

    for(uint16_t i=0; i<REMOTE_MAX_NUM; i++)
    {
        //找到为0的位置，则后面无需再找了
        if(0 == memcmp(adv_pair_params.adv_remotectr_mac+i, \
                       "\x00\x00\x00\x00\x00", sizeof(wiced_bt_device_address_t)))
        {
            return len;
        }
        else
        {
            memcpy(data+len,adv_pair_params.adv_remotectr_mac+i,sizeof(wiced_bt_device_address_t));
            len += sizeof(wiced_bt_device_address_t);
        }
    }
    return len;
}

void PrintfRemote(void)
{
#if _ANDONDEBUG  
#if LOGLEVEL >= LOGLEVEL_DEBUG
    for(uint16_t i=0; i<REMOTE_MAX_NUM; i++)
    {
        LOG_DEBUG("remote mac %d: %B \n", i,adv_pair_params.adv_remotectr_mac[i]);
    }
#endif
#endif
}

