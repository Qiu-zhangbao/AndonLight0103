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

#include "bt_types.h"
#include "wiced_bt_gatt.h"
#include "wiced_bt_trace.h"
#include "wiced_timer.h"
#include "wiced_bt_l2c.h"

#include "wiced_platform.h"
#include "customer_service.h"

#define HCI_CONTROL_GROUP_MESH_GATEWAY              0x80

#define HCI_CONTROL_MESH_GATEWAY_DATA_IN           ( ( HCI_CONTROL_GROUP_MESH_GATEWAY << 8 ) | 0x01 )

#define HCI_CONTROL_MESH_GATEWAY_DATA_OUT          ( ( HCI_CONTROL_GROUP_MESH_GATEWAY << 8 ) | 0x02 )

uint16_t customer_service_config_descriptor = 0;

uint16_t g_conn_id = 0;

/*
 * Set new value for client configuration descriptor
 */
void customer_service_set_client_configuration(uint16_t client_config)
{
    customer_service_config_descriptor = client_config;

    WICED_BT_TRACE("customer_service_config_descriptor changed: %d\n", customer_service_config_descriptor);
}

/*
 * Process GATT Read request
 */
wiced_bt_gatt_status_t wiced_customer_service_read_handler(uint16_t conn_id, wiced_bt_gatt_read_t * p_read_data)
{
    g_conn_id = conn_id;

    switch (p_read_data->handle)
    {
    case HANDLE_CUSTOMER_SERVICE_CONFIGURATION_DESCRIPTOR:
        if (p_read_data->offset >= 2)
            return WICED_BT_GATT_INVALID_OFFSET;

        if (*p_read_data->p_val_len < 2)
            return WICED_BT_GATT_INVALID_ATTR_LEN;

        if (p_read_data->offset == 1)
        {
            p_read_data->p_val[0] = customer_service_config_descriptor >> 8;
            *p_read_data->p_val_len = 1;
        }
        else
        {
            p_read_data->p_val[0] = customer_service_config_descriptor & 0xff;
            p_read_data->p_val[1] = customer_service_config_descriptor >> 8;
            *p_read_data->p_val_len = 2;
        }
        return WICED_BT_GATT_SUCCESS;
    }
    return WICED_BT_GATT_INVALID_HANDLE;
}

/*
 * Process GATT Write request
 */
wiced_bt_gatt_status_t wiced_customer_service_write_handler(uint16_t conn_id, wiced_bt_gatt_write_t *p_write_data)
{
    WICED_BT_TRACE("p_write_data->handle: %x. conn_id: %x\n", p_write_data->handle, conn_id);

    switch (p_write_data->handle)
    {
    case HANDLE_CUSTOMER_SERVICE_CHARACTERISTIC_WRITE_VALUE:
        
        mesh_application_send_hci_event(HCI_CONTROL_MESH_GATEWAY_DATA_IN , p_write_data->p_val, p_write_data->val_len);
        break;

    case HANDLE_CUSTOMER_SERVICE_CONFIGURATION_DESCRIPTOR:
        if (p_write_data->val_len != 2)
        {
            WICED_BT_TRACE("customer service config wrong len %d\n", p_write_data->val_len);
            return WICED_BT_GATT_INVALID_ATTR_LEN;
        }
        customer_service_set_client_configuration(p_write_data->p_val[0] + (p_write_data->p_val[1] << 8));
        break;

    default:
        return WICED_BT_GATT_INVALID_HANDLE;
        break;
    }
    return WICED_BT_GATT_SUCCESS;
}

/*
 * Send Notification if allowed, or indication if allowed, or return error
 */
wiced_bt_gatt_status_t wiced_customer_service_send_notification(uint16_t conn_id, uint16_t attr_handle, uint16_t val_len, uint8_t *p_val)
{
    if (customer_service_config_descriptor & GATT_CLIENT_CONFIG_NOTIFICATION)
    {
        return wiced_bt_gatt_send_notification(conn_id, attr_handle, val_len, p_val);
    }
    else if (customer_service_config_descriptor & GATT_CLIENT_CONFIG_INDICATION)
    {
        return wiced_bt_gatt_send_indication(conn_id, attr_handle, val_len, p_val);
    }
    
    return WICED_BT_GATT_ERROR;
}

/*
 * Process GATT indication confirmation
 */
wiced_bt_gatt_status_t wiced_customer_service_indication_cfm_handler(uint16_t conn_id, uint16_t handle)
{
    WICED_BT_TRACE("wiced_customer_service_indication_cfm_handler, conn %d hdl %d\n", conn_id, handle);

    return WICED_BT_GATT_SUCCESS;
}

wiced_bool_t wiced_customer_service_is_gatt_handle(uint16_t handle)
{
    return HANDLE_CUSTOMER_SERVICE <= handle && handle <= HANDLE_CUSTOMER_SERVICE_CHARACTERISTIC_WRITE_VALUE;
}

wiced_bool_t wiced_customer_service_data_out_handle(uint16_t opcode, uint8_t *p_data, uint32_t length)
{
    if (opcode == HCI_CONTROL_MESH_GATEWAY_DATA_OUT)
    {
        wiced_customer_service_send_notification(g_conn_id,
                                                 HANDLE_CUSTOMER_SERVICE_CHARACTERISTIC_NOTIFY_VALUE,
                                                 length,
                                                 p_data);

        return TRUE;
    }

    return FALSE;
}
