/*
* $ Copyright 2016-YEAR Cypress Semiconductor $
*/

/** @file
 *
 * Mesh GATT related functionality
 *
 */
// Comment out next line to disable proprietary command GATT service
//#define _DEB_COMMAND_SERVICE

#include "bt_types.h"
#include "wiced_bt_gatt.h"
#include "wiced_bt_ble.h"
#include "wiced_bt_cfg.h"
#include "wiced_bt_l2c.h"
#include "wiced_memory.h"
#include "wiced_bt_ota_firmware_upgrade.h"
#include "wiced_hal_puart.h"
#include "wiced_hal_wdog.h"
#include "wiced_bt_stack.h"
#include "wiced_bt_mesh_app.h"
#include "wiced_bt_uuid.h"
#include "wiced_bt_mesh_core.h"
#include "mesh_application.h"
#include "hci_control_api.h"
#include "wiced_bt_trace.h"
#if ( defined(CYW20706A2) || defined(CYW20719B1) || defined(CYW20719B0) || defined(CYW20721B1) || defined(CYW20735B0) || defined(CYW43012C0) )
#include "wiced_bt_app_common.h"
#endif

#include "src_light/andon_server.h"
#include "src_light/WyzeService.h"
#include "src_light/light_model.h"
#include "src_light/log.h"

// If defined then GATT_DB of the unprovisioned device contains OTA FW Upgrade service
#define MESH_FW_UPGRADE_UNPROVISIONED



// Comment out next line if we don't want to support GATT provisioning
#define MESH_SUPPORT_PB_GATT

#define MESH_GATT_MAX_WRITE_SIZE    512

/******************************************************
 *          Function Prototypes
 ******************************************************/
       wiced_bt_gatt_status_t   mesh_gatts_callback(wiced_bt_gatt_evt_t event, wiced_bt_gatt_event_data_t *p_data);
static wiced_bt_gatt_status_t   mesh_write_handler(uint16_t conn_id, wiced_bt_gatt_write_t * p_data);
static wiced_bt_gatt_status_t   mesh_prep_write_handler(uint16_t conn_id, wiced_bt_gatt_write_t * p_data);
static wiced_bt_gatt_status_t   mesh_write_exec_handler(uint16_t conn_id);
static void mesh_hci_trace_cback(wiced_bt_hci_trace_type_t type, uint16_t length, uint8_t* p_data);
extern wiced_bt_cfg_settings_t wiced_bt_cfg_settings;

extern wiced_bt_gatt_status_t remote_provision_gatt_client_event(wiced_bt_gatt_evt_t event, wiced_bt_gatt_event_data_t* p_data);
extern wiced_bt_gatt_status_t mesh_gatt_client_event(wiced_bt_gatt_evt_t event, wiced_bt_gatt_event_data_t* p_data);
extern void                   mesh_start_stop_scan_callback(wiced_bool_t start, wiced_bool_t is_active);

/******************************************************
 *          Variables Definitions
 ******************************************************/
/*
 * This is the GATT database for the WICED Mesh applications.
 * The database defines services, characteristics and
 * descriptors supported by the application.  Each attribute in the database
 * has a handle, (characteristic has two, one for characteristic itself,
 * another for the value).  The handles are used by the peer to access
 * attributes, and can be used locally by application, for example to retrieve
 * data written by the peer.  Definition of characteristics and descriptors
 * has GATT Properties (read, write, notify...) but also has permissions which
 * identify if peer application is allowed to read or write into it.
 * Handles do not need to be sequential, but need to be in order.
 *
 * Mesh applications have 2 GATT databases. One is shown while device is not
 * provisioned, this one contains Provisioning GATT service.  After device is
 * provisioned, it has GATT Proxy service.
 */
uint8_t gatt_db_unprovisioned[]=
{
    // Declare mandatory GATT service
    PRIMARY_SERVICE_UUID16(MESH_HANDLE_GATT_SERVICE, UUID_SERVICE_GATT),
        // Declare optional GATT service characteristic: Service Changed
        CHARACTERISTIC_UUID16(MESH_HANDLE_GATT_SERVICE_CHARACTERISTIC_SERVICE_CHANGED,
                              MESH_HANDLE_GATT_SERVICE_CHARACTERISTIC_SERVICE_CHANGED_VAL,
                              UUID_CHARACTERISTIC_SERVICE_CHANGED,
                              LEGATTDB_CHAR_PROP_INDICATE,
                              LEGATTDB_PERM_NONE),
            CHAR_DESCRIPTOR_UUID16_WRITABLE(MESH_HANDLE_DESCR_SERVICE_CHANGED,
                                            UUID_DESCRIPTOR_CLIENT_CHARACTERISTIC_CONFIGURATION,
                                            LEGATTDB_PERM_READABLE | LEGATTDB_PERM_WRITE_REQ),

    // Handle MESH_HANDLE_GAP_SERVICE (0x14): GAP service
    // Device Name and Appearance are mandatory characteristics.
    PRIMARY_SERVICE_UUID16(MESH_HANDLE_GAP_SERVICE, UUID_SERVICE_GAP),

        // Declare mandatory GAP service characteristic: Dev Name
        CHARACTERISTIC_UUID16(MESH_HANDLE_GAP_SERVICE_CHARACTERISTIC_DEV_NAME,
                              MESH_HANDLE_GAP_SERVICE_CHARACTERISTIC_DEV_NAME_VAL,
                              UUID_CHARACTERISTIC_DEVICE_NAME,
                              LEGATTDB_CHAR_PROP_READ, LEGATTDB_PERM_READABLE),

        // Declare mandatory GAP service characteristic: Appearance
        CHARACTERISTIC_UUID16(MESH_HANDLE_GAP_SERVICE_CHARACTERISTIC_APPEARANCE,
                              MESH_HANDLE_GAP_SERVICE_CHARACTERISTIC_APPEARANCE_VAL,
                              UUID_CHARACTERISTIC_APPEARANCE,
                              LEGATTDB_CHAR_PROP_READ, LEGATTDB_PERM_READABLE),

    // Mesh Provisionning Service.
    // This is the mesh application proprietary service. It has
    // characteristics which allows a device to provision a node.
    PRIMARY_SERVICE_UUID16(HANDLE_MESH_SERVICE_PROVISIONING, WICED_BT_MESH_CORE_UUID_SERVICE_PROVISIONING),

        // Handle HANDLE_CHAR_MESH_PROVISIONING_DATA_IN: characteristic Mesh Provisioning Data In
        // Handle HANDLE_CHAR_MESH_PROVISIONING_DATA_IN_VAL: characteristic Mesh Provisioning Data In Value
        // Characteristic is _WRITABLE and it allows writes.
        CHARACTERISTIC_UUID16_WRITABLE(HANDLE_CHAR_MESH_PROVISIONING_DATA_IN,
                                        HANDLE_CHAR_MESH_PROVISIONING_DATA_IN_VALUE,
                                        WICED_BT_MESH_CORE_UUID_CHARACTERISTIC_PROVISIONING_DATA_IN,
                                        LEGATTDB_CHAR_PROP_WRITE_NO_RESPONSE,
                                        LEGATTDB_PERM_WRITE_CMD | LEGATTDB_PERM_VARIABLE_LENGTH),

        // Handle HANDLE_CHAR_MESH_PROVISIONING_DATA_OUT: characteristic Mesh Provisioning Data Out
        // Handle HANDLE_CHAR_MESH_PROVISIONING_DATA_OUT_VAL: characteristic Mesh Provisioning Data Out Value
        // Characteristic can be notified to send provisioning PDU.
        CHARACTERISTIC_UUID16(HANDLE_CHAR_MESH_PROVISIONING_DATA_OUT,
                                        HANDLE_CHAR_MESH_PROVISIONING_DATA_OUT_VALUE,
                                        WICED_BT_MESH_CORE_UUID_CHARACTERISTIC_PROVISIONING_DATA_OUT,
                                        LEGATTDB_CHAR_PROP_NOTIFY,
                                        LEGATTDB_PERM_NONE),

		    // Handle HANDLE_DESCR_MESH_PROVISIONING_DATA_CLIENT_CONFIG: Characteristic Client Configuration Descriptor.
	        // This is standard GATT characteristic descriptor.  2 byte value 0 means that
	        // message to the client is disabled.  Peer can write value 1 to enable
	        // notifications.  Not _WRITABLE in the macro.  This
	        // means that attribute can be written by the peer.
            CHAR_DESCRIPTOR_UUID16_WRITABLE(HANDLE_DESCR_MESH_PROVISIONING_DATA_CLIENT_CONFIG,
	                                         UUID_DESCRIPTOR_CLIENT_CHARACTERISTIC_CONFIGURATION,
	                                         LEGATTDB_PERM_READABLE | LEGATTDB_PERM_WRITE_REQ),

    // Handle MESH_HANDLE_DEV_INFO_SERVICE (0x4D): Device Info service
    // Device Information service helps peer to identify manufacture or vendor of the
    // device.  It is required for some types of the devices, for example HID, medical,
    // and optional for others.  There are a bunch of characteristics available.
    PRIMARY_SERVICE_UUID16(MESH_HANDLE_DEV_INFO_SERVICE, UUID_SERVICE_DEVICE_INFORMATION),

        // Handle MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_MFR_NAME:
        //     characteristic Manufacturer Name
        // Handle MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_MFR_NAME_VAL:
        //     characteristic value
        CHARACTERISTIC_UUID16(MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_MFR_NAME,
                              MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_MFR_NAME_VAL,
                              UUID_CHARACTERISTIC_MANUFACTURER_NAME_STRING,
                              LEGATTDB_CHAR_PROP_READ, LEGATTDB_PERM_READABLE),

        // Handle MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_MODEL_NUM:
        //     characteristic Model Number
        // Handle MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_MODEL_NUM_VAL:
        //     characteristic value
        CHARACTERISTIC_UUID16(MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_MODEL_NUM,
                              MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_MODEL_NUM_VAL,
                              UUID_CHARACTERISTIC_MODEL_NUMBER_STRING,
                              LEGATTDB_CHAR_PROP_READ, LEGATTDB_PERM_READABLE),

        // Handle MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_SERIAL_NUMBER: characteristic System ID
        // Handle MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_SERIAL_NUMBER_VAL: characteristic value
        CHARACTERISTIC_UUID16(MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_SERIAL_NUMBER,
                              MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_SERIAL_NUMBER_VAL,
                              UUID_CHARACTERISTIC_SERIAL_NUMBER_STRING,
                              LEGATTDB_CHAR_PROP_READ, LEGATTDB_PERM_READABLE),

        // Handle MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_FW_VER: characteristic fW version
        // Handle MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_FW_VER_VAL: characteristic value
        CHARACTERISTIC_UUID16(MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_FW_VER,
                              MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_FW_VER_VAL,
                              UUID_CHARACTERISTIC_FIRMWARE_REVISION_STRING,
                              LEGATTDB_CHAR_PROP_READ, LEGATTDB_PERM_READABLE),

        // Handle MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_HW_VER: characteristic fW version
        // Handle MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_HW_VER_VAL: characteristic value
        CHARACTERISTIC_UUID16(MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_HW_VER,
                              MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_HW_VER_VAL,
                              UUID_CHARACTERISTIC_HARDWARE_REVISION_STRING,
                              LEGATTDB_CHAR_PROP_READ, LEGATTDB_PERM_READABLE),
                              

#ifdef DEF_ANDON_GATT
    // Declare proprietary Andon Service with 128 byte UUID
    PRIMARY_SERVICE_UUID128( HANDLE_ANDON_SERVICE, UUID_ANDON_SERVICE ),
    
    // Declare characteristic used to notify/indicate change
        CHARACTERISTIC_UUID128( HANDLE_ANDON_SERVICE_CHAR_NOTIFY, HANDLE_ANDON_SERVICE_CHAR_NOTIFY_VAL,
            UUID_ANDON_CHARACTERISTIC_NOTIFY, LEGATTDB_CHAR_PROP_NOTIFY, LEGATTDB_PERM_VARIABLE_LENGTH|LEGATTDB_PERM_READABLE ),

            // Declare client characteristic configuration descriptor
    // Value of the descriptor can be modified by the client
    // Value modified shall be retained during connection and across connection
            // for bonded devices.  Setting value to 1 tells this application to send notification
            // when value of the characteristic changes.  Value 2 is to allow indications.
            CHAR_DESCRIPTOR_UUID16_WRITABLE( HANDLE_ANDON_SERVICE_CHAR_CFG_DESC, UUID_DESCRIPTOR_CLIENT_CHARACTERISTIC_CONFIGURATION,
            LEGATTDB_PERM_READABLE | LEGATTDB_PERM_WRITE_REQ),// | LEGATTDB_PERM_AUTH_READABLE | LEGATTDB_PERM_AUTH_WRITABLE),

        // Declare characteristic Andon Configuration
        CHARACTERISTIC_UUID128_WRITABLE( HANDLE_ANDON_SERVICE_WRITE, HANDLE_ANDON_SERVICE_WRITE_VAL,
            UUID_ANDON_CHARACTERISTIC_WRITE, LEGATTDB_CHAR_PROP_WRITE|LEGATTDB_CHAR_PROP_WRITE_NO_RESPONSE,
            LEGATTDB_PERM_WRITE_CMD | LEGATTDB_PERM_WRITE_REQ ),
#endif

#ifdef DEF_ANDON_OTA
    // Handle 0x300: andon vendor specific WICED Upgrade Service. same to Broadcom, only no Encrypt and decrypt
    PRIMARY_SERVICE_UUID128(HANDLE_ANDON_OTA_FW_UPGRADE_SERVICE, UUID_ANDON_OTA_FW_UPGRADE_SERVICE),

        // Handles 0xff03: characteristic WS Control Point, handle 0xff04 characteristic value.
        CHARACTERISTIC_UUID128_WRITABLE(HANDLE_ANDON_OTA_FW_UPGRADE_CHARACTERISTIC_CONTROL_POINT, HANDLE_ANDON_OTA_FW_UPGRADE_CONTROL_POINT,
            UUID_ANDON_OTA_FW_UPGRADE_CHARACTERISTIC_CONTROL_POINT, LEGATTDB_CHAR_PROP_WRITE | LEGATTDB_CHAR_PROP_NOTIFY | LEGATTDB_CHAR_PROP_INDICATE,
            LEGATTDB_PERM_VARIABLE_LENGTH | LEGATTDB_PERM_WRITE_REQ /*| LEGATTDB_PERM_AUTH_WRITABLE*/),

            // Declare client characteristic configuration descriptor
            // Value of the descriptor can be modified by the client
            // Value modified shall be retained during connection and across connection
            // for bonded devices.  Setting value to 1 tells this application to send notification
            // when value of the characteristic changes.  Value 2 is to allow indications.
            CHAR_DESCRIPTOR_UUID16_WRITABLE(HANDLE_ANDON_OTA_FW_UPGRADE_CLIENT_CONFIGURATION_DESCRIPTOR, UUID_DESCRIPTOR_CLIENT_CHARACTERISTIC_CONFIGURATION,
                LEGATTDB_PERM_READABLE | LEGATTDB_PERM_WRITE_REQ /*| LEGATTDB_PERM_AUTH_WRITABLE */),

        // Handle 0xff07: characteristic WS Data, handle 0xff08 characteristic value. This
        // characteristic is used to send next portion of the FW Similar to the control point
        CHARACTERISTIC_UUID128_WRITABLE(HANDLE_ANDON_OTA_FW_UPGRADE_CHARACTERISTIC_DATA, HANDLE_ANDON_OTA_FW_UPGRADE_DATA,
            UUID_ANDON_OTA_FW_UPGRADE_CHARACTERISTIC_DATA, LEGATTDB_CHAR_PROP_WRITE,
            LEGATTDB_PERM_VARIABLE_LENGTH | LEGATTDB_PERM_WRITE_REQ | LEGATTDB_PERM_RELIABLE_WRITE /*| LEGATTDB_PERM_AUTH_WRITABLE */),
#endif // DEF_ANDON_OTA

#if WYZE_SERVICE_ENABLE    
    // Handle WYZE_SERVIC (0x0300): Wyze service
    // Device Name and Appearance are mandatory characteristics.
    PRIMARY_SERVICE_UUID16(HANDLE_WYZE_SERVICE, UUID_WYZE_SERVICE),

        // Declare mandatory Wyze service characteristic: 
        CHARACTERISTIC_UUID16_WRITABLE(HANDLE_WYZE_SERVICE_CHAR_WRITEANDNOTIFY,HANDLE_WYZE_SERVICE_CHAR_WRITEANDNOTIFY_VAL,
                              UUID_WYZE_CHARACTERISTIC_WRITEANDNOTIFY,LEGATTDB_CHAR_PROP_WRITE | LEGATTDB_CHAR_PROP_NOTIFY | LEGATTDB_CHAR_PROP_INDICATE,
                              LEGATTDB_PERM_VARIABLE_LENGTH | LEGATTDB_PERM_WRITE_REQ),
            CHAR_DESCRIPTOR_UUID16_WRITABLE(HANDLE_WYZE_SERVICE_CHAR_CFG_DESC, UUID_DESCRIPTOR_CLIENT_CHARACTERISTIC_CONFIGURATION,
	                                        LEGATTDB_PERM_READABLE | LEGATTDB_PERM_WRITE_REQ),
        // // Declare characteristic Andon Configuration
        // CHARACTERISTIC_UUID16_WRITABLE( HANDLE_WYZE_SERVICE_CHAR_WRITEANDNOTIFY, HANDLE_WYZE_SERVICE_CHAR_WRITEANDNOTIFY_VAL,
        //     UUID_WYZE_CHARACTERISTIC_WRITEANDNOTIFY, LEGATTDB_CHAR_PROP_WRITE,
        //     LEGATTDB_PERM_WRITE_CMD | LEGATTDB_PERM_WRITE_REQ ),
#endif //WYZE_SERVICE_ENABLE
    

#ifndef MESH_HOMEKIT_COMBO_APP
#ifdef MESH_FW_UPGRADE_UNPROVISIONED
    // Handle 0xff00: Broadcom vendor specific WICED Upgrade Service.
    PRIMARY_SERVICE_UUID128(HANDLE_OTA_FW_UPGRADE_SERVICE, UUID_OTA_FW_UPGRADE_SERVICE),

        // Handles 0xff03: characteristic WS Control Point, handle 0xff04 characteristic value.
        CHARACTERISTIC_UUID128_WRITABLE(HANDLE_OTA_FW_UPGRADE_CHARACTERISTIC_CONTROL_POINT, HANDLE_OTA_FW_UPGRADE_CONTROL_POINT,
            UUID_OTA_FW_UPGRADE_CHARACTERISTIC_CONTROL_POINT, LEGATTDB_CHAR_PROP_WRITE | LEGATTDB_CHAR_PROP_NOTIFY | LEGATTDB_CHAR_PROP_INDICATE,
            LEGATTDB_PERM_VARIABLE_LENGTH | LEGATTDB_PERM_WRITE_REQ /*| LEGATTDB_PERM_AUTH_WRITABLE*/),

            // Declare client characteristic configuration descriptor
            // Value of the descriptor can be modified by the client
            // Value modified shall be retained during connection and across connection
            // for bonded devices.  Setting value to 1 tells this application to send notification
            // when value of the characteristic changes.  Value 2 is to allow indications.
            CHAR_DESCRIPTOR_UUID16_WRITABLE(HANDLE_OTA_FW_UPGRADE_CLIENT_CONFIGURATION_DESCRIPTOR, UUID_DESCRIPTOR_CLIENT_CHARACTERISTIC_CONFIGURATION,
                LEGATTDB_PERM_READABLE | LEGATTDB_PERM_WRITE_REQ /*| LEGATTDB_PERM_AUTH_WRITABLE */),

        // Handle 0xff07: characteristic WS Data, handle 0xff08 characteristic value. This 
        // characteristic is used to send next portion of the FW Similar to the control point
        CHARACTERISTIC_UUID128_WRITABLE(HANDLE_OTA_FW_UPGRADE_CHARACTERISTIC_DATA, HANDLE_OTA_FW_UPGRADE_DATA,
            UUID_OTA_FW_UPGRADE_CHARACTERISTIC_DATA, LEGATTDB_CHAR_PROP_WRITE,
            LEGATTDB_PERM_VARIABLE_LENGTH | LEGATTDB_PERM_WRITE_REQ | LEGATTDB_PERM_RELIABLE_WRITE /*| LEGATTDB_PERM_AUTH_WRITABLE */),
#endif
#endif // MESH_HOMEKIT_COMBO_APP
};
// const uint32_t gatt_db_unprovisioned_size = sizeof(gatt_db_unprovisioned);

uint8_t gatt_db_provisioned[]=
{
    // Declare mandatory GATT service
    PRIMARY_SERVICE_UUID16(MESH_HANDLE_GATT_SERVICE, UUID_SERVICE_GATT),
        // Declare optional GATT service characteristic: Service Changed
        CHARACTERISTIC_UUID16(MESH_HANDLE_GATT_SERVICE_CHARACTERISTIC_SERVICE_CHANGED,
                              MESH_HANDLE_GATT_SERVICE_CHARACTERISTIC_SERVICE_CHANGED_VAL,
                              UUID_CHARACTERISTIC_SERVICE_CHANGED,
                              LEGATTDB_CHAR_PROP_INDICATE,
                              LEGATTDB_PERM_NONE),
            CHAR_DESCRIPTOR_UUID16_WRITABLE(MESH_HANDLE_DESCR_SERVICE_CHANGED,
                                            UUID_DESCRIPTOR_CLIENT_CHARACTERISTIC_CONFIGURATION,
                                            LEGATTDB_PERM_READABLE | LEGATTDB_PERM_WRITE_REQ),

    // Handle MESH_HANDLE_GAP_SERVICE (0x14): GAP service
    // Device Name and Appearance are mandatory characteristics.
    PRIMARY_SERVICE_UUID16(MESH_HANDLE_GAP_SERVICE, UUID_SERVICE_GAP),

        // Declare mandatory GAP service characteristic: Dev Name
        CHARACTERISTIC_UUID16(MESH_HANDLE_GAP_SERVICE_CHARACTERISTIC_DEV_NAME,
                              MESH_HANDLE_GAP_SERVICE_CHARACTERISTIC_DEV_NAME_VAL,
                              UUID_CHARACTERISTIC_DEVICE_NAME,
                              LEGATTDB_CHAR_PROP_READ, LEGATTDB_PERM_READABLE),

        // Declare mandatory GAP service characteristic: Appearance
        CHARACTERISTIC_UUID16(MESH_HANDLE_GAP_SERVICE_CHARACTERISTIC_APPEARANCE,
                              MESH_HANDLE_GAP_SERVICE_CHARACTERISTIC_APPEARANCE_VAL,
                              UUID_CHARACTERISTIC_APPEARANCE,
                              LEGATTDB_CHAR_PROP_READ, LEGATTDB_PERM_READABLE),

    PRIMARY_SERVICE_UUID16(HANDLE_MESH_SERVICE_PROXY, WICED_BT_MESH_CORE_UUID_SERVICE_PROXY),

	    // Handle HANDLE_CHAR_MESH_PROXY_DATA_IN: characteristic Mesh Proxy In
	    // Handle HANDLE_CHAR_MESH_PROXY_DATA_IN_VALUE: characteristic Mesh Proxy In Value
        CHARACTERISTIC_UUID16_WRITABLE(HANDLE_CHAR_MESH_PROXY_DATA_IN,
									    HANDLE_CHAR_MESH_PROXY_DATA_IN_VALUE,
                                        WICED_BT_MESH_CORE_UUID_CHARACTERISTIC_PROXY_DATA_IN,
									    LEGATTDB_CHAR_PROP_WRITE_NO_RESPONSE,
									    LEGATTDB_PERM_WRITE_CMD | LEGATTDB_PERM_VARIABLE_LENGTH),

	    // Handle HANDLE_CHAR_MESH_PROXY_DATA_OUT: characteristic Mesh Proxy Out
	    // Handle HANDLE_CHAR_MESH_PROXY_DATA_OUT_VALUE: characteristic Mesh Proxy Out Value
        CHARACTERISTIC_UUID16_WRITABLE(HANDLE_CHAR_MESH_PROXY_DATA_OUT,
									    HANDLE_CHAR_MESH_PROXY_DATA_OUT_VALUE,
                                        WICED_BT_MESH_CORE_UUID_CHARACTERISTIC_PROXY_DATA_OUT,
									    LEGATTDB_CHAR_PROP_NOTIFY,
									    LEGATTDB_PERM_WRITE_CMD | LEGATTDB_PERM_VARIABLE_LENGTH),

		    // Handle HANDLE_DESCR_MESH_PROXY_DATA_CLIENT_CONFIG: Characteristic Client Configuration Descriptor.
	        // This is standard GATT characteristic descriptor.  2 byte value 0 means that
	        // message to the client is disabled.  Peer can write value 1 to enable
	        // notifications.  Not _WRITABLE in the macro.  This
	        // means that attribute can be written by the peer.
	        CHAR_DESCRIPTOR_UUID16_WRITABLE (HANDLE_DESCR_MESH_PROXY_DATA_CLIENT_CONFIG,
	                                         UUID_DESCRIPTOR_CLIENT_CHARACTERISTIC_CONFIGURATION,
	                                         LEGATTDB_PERM_READABLE | LEGATTDB_PERM_WRITE_REQ),


    // Handle MESH_HANDLE_DEV_INFO_SERVICE (0x4D): Device Info service
    // Device Information service helps peer to identify manufacture or vendor of the
    // device.  It is required for some types of the devices, for example HID, medical,
    // and optional for others.  There are a bunch of characteristics available.
    PRIMARY_SERVICE_UUID16(MESH_HANDLE_DEV_INFO_SERVICE, UUID_SERVICE_DEVICE_INFORMATION),

        // Handle MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_MFR_NAME:
        //     characteristic Manufacturer Name
        // Handle MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_MFR_NAME_VAL:
        //     characteristic value
        CHARACTERISTIC_UUID16(MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_MFR_NAME,
                              MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_MFR_NAME_VAL,
                              UUID_CHARACTERISTIC_MANUFACTURER_NAME_STRING,
                              LEGATTDB_CHAR_PROP_READ, LEGATTDB_PERM_READABLE),

        // Handle MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_MODEL_NUM:
        //     characteristic Model Number
        // Handle MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_MODEL_NUM_VAL:
        //     characteristic value
        CHARACTERISTIC_UUID16(MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_MODEL_NUM,
                              MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_MODEL_NUM_VAL,
                              UUID_CHARACTERISTIC_MODEL_NUMBER_STRING,
                              LEGATTDB_CHAR_PROP_READ, LEGATTDB_PERM_READABLE),

        // Handle MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_SERIAL_NUMBER: characteristic System ID
        // Handle MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_SERIAL_NUMBER_VAL: characteristic value
        CHARACTERISTIC_UUID16(MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_SERIAL_NUMBER,
                              MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_SERIAL_NUMBER_VAL,
                              UUID_CHARACTERISTIC_SERIAL_NUMBER_STRING,
                              LEGATTDB_CHAR_PROP_READ, LEGATTDB_PERM_READABLE),

        // Handle MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_FW_VER: characteristic fW version
        // Handle MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_FW_VER_VAL: characteristic value
        CHARACTERISTIC_UUID16(MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_FW_VER,
                              MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_FW_VER_VAL,
                              UUID_CHARACTERISTIC_FIRMWARE_REVISION_STRING,
                              LEGATTDB_CHAR_PROP_READ, LEGATTDB_PERM_READABLE),

        // Handle MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_HW_VER: characteristic fW version
        // Handle MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_HW_VER_VAL: characteristic value
        CHARACTERISTIC_UUID16(MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_HW_VER,
                              MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_HW_VER_VAL,
                              UUID_CHARACTERISTIC_HARDWARE_REVISION_STRING,
                              LEGATTDB_CHAR_PROP_READ, LEGATTDB_PERM_READABLE),
                              

#ifdef _DEB_COMMAND_SERVICE
    // Handle HANDLE_MESH_SERVICE_COMMAND: Mesh temporary Command Service.
    // This is the mesh application proprietary service. It has
    // characteristics which allows a device to send commands to a node.
    PRIMARY_SERVICE_UUID16(HANDLE_MESH_SERVICE_COMMAND, WICED_BT_MESH_CORE_UUID_SERVICE_COMMAND),

        // Handle HANDLE_CHAR_MESH_COMMAND_DATA: temporary characteristic Mesh Command Data
        // Handle HANDLE_CHAR_MESH_COMMAND_DATA_VAL: temporary characteristic Mesh Command Data Value
        // Characteristic is _WRITABLE and it allows writes.
        CHARACTERISTIC_UUID16_WRITABLE(HANDLE_CHAR_MESH_COMMAND_DATA,
                                        HANDLE_CHAR_MESH_COMMAND_DATA_VALUE,
                                        WICED_BT_MESH_CORE_UUID_CHARACTERISTIC_COMMAND_DATA,
                                        LEGATTDB_CHAR_PROP_WRITE_NO_RESPONSE | LEGATTDB_CHAR_PROP_NOTIFY,
                                        LEGATTDB_PERM_WRITE_CMD | LEGATTDB_PERM_VARIABLE_LENGTH),

		    // Handle HANDLE_DESCR_MESH_COMMAND_DATA_CLIENT_CONFIG: Characteristic Client Configuration Descriptor.
	        // This is standard GATT characteristic descriptor.  2 byte value 0 means that
	        // message to the client is disabled.  Peer can write value 1 or 2 to enable
	        // notifications or indications respectively.  Not _WRITABLE in the macro.  This
	        // means that attribute can be written by the peer.
            CHAR_DESCRIPTOR_UUID16_WRITABLE(HANDLE_DESCR_MESH_COMMAND_DATA_CLIENT_CONFIG,
	                                         UUID_DESCRIPTOR_CLIENT_CHARACTERISTIC_CONFIGURATION,
	                                         LEGATTDB_PERM_READABLE | LEGATTDB_PERM_WRITE_REQ),
#endif

#ifdef DEF_ANDON_GATT
    // Declare proprietary Andon Service with 128 byte UUID
    PRIMARY_SERVICE_UUID128( HANDLE_ANDON_SERVICE, UUID_ANDON_SERVICE ),
    
    // Declare characteristic used to notify/indicate change
        CHARACTERISTIC_UUID128( HANDLE_ANDON_SERVICE_CHAR_NOTIFY, HANDLE_ANDON_SERVICE_CHAR_NOTIFY_VAL,
            UUID_ANDON_CHARACTERISTIC_NOTIFY, LEGATTDB_CHAR_PROP_NOTIFY, LEGATTDB_PERM_VARIABLE_LENGTH|LEGATTDB_PERM_READABLE ),

            // Declare client characteristic configuration descriptor
    // Value of the descriptor can be modified by the client
    // Value modified shall be retained during connection and across connection
            // for bonded devices.  Setting value to 1 tells this application to send notification
            // when value of the characteristic changes.  Value 2 is to allow indications.
            CHAR_DESCRIPTOR_UUID16_WRITABLE( HANDLE_ANDON_SERVICE_CHAR_CFG_DESC, UUID_DESCRIPTOR_CLIENT_CHARACTERISTIC_CONFIGURATION,
            LEGATTDB_PERM_READABLE | LEGATTDB_PERM_WRITE_REQ),// | LEGATTDB_PERM_AUTH_READABLE | LEGATTDB_PERM_AUTH_WRITABLE),

        // Declare characteristic Andon Configuration
        CHARACTERISTIC_UUID128_WRITABLE( HANDLE_ANDON_SERVICE_WRITE, HANDLE_ANDON_SERVICE_WRITE_VAL,
            UUID_ANDON_CHARACTERISTIC_WRITE, LEGATTDB_CHAR_PROP_WRITE|LEGATTDB_CHAR_PROP_WRITE_NO_RESPONSE,
            LEGATTDB_PERM_WRITE_CMD | LEGATTDB_PERM_WRITE_REQ ),
#endif  //DEF_ANDON_GATT

#ifdef DEF_ANDON_OTA
    // Handle 0x300: andon vendor specific WICED Upgrade Service. same to Broadcom, only no Encrypt and decrypt
    PRIMARY_SERVICE_UUID128(HANDLE_ANDON_OTA_FW_UPGRADE_SERVICE, UUID_ANDON_OTA_FW_UPGRADE_SERVICE),

        // Handles 0xff03: characteristic WS Control Point, handle 0xff04 characteristic value.
        CHARACTERISTIC_UUID128_WRITABLE(HANDLE_ANDON_OTA_FW_UPGRADE_CHARACTERISTIC_CONTROL_POINT, HANDLE_ANDON_OTA_FW_UPGRADE_CONTROL_POINT,
            UUID_ANDON_OTA_FW_UPGRADE_CHARACTERISTIC_CONTROL_POINT, LEGATTDB_CHAR_PROP_WRITE | LEGATTDB_CHAR_PROP_NOTIFY | LEGATTDB_CHAR_PROP_INDICATE,
            LEGATTDB_PERM_VARIABLE_LENGTH | LEGATTDB_PERM_WRITE_REQ /*| LEGATTDB_PERM_AUTH_WRITABLE*/),

            // Declare client characteristic configuration descriptor
            // Value of the descriptor can be modified by the client
            // Value modified shall be retained during connection and across connection
            // for bonded devices.  Setting value to 1 tells this application to send notification
            // when value of the characteristic changes.  Value 2 is to allow indications.
            CHAR_DESCRIPTOR_UUID16_WRITABLE(HANDLE_ANDON_OTA_FW_UPGRADE_CLIENT_CONFIGURATION_DESCRIPTOR, UUID_DESCRIPTOR_CLIENT_CHARACTERISTIC_CONFIGURATION,
                LEGATTDB_PERM_READABLE | LEGATTDB_PERM_WRITE_REQ /*| LEGATTDB_PERM_AUTH_WRITABLE */),

        // Handle 0xff07: characteristic WS Data, handle 0xff08 characteristic value. This
        // characteristic is used to send next portion of the FW Similar to the control point
        CHARACTERISTIC_UUID128_WRITABLE(HANDLE_ANDON_OTA_FW_UPGRADE_CHARACTERISTIC_DATA, HANDLE_ANDON_OTA_FW_UPGRADE_DATA,
            UUID_ANDON_OTA_FW_UPGRADE_CHARACTERISTIC_DATA, LEGATTDB_CHAR_PROP_WRITE,
            LEGATTDB_PERM_VARIABLE_LENGTH | LEGATTDB_PERM_WRITE_REQ | LEGATTDB_PERM_RELIABLE_WRITE /*| LEGATTDB_PERM_AUTH_WRITABLE */),
#endif // DEF_ANDON_OTA

#if WYZE_SERVICE_ENABLE 
    // Handle WYZE_SERVIC (0x0300): Wyze service
    // Device Name and Appearance are mandatory characteristics.
    PRIMARY_SERVICE_UUID16(HANDLE_WYZE_SERVICE, UUID_WYZE_SERVICE),

        // Declare mandatory Wyze service characteristic: 
        CHARACTERISTIC_UUID16_WRITABLE(HANDLE_WYZE_SERVICE_CHAR_WRITEANDNOTIFY,HANDLE_WYZE_SERVICE_CHAR_WRITEANDNOTIFY_VAL,
                              UUID_WYZE_CHARACTERISTIC_WRITEANDNOTIFY,LEGATTDB_CHAR_PROP_WRITE | LEGATTDB_CHAR_PROP_NOTIFY | LEGATTDB_CHAR_PROP_INDICATE,
                              LEGATTDB_PERM_VARIABLE_LENGTH | LEGATTDB_PERM_WRITE_REQ),
            CHAR_DESCRIPTOR_UUID16_WRITABLE(HANDLE_WYZE_SERVICE_CHAR_CFG_DESC,
	                                        UUID_DESCRIPTOR_CLIENT_CHARACTERISTIC_CONFIGURATION,
	                                        LEGATTDB_PERM_READABLE | LEGATTDB_PERM_WRITE_REQ),
#endif  //WYZE_SERVICE_ENABLE

    // Handle 0xff00: Broadcom vendor specific WICED Upgrade Service.
    PRIMARY_SERVICE_UUID128(HANDLE_OTA_FW_UPGRADE_SERVICE, UUID_OTA_FW_UPGRADE_SERVICE),

        // Handles 0xff03: characteristic WS Control Point, handle 0xff04 characteristic value.
        CHARACTERISTIC_UUID128_WRITABLE(HANDLE_OTA_FW_UPGRADE_CHARACTERISTIC_CONTROL_POINT, HANDLE_OTA_FW_UPGRADE_CONTROL_POINT,
            UUID_OTA_FW_UPGRADE_CHARACTERISTIC_CONTROL_POINT, LEGATTDB_CHAR_PROP_WRITE | LEGATTDB_CHAR_PROP_NOTIFY | LEGATTDB_CHAR_PROP_INDICATE,
            LEGATTDB_PERM_VARIABLE_LENGTH | LEGATTDB_PERM_WRITE_REQ /*| LEGATTDB_PERM_AUTH_WRITABLE*/),

            // Declare client characteristic configuration descriptor
            // Value of the descriptor can be modified by the client
            // Value modified shall be retained during connection and across connection
            // for bonded devices.  Setting value to 1 tells this application to send notification
            // when value of the characteristic changes.  Value 2 is to allow indications.
            CHAR_DESCRIPTOR_UUID16_WRITABLE(HANDLE_OTA_FW_UPGRADE_CLIENT_CONFIGURATION_DESCRIPTOR, UUID_DESCRIPTOR_CLIENT_CHARACTERISTIC_CONFIGURATION,
                LEGATTDB_PERM_READABLE | LEGATTDB_PERM_WRITE_REQ /*| LEGATTDB_PERM_AUTH_WRITABLE */),

        // Handle 0xff07: characteristic WS Data, handle 0xff08 characteristic value. This 
        // characteristic is used to send next portion of the FW Similar to the control point
        CHARACTERISTIC_UUID128_WRITABLE(HANDLE_OTA_FW_UPGRADE_CHARACTERISTIC_DATA, HANDLE_OTA_FW_UPGRADE_DATA,
            UUID_OTA_FW_UPGRADE_CHARACTERISTIC_DATA, LEGATTDB_CHAR_PROP_WRITE,
            LEGATTDB_PERM_VARIABLE_LENGTH | LEGATTDB_PERM_WRITE_REQ | LEGATTDB_PERM_RELIABLE_WRITE /*| LEGATTDB_PERM_AUTH_WRITABLE */),

};
// const uint32_t gatt_db_provisioned_size = sizeof(gatt_db_provisioned);

typedef struct
{
    uint16_t handle;
    uint16_t attr_len;
    void     *p_attr;
} attribute_t;

typedef struct
{
    // GATT Prepare Write command process
    uint8_t                 *p_write_buffer;
    uint16_t                write_length;
    uint16_t                write_handle;
} mesh_gatt_cb_t;

uint8_t mesh_0[20]                  = { 0 };
uint8_t mesh_prov_client_config[2]  = { BIT16_TO_8(GATT_CLIENT_CONFIG_NOTIFICATION) };
uint8_t mesh_proxy_client_config[2] = { 0 };
#ifdef _DEB_COMMAND_SERVICE
uint8_t mesh_cmd_client_config[2]   = { BIT16_TO_8(GATT_CLIENT_CONFIG_NOTIFICATION) };
#endif
uint16_t service_changed_client_configuration = 0;  // characteristic client configuration descriptor
uint16_t conn_id = 0;
uint16_t conn_mtu = 0; // MTU to use in wiced_bt_mesh_core_connection_status() at notifications enable
mesh_gatt_cb_t mesh_gatt_cb;

struct 
{
    uint16_t conn_id;
    uint16_t conn_mtu;
    wiced_bt_device_address_t remaddr;
}gattconnpara[3];

extern uint8_t mesh_mfr_name[WICED_BT_MESH_PROPERTY_LEN_DEVICE_MANUFACTURER_NAME];
extern uint8_t mesh_model_num[WICED_BT_MESH_PROPERTY_LEN_DEVICE_MODEL_NUMBER];
extern uint8_t mesh_system_id[WICED_BT_MESH_PROPERTY_LEN_DEVICE_SERIAL_NUMBER]; 
extern uint8_t mesh_fW_ver[WICED_BT_MESH_PROPERTY_LEN_DEVICE_FIRMWARE_REVISION];
extern uint8_t mesh_hW_ver[WICED_BT_MESH_PROPERTY_LEN_DEVICE_HARDWARE_REVISION];

attribute_t gauAttributes[] =
{
    { MESH_HANDLE_GAP_SERVICE_CHARACTERISTIC_DEV_NAME_VAL, 0, NULL },
    { MESH_HANDLE_GAP_SERVICE_CHARACTERISTIC_APPEARANCE_VAL, 0, NULL },
    { HANDLE_CHAR_MESH_PROVISIONING_DATA_IN_VALUE, 20, mesh_0 },
    { HANDLE_DESCR_MESH_PROVISIONING_DATA_CLIENT_CONFIG, sizeof(mesh_prov_client_config), mesh_prov_client_config },
    { HANDLE_CHAR_MESH_PROXY_DATA_IN_VALUE, 20, mesh_0 },
    { HANDLE_DESCR_MESH_PROXY_DATA_CLIENT_CONFIG, sizeof(mesh_proxy_client_config), mesh_proxy_client_config },
    { MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_MFR_NAME_VAL, sizeof(mesh_mfr_name), mesh_mfr_name },
    { MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_MODEL_NUM_VAL, sizeof(mesh_model_num), mesh_model_num },
    { MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_SERIAL_NUMBER_VAL, sizeof(mesh_system_id), mesh_system_id },
    { MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_FW_VER_VAL, sizeof(mesh_fW_ver), mesh_fW_ver },
    { MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_HW_VER_VAL, sizeof(mesh_hW_ver), mesh_hW_ver },
#ifdef _DEB_COMMAND_SERVICE
    { HANDLE_CHAR_MESH_COMMAND_DATA_VALUE, 20, mesh_0 },
    { HANDLE_DESCR_MESH_COMMAND_DATA_CLIENT_CONFIG, sizeof(mesh_cmd_client_config), mesh_cmd_client_config },
#endif
};

#ifdef DEF_ANDON_OTA
uint16_t ota_channel_flag = 0;           //记录OTA使用的服务，1是Andon服务，带加密，0是Cy的复位，有加密 不能同时交叉使用
#endif

/******************************************************
 *               Function Definitions
 ******************************************************/

wiced_bt_gatt_status_t mesh_ota_firmware_upgrade_send_data_callback(wiced_bool_t is_notification, uint16_t conn_id, uint16_t attr_handle, uint16_t val_len, uint8_t *p_val)
{
    wiced_bt_gatt_status_t status = WICED_BT_GATT_INTERNAL_ERROR;
    uint16_t ret_data_len = 0;

    uint8_t *p_buffer = (uint8_t *)wiced_bt_get_buffer(128);
    const char *str = "ota_tx_cb: ";

    if (p_buffer == NULL)
    {
        // WICED_BT_TRACE("mesh_ota_firmware_upgrade_send_data_callback: no_mem len=%d\n", 128);
        WICED_BT_TRACE("%sno_mem len=%d\n", str, 128);
        return WICED_BT_GATT_NO_RESOURCES;
    }
#ifdef DEF_ANDON_OTA
    if(ota_channel_flag == 1)
    {
        //TODO 使用Andon加密算法加密
        uint8_t *encryptarray;
        uint16_t outlen;

        encryptarray = AndonServiceUpgradeEncrypt(p_val,val_len,&outlen);
        if(encryptarray == NULL){
            return WICED_BT_GATT_NO_RESOURCES;
        }
        if(outlen > 128)
        {
            wiced_bt_free_buffer(encryptarray);
            return WICED_BT_GATT_NO_RESOURCES;
        }
        memcpy(p_buffer,encryptarray,outlen);
        ret_data_len = outlen;
        wiced_bt_free_buffer(encryptarray);
        attr_handle = (attr_handle&0xff)|(HANDLE_ANDON_OTA_FW_UPGRADE_SERVICE&0xFF00);
    }
    else
#endif
    {
        // Encrypt outgoing notification/indication data here
        ret_data_len =  wiced_bt_mesh_core_crypt(WICED_TRUE, p_val, val_len, p_buffer, 128);
    }
    

    // WICED_BT_TRACE("mesh_ota_firmware_upgrade_send_data_callback: ret_data_len:%d\n", ret_data_len);
    WICED_BT_TRACE("%sret_data_len:%d\n", str, ret_data_len);

    if (ret_data_len)
    {
        if (is_notification)
        {
            status = wiced_bt_gatt_send_notification(conn_id, attr_handle, ret_data_len, p_buffer);
        }
        else
        {
            status = wiced_bt_gatt_send_indication(conn_id, attr_handle, ret_data_len, p_buffer);
        }
    }

    wiced_bt_free_buffer(p_buffer);

    return status;
}

// This function is executed in the BTM_ENABLED_EVT management callback.
void mesh_app_gatt_init(void)
{
#ifndef MESH_HOMEKIT_COMBO_APP
    /* Register with stack to receive GATT callback */
    wiced_bt_gatt_register(mesh_gatts_callback);
    memset(&mesh_gatt_cb, 0, sizeof(mesh_gatt_cb_t));
#endif // MESH_HOMEKIT_COMBO_APP
}

// setup appropriate GATT DB
void mesh_app_gatt_db_init(wiced_bool_t is_authenticated)
{
    wiced_bt_gatt_status_t gatt_status;
    const char *str = "gatt_db_init: ";

#ifndef WICEDX_LINUX

    if (!is_authenticated)
    {
        gatt_status = wiced_bt_gatt_db_init(gatt_db_unprovisioned, sizeof(gatt_db_unprovisioned));
        // WICED_BT_TRACE("mesh_application_init: unprovisioned GATT DB status:%x\n", gatt_status);
        WICED_BT_TRACE("%sunprovisioned GATT DB status:%x\n", str, gatt_status);
    }
    // If device is provisioned we will always show the Proxy service.  If device really
    // does not support Proxy feature it will not send connectable adverts, so it will not
    // cause any problems.  If Proxy feature is supported and turned on, or if device is
    // making itself connectable after provisioning over PB-GATT or when Identification is
    // turned on, the Proxy service is required.  In the latter 2 cases, core will make sure that
    // the device does not relay packets received over GATT.
    else
    {
        gatt_status = wiced_bt_gatt_db_init(gatt_db_provisioned, sizeof(gatt_db_provisioned));
        // WICED_BT_TRACE("mesh_application_init: provisioned GATT DB status:%x\n", gatt_status);
        WICED_BT_TRACE("%sprovisioned GATT DB status:%x\n", str, gatt_status);
    }
#endif

//    wiced_bt_dev_register_hci_trace(mesh_hci_trace_cback);

    UNUSED_VARIABLE(gatt_status);
}

// After the device is provisioned over GATT, this function is called to send GATT Service
// Changed indication to notify client that GATT database has changed. It is up to client
// whether to disconnect GATT or do service discovery again to find proxy service.
void mesh_app_gatt_send_service_changed_indication(void)
{
    uint8_t buf[4];
    uint8_t *p = buf;
    wiced_bt_gatt_status_t status;

    if (service_changed_client_configuration & GATT_CLIENT_CONFIG_INDICATION)
    {
        // The client enabled indication. Sends indication to notify the client that
        // GATT service has changed
        UINT16_TO_STREAM(p, 0x0001);
        UINT16_TO_STREAM(p, 0xFFFF);
        status = wiced_bt_gatt_send_indication(conn_id, MESH_HANDLE_GATT_SERVICE_CHARACTERISTIC_SERVICE_CHANGED_VAL, 4, buf);
        WICED_BT_TRACE("send_service_changed_indication: status:%x\n", status);
    }
}

// Find attribute description by handle
static attribute_t * get_attribute_(uint16_t handle)
{
    int i;
    for (i = 0; i < sizeof(gauAttributes) / sizeof(gauAttributes[0]); i++)
    {
        if((gauAttributes[i].handle >= MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_MFR_NAME) 
            && (gauAttributes[i].handle <= MESH_HANDLE_DEV_INFO_SERVICE_CHARACTERISTIC_HW_VER_VAL))
        {
            gauAttributes[i].attr_len = strlen(gauAttributes[i].p_attr);
        }
        if (gauAttributes[i].handle == handle)
        {
            return (&gauAttributes[i]);
        }
    }
    WICED_BT_TRACE("get_attribute_: attr not found:%x\n", handle);
    return NULL;
}


// Process Read request or command from peer device
static wiced_bt_gatt_status_t read_handler_(uint16_t conn_id, wiced_bt_gatt_read_t * p_read_data)
{
    attribute_t *puAttribute;
    int          attr_len_to_copy;
    uint8_t     buf[2];

#ifndef WICEDX_LINUX
    // if read request is for the OTA FW upgrade service, pass it to the library to process
    if ((p_read_data->handle > HANDLE_OTA_FW_UPGRADE_SERVICE) && (p_read_data->handle <= HANDLE_OTA_FW_UPGRADE_APP_INFO))
    {
#ifdef DEF_ANDON_OTA
        ota_channel_flag = 0;
#endif // DEF_ANDON_OTA
        return wiced_ota_fw_upgrade_read_handler(conn_id, p_read_data);
    }
#ifdef DEF_ANDON_OTA
    if ((p_read_data->handle > HANDLE_ANDON_OTA_FW_UPGRADE_SERVICE) && (p_read_data->handle <= HANDLE_ANDON_OTA_FW_UPGRADE_APP_INFO))
    {
        ota_channel_flag = 1;
        p_read_data->handle = (p_read_data->handle&0XFF) | (HANDLE_OTA_FW_UPGRADE_SERVICE&0xFF00);
        return wiced_ota_fw_upgrade_read_handler(conn_id, p_read_data);
    }
#endif // DEF_ANDON_OTA
#endif

#ifdef DEF_ANDON_GATT    
    // if read request is for the andon service
    if ((p_read_data->handle > HANDLE_ANDON_SERVICE) && (p_read_data->handle <= HANDLE_ANDON_SERVICE_WRITE_VAL))
    {
        LOG_VERBOSE("Andon Server Read\n");
        return AndonServerReadHandle(conn_id, p_read_data);
    }
#endif

#if WYZE_SERVICE_ENABLE 
    // if read request is for the wyze service
    if ((p_read_data->handle > HANDLE_WYZE_SERVICE) && (p_read_data->handle <= HANDLE_WYZE_SERVICE_CHAR_CFG_DESC))
    {
        LOG_VERBOSE("Andon Server Read\n");
        return WyzeServerReadHandle(conn_id, p_read_data);
    }
#endif

    if (p_read_data->handle == MESH_HANDLE_DESCR_SERVICE_CHANGED)
    {
        if (p_read_data->offset >= 2)
            return WICED_BT_GATT_INVALID_OFFSET;
        if (*p_read_data->p_val_len < 2)
            return WICED_BT_GATT_INVALID_ATTR_LEN;
        if (p_read_data->offset == 1)
        {
            p_read_data->p_val[0] = service_changed_client_configuration >> 8;
            *p_read_data->p_val_len = 1;
        }
        else
        {
            p_read_data->p_val[0] = service_changed_client_configuration & 0xFF;
            p_read_data->p_val[1] = service_changed_client_configuration >> 8;
            *p_read_data->p_val_len = 2;
        }
        return WICED_BT_GATT_SUCCESS;
    }

    if ((puAttribute = get_attribute_(p_read_data->handle)) == NULL)
    {
        return WICED_BT_GATT_INVALID_HANDLE;
    }
    if ((p_read_data->handle == MESH_HANDLE_GAP_SERVICE_CHARACTERISTIC_DEV_NAME_VAL) && (puAttribute->attr_len == 0))
    {
        puAttribute->p_attr = wiced_bt_cfg_settings.device_name;
        puAttribute->attr_len = strlen((char*)wiced_bt_cfg_settings.device_name);
    }
    else if ((p_read_data->handle == MESH_HANDLE_GAP_SERVICE_CHARACTERISTIC_APPEARANCE_VAL) && (puAttribute->attr_len == 0))
    {
        buf[0] = (uint8_t)wiced_bt_cfg_settings.gatt_cfg.appearance;
        buf[1] = (uint8_t)(wiced_bt_cfg_settings.gatt_cfg.appearance >> 8);
        puAttribute->p_attr = buf;
        puAttribute->attr_len = 2;
    }
    attr_len_to_copy = puAttribute->attr_len;

    //WICED_BT_TRACE("read_handler_: conn_id:%d hdl:%x\n", conn_id, p_read_data->handle);
    //WICED_BT_TRACE(" offset:%d len:%d\n", p_read_data->offset, attr_len_to_copy);

    if (p_read_data->offset >= puAttribute->attr_len)
    {
        attr_len_to_copy = 0;
    }

    if (attr_len_to_copy != 0)
    {
        uint8_t *from;
        int      to_copy = attr_len_to_copy - p_read_data->offset;


        if (to_copy > *p_read_data->p_val_len)
        {
            to_copy = *p_read_data->p_val_len;
        }

        from = ((uint8_t *)puAttribute->p_attr) + p_read_data->offset;
        *p_read_data->p_val_len = to_copy;

        memcpy(p_read_data->p_val, from, to_copy);
    }

    return WICED_BT_GATT_SUCCESS;
}

/*
* Process indication_confirm from peer device
*/
wiced_bt_gatt_status_t mesh_cfm_handler(uint16_t conn_id, uint16_t handle)
{
#ifndef WICEDX_LINUX
    // if write request is for the OTA FW upgrade service, pass it to the library to process
    if ((handle > HANDLE_OTA_FW_UPGRADE_SERVICE) && (handle <= HANDLE_OTA_FW_UPGRADE_APP_INFO))
    {
#ifdef DEF_ANDON_OTA
        ota_channel_flag = 0;
#endif
        return wiced_ota_fw_upgrade_indication_cfm_handler(conn_id, handle);
    }
#ifdef DEF_ANDON_OTA
    if ((handle > HANDLE_ANDON_OTA_FW_UPGRADE_SERVICE) && (handle <= HANDLE_ANDON_OTA_FW_UPGRADE_APP_INFO))
    {
        ota_channel_flag = 1;
        handle= (handle&0XFF) | (HANDLE_OTA_FW_UPGRADE_SERVICE&0xFF00);
        return wiced_ota_fw_upgrade_indication_cfm_handler(conn_id, handle);
    }
    #endif
#endif
    if (handle == MESH_HANDLE_GATT_SERVICE_CHARACTERISTIC_SERVICE_CHANGED_VAL)
    {
        return WICED_BT_GATT_SUCCESS;
    }
    return WICED_BT_GATT_INVALID_HANDLE;
}

// Process GATT request from the peer
wiced_bt_gatt_status_t mesh_gatts_req_cb(wiced_bt_gatt_attribute_request_t *p_data)
{
    wiced_bt_gatt_status_t result = WICED_BT_GATT_INVALID_PDU;

    WICED_BT_TRACE("mesh_gatts_req_cb. conn:%d, type:%d\n", p_data->conn_id, p_data->request_type);

    switch (p_data->request_type)
    {
    case GATTS_REQ_TYPE_READ:
        result = read_handler_(p_data->conn_id, &(p_data->data.read_req));
        break;

    case GATTS_REQ_TYPE_WRITE:
        result = mesh_write_handler(p_data->conn_id, &(p_data->data.write_req));
        break;

    case GATTS_REQ_TYPE_PREP_WRITE:
        result = mesh_prep_write_handler(p_data->conn_id, &(p_data->data.write_req));
        break;

    case GATTS_REQ_TYPE_WRITE_EXEC:
        result = mesh_write_exec_handler(p_data->conn_id);
        break;

    case GATTS_REQ_TYPE_MTU:
        WICED_BT_TRACE("mesh_gatts_req_cb:GATTS_REQ_TYPE_MTU mtu:%x\n", p_data->data.mtu);
        // We will use that MTU in wiced_bt_mesh_core_connection_status() at notifications enable
        conn_mtu = p_data->data.mtu;
        wiced_bt_mesh_core_set_gatt_mtu(p_data->data.mtu);
        result = WICED_BT_GATT_SUCCESS;
        break;

    case GATTS_REQ_TYPE_CONF:
        result = mesh_cfm_handler(p_data->conn_id, p_data->data.handle);
        break;

    default:
	break;
    }
    return result;
}

/*
* Callback for various GATT events.  As this application performs only as a GATT server, some of the events are ommitted.
*/
wiced_bt_gatt_status_t mesh_gatts_callback(wiced_bt_gatt_evt_t event, wiced_bt_gatt_event_data_t *p_data)
{
    extern wiced_bool_t appAndonBleConnectUsed(void);

    wiced_bt_gatt_status_t result = WICED_BT_GATT_INVALID_PDU;

    WICED_BT_TRACE("mesh_gatts_callback: event:%x p_data:%x\n", event, (uint32_t)p_data);

    switch (event)
    {
    case GATT_CONNECTION_STATUS_EVT:
        WICED_BT_TRACE("GATT_CONNECTION_STATUS: connected:%d authenticated:%d conn_id:%x reason:%d role:%d\n", p_data->connection_status.connected, mesh_app_node_is_provisioned(), p_data->connection_status.conn_id, p_data->connection_status.reason, p_data->connection_status.link_role);
#ifdef DEF_ANDON_OTA
		ota_channel_flag = 0; 
#endif
        /*
#ifdef USE_LED_AND_BUTTON
        if (p_data->connection_status.connected)
        {
            wiced_bt_btn_led_blink(5, 300);
        }
#endif
        */

        result = WICED_BT_GATT_SUCCESS;

        // If we are in Slave role, this is a connection for Proxy or for Provisioning server.
        // Otherwise, we are a provisioner and we established connection to a new device or to a proxy device
        if (p_data->connection_status.link_role == HCI_ROLE_SLAVE)
        {
            void appBleConnectNotify(wiced_bool_t isconneted, wiced_bt_device_address_t addr);
            if (wiced_bt_mesh_app_func_table.p_mesh_app_gatt_conn_status)
            {
                wiced_bt_mesh_app_func_table.p_mesh_app_gatt_conn_status(&p_data->connection_status);
            }

            if (p_data->connection_status.connected)
            {
                // on connection up return proxy advert interval to 0.5 sec
                // On connection up core stops proxy adverts.On next disconnection it will start adverts with interval 800
                wiced_bt_mesh_core_proxy_adv_interval = 800;

                conn_id = p_data->connection_status.conn_id;
                // When connection is established befor GATTS_REQ_TYPE_MTU, use the default MTU 23 bytes which makes max message length 20
                // Otherwise don't change MTU
                if(conn_mtu == 0)
                    conn_mtu = 23;
                appBleConnectNotify(WICED_TRUE,p_data->connection_status.bd_addr);   
                for(uint8_t i =0; i<sizeof(gattconnpara)/sizeof(gattconnpara[0]);i++){
                    if((gattconnpara[i].conn_id) && (gattconnpara[i].conn_id != conn_id)){
                        continue;
                    }
                    gattconnpara[i].conn_id = conn_id;
                    gattconnpara[i].conn_mtu = conn_mtu;
                    memcpy(gattconnpara[i].remaddr,p_data->connection_status.bd_addr,sizeof(wiced_bt_device_address_t));
                } 
                // wiced_bt_l2cap_update_ble_conn_params(p_data->connection_status.bd_addr, 30, 48, 0, 200);
                // wiced_bt_l2cap_update_ble_conn_params(p_data->connection_status.bd_addr, 60, 96, 0, 400);
                // We will call mesh_core's wiced_bt_mesh_core_connection_status() on notification enable (write 0x0001 to HANDLE_DESCR_MESH_PROXY_DATA_CLIENT_CONFIG)
            }
            else
            {
                for(uint8_t i =0; i<sizeof(gattconnpara)/sizeof(gattconnpara[0]);i++){
                    if(gattconnpara[i].conn_id != conn_id){
                         continue;
                    }
                    gattconnpara[i].conn_id = 0;
                    gattconnpara[i].conn_mtu = 0;
                    memset(gattconnpara[i].remaddr,0,sizeof(wiced_bt_device_address_t));
                } 
                conn_id = 0;
                conn_mtu = 0;
				appBleConnectNotify(WICED_FALSE,p_data->connection_status.bd_addr);  
                appAndonBleConnectUsed();
                // On disconnect ref_data is disconnection reason.
                wiced_bt_mesh_core_connection_status(0, WICED_FALSE, p_data->connection_status.reason, 20);
            }
    #ifndef WICEDX_LINUX
            // Pass connection up/down event to the OTA FW upgrade library
            {
                wiced_bt_gatt_connection_status_t connection_status = { 0 };
                connection_status.connected = p_data->connection_status.connected;
                connection_status.conn_id = p_data->connection_status.connected ? p_data->connection_status.conn_id : 0;
                connection_status.bd_addr = p_data->connection_status.bd_addr;
                // if(!appAndonBleConnectUsed())
                {
                    wiced_ota_fw_upgrade_connection_status_event(&connection_status);
                }
            }
    #endif
        }
        else
        {
#if defined REMOTE_PROVISION_SERVER_SUPPORTED && defined REMOTE_PROVISION_OVER_GATT_SUPPORTED
            remote_provision_gatt_client_event(event, p_data);
#endif
#if defined GATT_PROXY_CLIENT_SUPPORTED
            mesh_gatt_client_event(event, p_data);
#endif
        }
        break;

    case GATT_ATTRIBUTE_REQUEST_EVT:
        result = mesh_gatts_req_cb(&p_data->attribute_request);
        break;

    default:
#if defined REMOTE_PROVISION_SERVER_SUPPORTED && defined REMOTE_PROVISION_OVER_GATT_SUPPORTED
        remote_provision_gatt_client_event(event, p_data);
#endif
#if defined GATT_PROXY_CLIENT_SUPPORTED
        mesh_gatt_client_event(event, p_data);
#endif
	break;
    }
    return result;
}

uint8_t mesh_app_gatt_mtu(void)
{
    return conn_mtu;
}

wiced_bool_t mesh_app_gatt_is_connected(void)
{
    return conn_id != 0;
}

void mesh_app_gatt_is_disconnected(void)
{
    if(conn_id != 0)
    {
        wiced_bt_gatt_disconnect(conn_id);
    }
}
// Handle a command packet received from the BLE host.
static wiced_bt_gatt_status_t mesh_write_handler(uint16_t conn_id, wiced_bt_gatt_write_t * p_data)
{
    wiced_bt_gatt_status_t result    = WICED_BT_GATT_SUCCESS;
    uint8_t                *attr     = p_data->p_val;
    uint16_t                len      = p_data->val_len;
    attribute_t            *pAttr;
    uint8_t                *p_buffer;
    const char             *str = "write_handler: ";

    WICED_BT_TRACE("write_handler: conn_id:%x handle=%x len=%d\n", conn_id, p_data->handle, len);
    //WICED_BT_TRACE_ARRAY((char*)attr, len, "");
#ifndef WICEDX_LINUX

    // if write request is for the OTA FW upgrade service, pass it to the library to process
    if ((p_data->handle > HANDLE_OTA_FW_UPGRADE_SERVICE) && (p_data->handle <= HANDLE_OTA_FW_UPGRADE_APP_INFO))
    {
#ifdef DEF_ANDON_OTA
        ota_channel_flag = 0;
#endif // DEF_ANDON_OTA
        if ((p_data->handle == HANDLE_OTA_FW_UPGRADE_CONTROL_POINT) || (p_data->handle ==  HANDLE_OTA_FW_UPGRADE_DATA))
        {
            p_buffer = (uint8_t *)wiced_bt_get_buffer(p_data->val_len);
            if (p_buffer == NULL)
            {
                WICED_BT_TRACE("%sno_mem len=%d\n", str, p_data->val_len);
                return WICED_BT_GATT_NO_RESOURCES;
            }
            // Decrypt data to use
            p_data->val_len = wiced_bt_mesh_core_crypt(WICED_FALSE, p_data->p_val, p_data->val_len, p_buffer, p_data->val_len);
            p_data->p_val = p_buffer;

            result = wiced_ota_fw_upgrade_write_handler(conn_id, p_data);
            wiced_bt_free_buffer(p_buffer);
        }
        else
        {
            result = wiced_ota_fw_upgrade_write_handler(conn_id, p_data);
        }
    }
#ifdef DEF_ANDON_OTA
    else if ((p_data->handle > HANDLE_ANDON_OTA_FW_UPGRADE_SERVICE) && (p_data->handle <= HANDLE_ANDON_OTA_FW_UPGRADE_APP_INFO))
    {
        uint16_t decryptlen;
        uint8_t* decryptarray;
        ota_channel_flag = 1;
        //TODO 数据解密
        WICED_BT_TRACE("%s andon ota write data\n", str);
        WICED_BT_TRACE("%s handle %x \n", str,p_data->handle);
        if ((p_data->handle == HANDLE_ANDON_OTA_FW_UPGRADE_CONTROL_POINT) || (p_data->handle ==  HANDLE_ANDON_OTA_FW_UPGRADE_DATA))
        {
            decryptarray = AndonServiceUpgradeDecrypt(p_data->p_val,p_data->val_len,&decryptlen);
            if(decryptarray == NULL)
            {
                result = WICED_BT_GATT_ERROR;
            }
            memcpy(p_data->p_val,decryptarray,decryptlen);
            // WICED_BT_TRACE_ARRAY(p_data->p_val,decryptlen,"");
            p_data->val_len = decryptlen;
            wiced_bt_free_buffer(decryptarray);
            p_data->handle = (p_data->handle&0xFF) | (HANDLE_OTA_FW_UPGRADE_SERVICE&0xFF00);
            result = wiced_ota_fw_upgrade_write_handler(conn_id, p_data);
        }
        else
        {
            p_data->handle = (p_data->handle&0xFF) | (HANDLE_OTA_FW_UPGRADE_SERVICE&0xFF00);
            WICED_BT_TRACE("%s handle %x \n", str,p_data->handle);
            result = wiced_ota_fw_upgrade_write_handler(conn_id, p_data);
        }
    }
#endif // DEF_ANDON_OTA
    else
#endif
#ifdef DEF_ANDON_GATT
    if ((p_data->handle > HANDLE_ANDON_SERVICE) && (p_data->handle <= HANDLE_ANDON_SERVICE_WRITE_VAL))
    {
        LOG_VERBOSE("Andon Server Write\n");
        result = AndonServerWriteHandle(conn_id, p_data);
    }
    else
#endif
#if WYZE_SERVICE_ENABLE 
    if((p_data->handle > HANDLE_WYZE_SERVICE) && (p_data->handle <= HANDLE_WYZE_SERVICE_CHAR_CFG_DESC))
    {
        LOG_VERBOSE("Andon Server Write\n");
        result = WyzeServerWriteHandle(conn_id, p_data);
    }
    else 
#endif
    {
        switch (p_data->handle)
        {
#ifdef _DEB_COMMAND_SERVICE
        case HANDLE_CHAR_MESH_COMMAND_DATA_VALUE:
            if (!mesh_app_node_is_provisioned())
            {
                result = WICED_BT_GATT_WRONG_STATE;
                WICED_BT_TRACE("%smesh isn't started yet. ignoring...\n", str);
                break;
            }
            switch (*attr)
            {
            case MESH_COMMAND_RESET_NODE:
                if (len != 1)
                {
                    result = WICED_BT_GATT_INVALID_ATTR_LEN;
                    break;
                }
                WICED_BT_TRACE("%sRESET_NODE\n", str);
                wiced_bt_mesh_core_init(NULL);
                extern wiced_bool_t node_authenticated;
                node_authenticated = WICED_FALSE;
                break;
            }
            break;
#endif

        case HANDLE_CHAR_MESH_PROVISIONING_DATA_IN_VALUE:
            if (mesh_app_node_is_provisioned())
            {
                result = WICED_BT_GATT_WRONG_STATE;
                WICED_BT_TRACE("%smesh is started already. ignoring...\n", str);
                break;
            }
            // Pass received packet to pb_gat transport layer
            wiced_bt_mesh_core_provision_gatt_packet(0, conn_id, attr, len);
            break;

        case HANDLE_CHAR_MESH_PROXY_DATA_IN_VALUE:
            // handle it the same way as mesh packet received via advert report
            if (!mesh_app_node_is_provisioned())
            {
                result = WICED_BT_GATT_WRONG_STATE;
                WICED_BT_TRACE("%smesh isn't started. ignoring...\n", str);
                break;
            }
            wiced_bt_mesh_core_proxy_packet(attr, len);
            break;

        case HANDLE_DESCR_MESH_PROVISIONING_DATA_CLIENT_CONFIG:
        case HANDLE_DESCR_MESH_PROXY_DATA_CLIENT_CONFIG:
#ifdef _DEB_COMMAND_SERVICE
        case HANDLE_DESCR_MESH_COMMAND_DATA_CLIENT_CONFIG:
#endif
            pAttr = get_attribute_(p_data->handle);
            if (pAttr)
            {
                memset(pAttr->p_attr, 0, pAttr->attr_len);
                memcpy(pAttr->p_attr, attr, len <= pAttr->attr_len ? len : pAttr->attr_len);
            }

#ifdef _DEB_COMMAND_SERVICE
            if (p_data->handle != HANDLE_DESCR_MESH_PROXY_DATA_CLIENT_CONFIG && p_data->handle != HANDLE_DESCR_MESH_PROVISIONING_DATA_CLIENT_CONFIG)
                break;
#endif
            // Use conn_mtu value to call wiced_bt_mesh_core_connection_status() just once - on first write to HANDLE_DESCR_MESH_PROXY_DATA_CLIENT_CONFIG
            if (conn_mtu != 0)
            {
                wiced_bt_mesh_core_connection_status(conn_id, WICED_FALSE, 0, conn_mtu);
                conn_mtu = 0;
            }

            break;

        case MESH_HANDLE_DESCR_SERVICE_CHANGED:
            if (p_data->val_len != 2)
            {
                return WICED_BT_GATT_INVALID_ATTR_LEN;
            }
            service_changed_client_configuration = (p_data->p_val[1] << 8) | p_data->p_val[0];
            WICED_BT_TRACE("%sDESCR_SERVICE_CHANGED: %04x\n", str, service_changed_client_configuration);
            break;

        default:
            WICED_BT_TRACE("%sSkip the packet...\n", str);
            result = WICED_BT_GATT_INVALID_HANDLE;
            break;
        }
    }
    // WICED_BT_TRACE("write_handler: returns %d handle:%x\n", result, p_data->handle);
    return result;
}

// Handle GATT Prepare Write command
static wiced_bt_gatt_status_t mesh_prep_write_handler(uint16_t conn_id, wiced_bt_gatt_write_t * p_write_req)
{
    wiced_bt_gatt_status_t result = WICED_BT_GATT_SUCCESS;

    if (!mesh_gatt_cb.p_write_buffer)
    {
        mesh_gatt_cb.p_write_buffer = wiced_bt_get_buffer(MESH_GATT_MAX_WRITE_SIZE);
        if (!mesh_gatt_cb.p_write_buffer)
            return WICED_BT_GATT_PREPARE_Q_FULL;
    }

    if (p_write_req->offset + p_write_req->val_len <= MESH_GATT_MAX_WRITE_SIZE)
    {
        mesh_gatt_cb.write_handle = p_write_req->handle;
        memcpy(mesh_gatt_cb.p_write_buffer + p_write_req->offset, p_write_req->p_val, p_write_req->val_len);
        mesh_gatt_cb.write_length = p_write_req->offset + p_write_req->val_len;
    }
    else
    {
        result = WICED_BT_GATT_INSUF_RESOURCE;
    }

    return result;
}

// Handle GATT Write Execute command
static wiced_bt_gatt_status_t mesh_write_exec_handler(uint16_t conn_id)
{
    wiced_bt_gatt_status_t result = WICED_BT_GATT_SUCCESS;

    if (mesh_gatt_cb.p_write_buffer)
    {
        wiced_bt_gatt_write_t write_req;

        memset(&write_req, 0, sizeof(wiced_bt_gatt_write_t));
        write_req.handle  = mesh_gatt_cb.write_handle;
        write_req.p_val   = mesh_gatt_cb.p_write_buffer;
        write_req.val_len = mesh_gatt_cb.write_length;
        write_req.is_prep = WICED_FALSE;

        result = mesh_write_handler(conn_id, &write_req);

        wiced_bt_free_buffer(mesh_gatt_cb.p_write_buffer);
        mesh_gatt_cb.p_write_buffer = NULL;
    }
    else
    {
        result = WICED_BT_GATT_INSUF_RESOURCE;
    }

    return result;
}

// Callback function to send a packet over GATT proxy connection.  If gatt_char_handle parameter is not
// zero, this device performs as a GATT client and should use GATT Write Command to send the packet out.
// Otherwise, the device is a GATT server and should use GATT Notification
void mesh_app_proxy_gatt_send_cb(uint32_t conn_id, uint32_t ref_data, const uint8_t *packet, uint32_t packet_len)
{
    uint16_t               gatt_char_handle = (uint16_t)ref_data;

    //WICED_BT_TRACE("proxy_gatt_send_cb: conn_id:%x packet: \n", conn_id);
    //WICED_BT_TRACE_ARRAY((char*)packet, packet_len, "");
    if (ref_data)
    {
        wiced_bt_gatt_value_t *p_value = (wiced_bt_gatt_value_t *)wiced_bt_get_buffer(packet_len + sizeof(wiced_bt_gatt_value_t) - 1);
        if (p_value)
        {
            p_value->handle = (uint16_t)ref_data;
            p_value->offset = 0;
            p_value->len = packet_len;
            p_value->auth_req = 0;
            memcpy(p_value->value, packet, packet_len);

            wiced_bt_gatt_send_write(conn_id, GATT_WRITE_NO_RSP, p_value);
            wiced_bt_free_buffer(p_value);
        }
    }
    else if (mesh_proxy_client_config[0] & GATT_CLIENT_CONFIG_NOTIFICATION)
    {
        wiced_bt_gatt_send_notification(conn_id, HANDLE_CHAR_MESH_PROXY_DATA_OUT_VALUE, packet_len, (uint8_t*)packet);
        // WICED_BT_TRACE("proxy_gatt_send_cb: conn_id:%x res:%d\n", conn_id, res);
    }
    else
    {
#ifndef MESH_APPLICATION_MCU_MEMORY
        WICED_BT_TRACE("proxy_gatt_send_cb: drop notification\n");
#else
        WICED_BT_TRACE("Proxy Data to MCU len:%d\n", packet_len);
        mesh_application_send_hci_event(HCI_CONTROL_MESH_EVENT_PROXY_DATA, packet, packet_len);
#endif
    }
}

//  Pass protocol traces up through the UART
static void mesh_hci_trace_cback(wiced_bt_hci_trace_type_t type, uint16_t length, uint8_t* p_data)
{
    // handle HCI_HARDWARE_ERROR_EVT - print trace and reboot.
    if (type == HCI_TRACE_EVENT && p_data[0] == HCI_HARDWARE_ERROR_EVT && p_data[2] != 0)
    {
        // allow retry for HARDWARE_CODE_VS_UART_PARSING_ERROR (p_data[2] == 0)
        WICED_BT_TRACE("Rebooting on HCI_HARDWARE_ERROR_EVT(%x)...\n", p_data[2]);
        wiced_hal_wdog_reset_system();
    }
#ifdef _DEB_ENABLE_HCI_TRACE
#ifdef _DEB_ENABLE_HCI_TRACE_NO_BLE_ADV_EVT
    // filter out HCI_BLE_Advertising_Report_Event
    if (type == 0               // if HCI event
        && p_data[0] == 0x3e    // and HCI_BLE_Event
        && p_data[2] == 2)      // and HCI_BLE_Advertising_Report_Event
        return;
#endif
    wiced_transport_send_hci_trace(NULL, type, length, p_data);
//#else
//    if ((type == HCI_TRACE_COMMAND) || (type == HCI_TRACE_EVENT))
//    {
//        WICED_BT_TRACE("hci_trace_cback: type:%x length:%d\n", type, length);
//        WICED_BT_TRACE_ARRAY((char*)p_data, length, "");
//    }
#endif
}

void wiced_bt_gatt_mesh_close_link(uint16_t conn_id)
{
    wiced_bt_gatt_disconnect(conn_id);
}
