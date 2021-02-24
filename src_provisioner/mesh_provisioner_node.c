/*
 * Copyright 2017, Cypress Semiconductor Corporation or a subsidiary of Cypress Semiconductor
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
 * This file shows how to create a device which implements mesh provisioner client.
 * The main purpose of the app is to process messages coming from the MCU and call Mesh Core
 * Library to perform functionality.
 */
#include "wiced_bt_ble.h"
#include "wiced_bt_cfg.h"
#include "wiced_bt_gatt.h"
#include "wiced_bt_trace.h"
#include "wiced_timer.h"
#include "wiced_memory.h"
#include "wiced_platform.h"
#include "spar_utils.h"

#include "wiced_bt_mesh_app.h"
#include "wiced_bt_mesh_client.h"
#include "wiced_bt_mesh_core.h"
#include "wiced_bt_mesh_core_extra.h"
#include "wiced_bt_mesh_dfu.h"
#include "wiced_bt_mesh_event.h"
#include "wiced_bt_mesh_lpn.h"
#include "wiced_bt_mesh_models.h"
#include "wiced_bt_mesh_model_defs.h"
#include "wiced_bt_mesh_self_config.h"
#include "mesh_provisioner_node.h"
#ifdef MESH_DFU_SUPPORTED
#include "wiced_bt_mesh_dfu.h"
#endif

// #include "wiced_bt_mesh_vendor_app.h"
#include "src_light/config.h"
#include "src_light/storage.h"
#include "src_light/adv_pack.h"
#include "src_light/light_model.h"
#include "src_light/vendor.h"


#define USE_LED_AND_BUTTON   // Use board specific LED and buttons

#ifdef USE_LED_AND_BUTTON
#include "wiced_bt_btn_handler.h"
#endif


/******************************************************
 *          Constants
 ******************************************************/
//#define MESH_NAME               "Cypress Mesh"

// #define MESH_PID                0x301D
// #define MESH_VID                0x0001
// #define MESH_CACHE_REPLAY_SIZE  200

/******************************************************
 *          Structures
 ******************************************************/


/******************************************************
 *          Function Prototypes
 ******************************************************/
// extern const char  *wiced_bt_mesh_vendor_get_default_group_name(void);
// extern uint16_t     wiced_bt_mesh_vendor_get_default_group_addr(void);

static void mesh_app_hardware_init(void);
static void mesh_app_init(wiced_bool_t is_provisioned);




extern wiced_bool_t mesh_vendor_message_handler(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint16_t data_len);
static wiced_bool_t mesh_provisioner_message_handler(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint16_t data_len);

static void mesh_provisioner_frnd_friendship(wiced_bool_t established, uint16_t lpn_node_id);

static void mesh_provisioner_btn_event_handler(int btn_down_time);

static wiced_bool_t mesh_provisioner_check_device_identity(const uint8_t *p_uuid);
void mesh_provisioner_client_status2(uint8_t status, uint16_t node_id, const uint8_t *p_uuid);
void mesh_provisioner_client_status1(uint8_t status, uint16_t node_id, const uint8_t *p_uuid);
void provisioner_stop_scan(void);

/******************************************************
 *          Variables Definitions
 ******************************************************/
extern wiced_bt_cfg_settings_t wiced_bt_cfg_settings;
extern uint8_t node_uuid[MESH_DEVICE_UUID_LEN];

uint8_t mesh_mfr_name[WICED_BT_MESH_PROPERTY_LEN_DEVICE_MANUFACTURER_NAME] = MANUFACTURER_CODE;
uint8_t mesh_model_num[WICED_BT_MESH_PROPERTY_LEN_DEVICE_MODEL_NUMBER]     = PRODUCT_CODE;
uint8_t mesh_system_id[WICED_BT_MESH_PROPERTY_LEN_DEVICE_SERIAL_NUMBER]    = { 0 };
uint8_t mesh_fW_ver[WICED_BT_MESH_PROPERTY_LEN_DEVICE_FIRMWARE_REVISION]   = { 0 };
uint8_t mesh_hW_ver[WICED_BT_MESH_PROPERTY_LEN_DEVICE_HARDWARE_REVISION]   = { 0 };


extern wiced_bool_t mesh_andon_vendor_message_handler(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint16_t data_len);

wiced_bt_mesh_core_config_model_t mesh_element1_models[] =
{
    WICED_BT_MESH_DEVICE,
    WICED_BT_MESH_MODEL_CONFIG_CLIENT,

#ifdef REMOTE_PROVISION_SERVER_SUPPORTED
    WICED_BT_MESH_MODEL_REMOTE_PROVISION_SERVER,
    WICED_BT_MESH_MODEL_REMOTE_PROVISION_CLIENT,
#endif

#ifdef MESH_DFU_SUPPORTED
    WICED_BT_MESH_MODEL_FW_DISTRIBUTOR_UPDATE_SERVER,
    WICED_BT_MESH_MODEL_FW_DISTRIBUTION_CLIENT,
#endif

#ifdef ENABLE_SELF_CONFIG_MODEL
    { MESH_COMPANY_ID_CYPRESS, WICED_BT_MESH_MODEL_ID_SELF_CONFIG, wiced_bt_mesh_self_config_message_handler, NULL, NULL},
#endif

    { MESH_VENDOR_COMPANY_ID, MESH_VENDOR_MODEL_ID, mesh_andon_vendor_message_handler, NULL, NULL},
};

#define MESH_APP_NUM_MODELS  (sizeof(mesh_element1_models) / sizeof(wiced_bt_mesh_core_config_model_t))

#define MESH_PROVISIONER_ELEMENT_INDEX   0

wiced_bt_mesh_core_config_element_t mesh_elements[] =
{
    {
        .location = MESH_ELEM_LOC_MAIN,                                 // location description as defined in the GATT Bluetooth Namespace Descriptors section of the Bluetooth SIG Assigned Numbers
        .default_transition_time = MESH_DEFAULT_TRANSITION_TIME_IN_MS,  // Default transition time for models of the element in milliseconds
        .onpowerup_state = WICED_BT_MESH_ON_POWER_UP_STATE_RESTORE,     // Default element behavior on power up
        .default_level = 0,                                             // Default value of the variable controlled on this element (for example power, lightness, temperature, hue...)
        .range_min = 1,                                                 // Minimum value of the variable controlled on this element (for example power, lightness, temperature, hue...)
        .range_max = 0xffff,                                            // Maximum value of the variable controlled on this element (for example power, lightness, temperature, hue...)
        .move_rollover = 0,                                             // If true when level gets to range_max during move operation, it switches to min, otherwise move stops.
        .properties_num = 0,                                            // Number of properties in the array models
        .properties = NULL,                                             // Array of properties in the element.
        .sensors_num = 0,                                               // Number of sensors in the sensor array
        .sensors = NULL,                                                // Array of sensors of that element
        .models_num = MESH_APP_NUM_MODELS,                              // Number of models in the array models
        .models = mesh_element1_models,                                 // Array of models located in that element. Model data is defined by structure wiced_bt_mesh_core_config_model_t
    },
};

wiced_bt_mesh_core_config_t  mesh_config =
{
    .company_id         = MESH_VENDOR_COMPANY_ID,                  // Company identifier assigned by the Bluetooth SIG
    .product_id         = MESH_PID,                                 // Vendor-assigned product identifier
    .vendor_id          = MESH_VID,                                 // Vendor-assigned product version identifier
    .replay_cache_size  = MESH_CACHE_REPLAY_SIZE,                   // Number of replay protection entries, i.e. maximum number of mesh devices that can send application messages to this device.
    .features           = WICED_BT_MESH_CORE_FEATURE_BIT_FRIEND | WICED_BT_MESH_CORE_FEATURE_BIT_RELAY | WICED_BT_MESH_CORE_FEATURE_BIT_GATT_PROXY_SERVER,  // support friend, relay, proxy
    .friend_cfg         =                                           // Configuration of the Friend Feature(Receive Window in Ms, messages cache)
    {
        .receive_window        = 20,
        .cache_buf_len         = 300,                               // Length of the buffer for the cache
        .max_lpn_num           = 4                                  // Max number of Low Power Nodes with established friendship. Must be > 0 if Friend feature is supported. 
    },
    .low_power          =                                           // Configuration of the Low Power Feature
    {
        .rssi_factor           = 0,                                 // contribution of the RSSI measured by the Friend node used in Friend Offer Delay calculations.
        .receive_window_factor = 0,                                 // contribution of the supported Receive Window used in Friend Offer Delay calculations.
        .min_cache_size_log    = 0,                                 // minimum number of messages that the Friend node can store in its Friend Cache.
        .receive_delay         = 0,                                 // Receive delay in 1 ms units to be requested by the Low Power node.
        .poll_timeout          = 0                                  // Poll timeout in 100ms units to be requested by the Low Power node.
    },
    .gatt_client_only          = WICED_FALSE,                       // Can connect to mesh over GATT or ADV
    .elements_num  = (uint8_t)(sizeof(mesh_elements) / sizeof(mesh_elements[0])),   // number of elements on this device
    .elements      = mesh_elements                                  // Array of elements for this device
};

/*
 * Mesh application library will call into application functions if provided by the application.
 */
wiced_bt_mesh_app_func_table_t wiced_bt_mesh_app_func_table =
{
    mesh_app_init,          // application initialization
    mesh_app_hardware_init, // hardware initialization
    NULL,                   // GATT connection status
    NULL,                   // attention processing
    NULL,                   // notify period set
    NULL,                   // WICED HCI command
    NULL,                   // lpn sleep
    NULL                    // factory reset
};

char   *mesh_dev_name                                           = "Node";
// uint8_t mesh_appearance[WICED_BT_MESH_PROPERTY_LEN_DEVICE_APPEARANCE] = { BIT16_TO_8(APPEARANCE_GENERIC_TAG) };

static const char *MESH_NAME = "Cypress Mesh";
static  uint16_t  groupaddr = 0;

static uint16_t     local_addr     = 0;
wiced_bool_t is_provisioner = WICED_FALSE;
static wiced_bool_t network_opened = WICED_FALSE;
uint16_t ProvisionedNum = 0;

static void   (*mesh_provisioner_client_status)(uint8_t status, uint16_t node_id, const uint8_t *p_uuid);
extern void (*mesh_provisioner_timer_cb)(void);

uint16_t wiced_bt_core_lower_transport_seg_trans_timeout_ms;

/******************************************************
 *               Function Definitions
 ******************************************************/
static void mesh_app_hardware_init(void)
{
    // The global variables below are more harwdare related. However, they need
    // be set properly before mesh_core_init() is called, hence we do it here.
    // For provisioner node, we only support 1 net_key and one app_key
    wiced_bt_mesh_core_net_key_max_num = 1;
    wiced_bt_mesh_core_app_key_max_num = 1;
    wiced_bt_mesh_scene_max_num        = 2;

    // Set mesh application related callback functions
    mesh_app_post_init = NULL;
    is_provisioner = WICED_FALSE;
    // mesh_app_post_init = NULL;
}



void provisionerSetDevName(void)
{
    extern uint8_t* appGetDevName(void);

    uint8_t *dev_name;
    wiced_bt_ble_advert_elem_t adv_elem[3];
    uint8_t buf[2];
    uint8_t num_elem = 0;

    // dev_name = appGetDevName();
    dev_name = wiced_bt_cfg_settings.device_name;
    adv_elem[num_elem].advert_type = BTM_BLE_ADVERT_TYPE_NAME_COMPLETE;
    //adv_elem[num_elem].len = (uint16_t)strlen((const char *)mywiced_bt_cfg_settings.device_name);
    //adv_elem[num_elem].p_data = mywiced_bt_cfg_settings.device_name;
    adv_elem[num_elem].len = (uint16_t)strlen((const char *)dev_name);
    adv_elem[num_elem].p_data = dev_name;
            
    num_elem++;

    adv_elem[num_elem].advert_type = BTM_BLE_ADVERT_TYPE_APPEARANCE;
    adv_elem[num_elem].len = 2;
    buf[0] = (uint8_t)wiced_bt_cfg_settings.gatt_cfg.appearance;
    buf[1] = (uint8_t)(wiced_bt_cfg_settings.gatt_cfg.appearance >> 8);
    adv_elem[num_elem].p_data = buf;
    num_elem++;

    LOG_VERBOSE("Start BLE Advertising\n");
    LOG_INFO("Local dev name:%s\n",dev_name);
    wiced_bt_mesh_set_raw_scan_response_data(num_elem, adv_elem);

    wiced_bt_cfg_settings.device_name = dev_name;
    // provissioning service advertising interval set to 50ms
    //wiced_bt_mesh_core_provisioning_srv_adv_interval = 320;
}

static void mesh_app_init(wiced_bool_t is_provisioned)
{
    // wiced_bt_device_address_t bd_addr;
    extern void mesh_node_app_init(wiced_bool_t is_provisioned);

    const char *str = "app_init: ";

    static wiced_bool_t frist_run = WICED_FALSE;
    

    // Temporarily walk-around for a bug in mesh_core library
     wiced_bt_core_lower_transport_seg_trans_timeout_ms = 1300;

#if 0
    // Set Debug trace level for mesh_models_lib and mesh_provisioner_lib
    wiced_bt_mesh_models_set_trace_level(WICED_BT_MESH_CORE_TRACE_INFO);
#endif
#if 0
    // Set Debug trace level for all modules but Info level for CORE_AES_CCM module
    wiced_bt_mesh_core_set_trace_level(WICED_BT_MESH_CORE_TRACE_FID_ALL, WICED_BT_MESH_CORE_TRACE_DEBUG);
    wiced_bt_mesh_core_set_trace_level(WICED_BT_MESH_CORE_TRACE_FID_CORE_AES_CCM, WICED_BT_MESH_CORE_TRACE_INFO);
#endif

    //is_provisioner = wiced_bt_mesh_app_is_provisioner();
    mesh_provisioner_timer_cb = NULL;
    WICED_BT_TRACE("%sprov'ed:%d \n", str, is_provisioned);
    
    
    // wiced_bt_cfg_settings.device_name = (uint8_t*)mesh_dev_name;
    // wiced_bt_cfg_settings.gatt_cfg.appearance = APPEARANCE_GENERIC_TAG;
    
    // Adv Data is fixed. Spec allows to put URI, Name, Appearance and Tx Power in the Scan Response Data.
    if (is_provisioned)
    {
        local_addr = wiced_bt_mesh_core_get_local_addr();

    }
    provisionerSetDevName();
    
#if REMOTE_PROVISION_SERVER_SUPPORTED
    wiced_bt_mesh_remote_provisioning_server_init();
#endif

#ifdef MESH_DFU_SUPPORTED
    wiced_bt_mesh_model_fw_update_server_init("");
    wiced_bt_mesh_model_fw_provider_init();
    wiced_bt_mesh_model_fw_distribution_server_init();
    wiced_bt_mesh_model_blob_transfer_server_init(WICED_BT_MESH_FW_TRANSFER_MODE_PUSH);
#ifdef MESH_DFU_OOB_SUPPORTED
    mesh_app_dfu_oob_init();
#endif
#endif

    // wiced_bt_mesh_provision_client_init(mesh_client_message_handler, is_provisioned);
    // wiced_bt_mesh_client_init(mesh_client_message_handler, is_provisioned);
    
    // wiced_bt_mesh_config_client_init(mesh_client_message_handler, is_provisioned);

    WICED_BT_TRACE("%sPID:%04X, VID:%04X\n", str, MESH_PID, MESH_VID);
    
    mesh_app_frnd_friendship_cb = mesh_provisioner_frnd_friendship;
    
    if(!frist_run)
    {
        mesh_node_app_init(is_provisioned);
    }
    frist_run = WICED_TRUE;
    // wiced_bt_mesh_app_add_app_timer_cb(set_scan_response_timer);
}







static void mesh_provisioner_frnd_friendship(wiced_bool_t established, uint16_t lpn_node_id)
{
    WICED_BT_TRACE("Friendship: established:%d, lpn id:%04X\n", established, lpn_node_id);
}



#ifdef USE_VENDOR_MODEL

// UUID magic bytes to identify whether the device is vendor's
uint8_t node_uuid_magic_bytes[4] = {0x00, 0x00, 0x00, 0x00};

void rand128(uint8_t *p_array)
{
    extern uint32_t wiced_hal_rand_gen_num(void);
    int i;
    uint32_t *p_32 = (uint32_t *)p_array;
    for (i = 0; i < 4; i++)
        p_32[i] = wiced_hal_rand_gen_num();
}



void wiced_bt_mesh_vendor_gen_device_uuid(uint8_t *uuid)
{
    rand128(uuid);
    uuid[6] = node_uuid_magic_bytes[0];
    uuid[7] = node_uuid_magic_bytes[1];
    uuid[8] = node_uuid_magic_bytes[2];
    uuid[9] = node_uuid_magic_bytes[3];
    // uint8_t mac_string[13] = {0};
    // {
    //     uint8_t *pdata;
    //     pdata = (uint8_t *)wiced_bt_get_buffer(FLASH_XXTEA_SN_LEN+1);
    //     memset(pdata,0,FLASH_XXTEA_SN_LEN+1);
    //     if( FLASH_XXTEA_SN_LEN == storage_read_sn(pdata))
    //     {
    //         memcpy(mac_string,pdata+strlen(PRODUCT_CODE)+1,12);
    //     }
    //     else
    //     {
    //         memset(mac_string,0,12);
    //     }
    //     LOG_DEBUG("MAC: %s  %s\n",pdata,mac_string);
    //     wiced_bt_free_buffer(pdata);
    //     for(uint8_t i=0;i<12;i+=2)
    //     {
    //         uuid[i/2+10] = uuid[i/2] = HexStr2Int8U(mac_string+i);
    //     }
    // }
    uuid[6] = (MESH_PID>>8)&0xFF;
    uuid[7] = MESH_PID&0xFF;
    uuid[8] = (MESH_PID>>8)&0xFF;
    uuid[9] = MESH_PID&0xFF;
}

// uint16_t wiced_bt_mesh_vendor_get_group(wiced_bt_mesh_vendor_group_t group)
// {
//     static const uint16_t groups[] = { 0xC000, 0xC001 };
//     if (group < MESH_VENDOR_GROUP_NUM)
//     {
//         return groups[group];
//     }
//     return 0;
// }
// Default groups
static wiced_bt_mesh_vendor_group_t vendor_groups[] =
{
    { 0xC040, "ALL" },
    { 0xC041, "PROVISIONER" },
    { 0xC042, "DFU" }
};

wiced_bt_mesh_vendor_group_t *wiced_bt_mesh_vendor_get_group(uint8_t group_index)
{
    if (group_index < MESH_VENDOR_GROUP_NUM)
    {
        return &vendor_groups[group_index];
    }
    return NULL;
}


wiced_bool_t myself_is_provisioner(void)
{
    return is_provisioner;
}

#endif


wiced_bool_t wiced_bt_fw_is_ota_supported(void)
{
    return WICED_FALSE;
}

void wiced_bt_fw_start_ota(void)
{
    
}

