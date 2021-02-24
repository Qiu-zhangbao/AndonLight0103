/*
* Copyright 2018, Cypress Semiconductor Corporation or a subsidiary of Cypress Semiconductor 
 *  Corporation. All rights reserved. This software, including source code, documentation and  related 
 * materials ("Software"), is owned by Cypress Semiconductor  Corporation or one of its 
 *  subsidiaries ("Cypress") and is protected by and subject to worldwide patent protection  
 * (United States and foreign), United States copyright laws and international treaty provisions. 
 * Therefore, you may use this Software only as provided in the license agreement accompanying the 
 * software package from which you obtained this Software ("EULA"). If no EULA applies, Cypress 
 * hereby grants you a personal, nonexclusive, non-transferable license to  copy, modify, and 
 * compile the Software source code solely for use in connection with Cypress's  integrated circuit 
 * products. Any reproduction, modification, translation, compilation,  or representation of this 
 * Software except as specified above is prohibited without the express written permission of 
 * Cypress. Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO  WARRANTY OF ANY KIND, EXPRESS 
 * OR IMPLIED, INCLUDING,  BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY 
 * AND FITNESS FOR A PARTICULAR PURPOSE. Cypress reserves the right to make changes to 
 * the Software without notice. Cypress does not assume any liability arising out of the application 
 * or use of the Software or any product or circuit  described in the Software. Cypress does 
 * not authorize its products for use in any products where a malfunction or failure of the 
 * Cypress product may reasonably be expected to result  in significant property damage, injury 
 * or death ("High Risk Product"). By including Cypress's product in a High Risk Product, the 
 *  manufacturer of such system or application assumes  all risk of such use and in doing so agrees 
 * to indemnify Cypress against all liability.
*/

/** @file
*
* mesh definitions
*
*/


#ifndef __MESH_APPLICATION_H__
#define __MESH_APPLICATION_H__

// Definitions for handles used in the GATT database
enum
{
    MESH_HANDLE_GATT_SERVICE = 0x01,
        MESH_HANDLE_GATT_SERVICE_CHARACTERISTIC_SERVICE_CHANGED,
        MESH_HANDLE_GATT_SERVICE_CHARACTERISTIC_SERVICE_CHANGED_VAL,
            MESH_HANDLE_DESCR_SERVICE_CHANGED,

    MESH_HANDLE_GAP_SERVICE = 0x14,
        MESH_HANDLE_GAP_SERVICE_CHARACTERISTIC_DEV_NAME,
        MESH_HANDLE_GAP_SERVICE_CHARACTERISTIC_DEV_NAME_VAL,

        MESH_HANDLE_GAP_SERVICE_CHARACTERISTIC_APPEARANCE,
        MESH_HANDLE_GAP_SERVICE_CHARACTERISTIC_APPEARANCE_VAL,

    HANDLE_MESH_SERVICE_PROVISIONING = 0x28,
        HANDLE_CHAR_MESH_PROVISIONING_DATA_IN,
        HANDLE_CHAR_MESH_PROVISIONING_DATA_IN_VALUE,
        HANDLE_CHAR_MESH_PROVISIONING_DATA_OUT,
        HANDLE_CHAR_MESH_PROVISIONING_DATA_OUT_VALUE,
        HANDLE_DESCR_MESH_PROVISIONING_DATA_CLIENT_CONFIG,

    HANDLE_MESH_SERVICE_PROXY = 0x30,
        HANDLE_CHAR_MESH_PROXY_DATA_IN,
        HANDLE_CHAR_MESH_PROXY_DATA_IN_VALUE,
        HANDLE_CHAR_MESH_PROXY_DATA_OUT,
        HANDLE_CHAR_MESH_PROXY_DATA_OUT_VALUE,
        HANDLE_DESCR_MESH_PROXY_DATA_CLIENT_CONFIG,

    MESH_HANDLE_DEV_INFO_SERVICE = 0x40,
        MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_MFR_NAME,
        MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_MFR_NAME_VAL,

        MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_MODEL_NUM,
        MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_MODEL_NUM_VAL,

        MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_SERIAL_NUMBER,
        MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_SERIAL_NUMBER_VAL,

        MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_FW_VER,
        MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_FW_VER_VAL,

        MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_HW_VER,
        MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_HW_VER_VAL,

#ifdef _DEB_COMMAND_SERVICE
    HANDLE_MESH_SERVICE_COMMAND = 0x50,
        HANDLE_CHAR_MESH_COMMAND_DATA,
        HANDLE_CHAR_MESH_COMMAND_DATA_VALUE,
        HANDLE_DESCR_MESH_COMMAND_DATA_CLIENT_CONFIG,
#endif
};


#ifdef __cplusplus
extern "C"
{
#endif

#define MESH_APP_TIMEOUT_IN_SECONDS   1

#ifdef CYW20706A2
#define NVRAM_ID_NODE_INFO          WICED_NVRAM_VSID_START
#elif ( defined(CYW20719B1) || defined(CYW20719B0) || defined(CYW20721B1) || defined (CYW20819A1) || defined(CYW20719B2) || defined(CYW20721B2))
#define NVRAM_ID_NODE_INFO          WICED_NVRAM_VSID_START
#else
#define NVRAM_ID_NODE_INFO  (WICED_NVRAM_VSID_START + 0x100)  // ID of the memory block used for NVRAM access (+0x100 - workaround to get rid of lost NVM IDs)
#endif

#define NVRAM_ID_PAIRED_KEYS        (NVRAM_ID_NODE_INFO + 0)
#define NVRAM_ID_LOCAL_KEYS         (NVRAM_ID_NODE_INFO + 1)
#define NVRAM_ID_LOCAL_UUID         (NVRAM_ID_NODE_INFO + 2)
#define NVRAM_ID_LOCAL_IV           (NVRAM_ID_NODE_INFO + 3)
#define NVRAM_ID_LOCAL_PROVISION    (NVRAM_ID_NODE_INFO + 4)
#define NVRAM_ID_LOCAL_PROVISIONER  (NVRAM_ID_NODE_INFO + 5)
// the first usable by application NVRAM Identifier
#define NVRAM_ID_APP_START          (NVRAM_ID_NODE_INFO + 6)


#define MESH_APP_PROVISION_STATUS_IDLE           0
#define MESH_APP_PROVISION_STATUS_PROVISIONING   1
#define MESH_APP_PROVISION_STATUS_PROVISIONED    2
#define MESH_APP_PROVISION_STATUS_CONFIGURED     3
#define MESH_APP_PROVISION_STATUS_PRORESTART     4

#ifdef _DEB_COMMAND_SERVICE
// UUIDs for proprietary COMMAND service and characteristic
#define WICED_BT_MESH_CORE_UUID_SERVICE_COMMAND                 0x7FD3
#define WICED_BT_MESH_CORE_UUID_CHARACTERISTIC_COMMAND_DATA     0x7FCB
#endif

// device UUID length
#define MESH_DEVICE_UUID_LEN                      16

// mesh device ID (address) length
#define WICED_BT_MESH_ADDR_LEN                    2

#ifdef _DEB_COMMAND_SERVICE
#define MESH_COMMAND_START_SCAN_UNPROVISIONED     1       // <cmd=1>(1 byte). Start sending UUIDs of detected unprovisioned devices
#define MESH_COMMAND_STOP_SCAN_UNPROVISIONED      2       // <cmd=2>(1 byte). Stops sending UUIDs of detected unprovisioned devices
#define MESH_COMMAND_PROVISION                    3       // <cmd=3>(1 byte), <uuid>(16bytes), <addr>(2bytes). Starts provisionning of device with uuid and assigns addrt to it
#define MESH_COMMAND_RESET_NODE                   4       // <cmd=4>(1 byte). Resets node and makes it unprovisioned

#define MESH_COMMAND_EVENT_UNPROVISIONED_DEV      1       // <event=1>(1 byte), <uuid>(16bytes). Detected unprovisioned device
#define MESH_COMMAND_EVENT_PB_END                 2       // <event=2>(1 byte), <conn_id>(4bytes), <reason>(1byte). End of provisioning
#endif

#define BE2TOUINT16(p) ((uint16_t)(p)[1] + (((uint16_t)(p)[0])<<8))
#define UINT16TOBE2(p, ui) (p)[0]=(uint8_t)((ui)>>8); (p)[1]=(uint8_t)(ui)

uint8_t mesh_app_gatt_mtu(void);

wiced_bt_gatt_status_t mesh_ota_firmware_upgrade_send_data_callback(wiced_bool_t is_notification, uint16_t conn_id, uint16_t attr_handle, uint16_t val_len, uint8_t *p_val);
void mesh_app_gatt_init(void);
void mesh_app_gatt_db_init(wiced_bool_t is_authenticated);
void mesh_app_gatt_send_service_changed_indication(void);
void mesh_app_proxy_gatt_send_cb(uint32_t conn_id, uint32_t ref_data, const uint8_t *packet, uint32_t packet_len);
wiced_bool_t mesh_app_gatt_is_connected(void);
wiced_bool_t mesh_app_node_is_provisioned(void);
void mesh_app_hci_init(void);
void mesh_app_timer_init(void);
void mesh_app_hci_send_seq_changed(wiced_bt_mesh_core_state_seq_t *p_seq);
void mesh_app_dfu_init(void);
void mesh_application_init(void);
#ifdef WICEDX_LINUX
void mesh_application_deinit(void);
#endif
uint16_t mesh_application_mcu_memory_read(uint16_t id, uint16_t buf_len, uint8_t *p_buf);
uint16_t mesh_application_mcu_memory_write(uint16_t id, uint16_t data_len, const uint8_t *p_data);
wiced_result_t mesh_application_process_hci_cmd(uint16_t opcode, const uint8_t *p_data, uint16_t data_len);
wiced_result_t mesh_application_send_hci_event(uint16_t opcode, const uint8_t *p_data, uint16_t data_len);

#if defined HCI_CONTROL
void mesh_app_set_raw_access_layer_mode_handler(wiced_bt_mesh_core_received_msg_handler_t msg_handler);
void mesh_app_set_raw_access_layer_clnt_devkey(uint16_t addr, uint16_t netkey_idx, uint8_t* devkey);
void mesh_app_send_raw_access_layer_pdu(uint8_t flags, uint8_t ttl, uint8_t element_idx, uint16_t addr, uint16_t app_key_idx,
    uint16_t company_id, uint16_t model_id, uint16_t opcode, uint8_t retrans_cnt, uint8_t retrans_interval, uint8_t* params, uint32_t params_len);
void mesh_app_cancel_raw_access_layer_pdu();
#endif

#endif // __MESH_APPLICATION_H__
