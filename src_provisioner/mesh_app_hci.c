/*
* $ Copyright 2016-YEAR Cypress Semiconductor $
*/

/** @file
 *
 * This file contains functions which embeddded application can use to send and
 * receive data from the host.
 */
#include "wiced_bt_cfg.h"
#include "wiced_memory.h"
#include "wiced_timer.h"
#include "wiced_platform.h"
#include "wiced_transport.h"
#include "wiced_bt_mesh_app.h"
#include "wiced_hal_wdog.h"
#include "wiced_bt_mesh_app.h"
#include "wiced_bt_mesh_models.h"
#include "wiced_bt_mesh_core.h"
#ifdef MESH_DFU_SUPPORTED
#include "wiced_bt_mesh_dfu.h"
#endif
#include "wiced_bt_trace.h"
#include "mesh_application.h"
#include "hci_control_api.h"
#include "wiced_hal_puart.h"
#include "src_light/log.h"
#ifdef CYW20706A2
#include "wiced_bt_hci_defs.h"
#else
#include "hcidefs.h"
#endif

// Enables HCI trace to first(same as app download) com port.
//#define _DEB_ENABLE_HCI_TRACE
#define _DEB_ENABLE_HCI_TRACE_NO_BLE_ADV_EVT

static void  mesh_hci_trace_cback(wiced_bt_hci_trace_type_t type, uint16_t length, uint8_t* p_data);
#ifdef HCI_CONTROL
wiced_bool_t mesh_app_raw_access_layer_mode_handler(wiced_bt_mesh_event_t* p_event, const uint8_t* params, uint16_t params_len);
void mesh_app_on_send_raw_access_layer_pdu(uint8_t* p_data, uint16_t len);
void mesh_app_on_set_raw_access_layer_clnt_devkey(uint8_t* p_data, uint16_t len);
#endif

/******************************************************
 *          Function Prototypes
 ******************************************************/

/******************************************************
 *          Variables Definitions
 ******************************************************/


#define HCI_UART_BAUDRATE  115200
uint32_t mesh_application_proc_rx_cmd(uint8_t *p_buffer, uint32_t length);

/* transport configuration */
const wiced_transport_cfg_t  transport_cfg =
{
    .type                = WICED_TRANSPORT_UART,
#ifdef HCI_UART_BAUDRATE
    .cfg.uart_cfg        = { .mode = WICED_TRANSPORT_UART_HCI_MODE, .baud_rate = HCI_UART_BAUDRATE },
#else
    .cfg.uart_cfg        = { .mode = WICED_TRANSPORT_UART_HCI_MODE, .baud_rate = HCI_UART_DEFAULT_BAUD },   // default it 3mbps
#endif

#ifdef HCI_CONTROL
    .rx_buff_pool_cfg    = {  .buffer_size = 1024, .buffer_count = 2 },
#else
    .rx_buff_pool_cfg    = {  .buffer_size = 100, .buffer_count = 2 },
#endif
    .p_status_handler    = NULL,
    .p_data_handler      = mesh_application_proc_rx_cmd,
    .p_tx_complete_cback = NULL
};

wiced_transport_buffer_pool_t* host_trans_pool = NULL;

/******************************************************
 *               Function Definitions
 ******************************************************/
void mesh_app_hci_init(void)
{
#ifndef MESH_HOMEKIT_COMBO_APP
    wiced_transport_init(&transport_cfg);

    // create special pool for sending data to the MCU
#ifdef HCI_CONTROL
    host_trans_pool = wiced_transport_create_buffer_pool(1024, 2);
#else
    host_trans_pool = wiced_transport_create_buffer_pool(100, 2);
#endif
#endif

#ifndef WICEDX_LINUX
#if (defined(WICED_BT_TRACE_ENABLE) || defined(_ANDONDEBUG))
#ifndef _DEB_ENABLE_HCI_TRACE
    //For the 24 MHz board set the UART type as WICED_ROUTE_DEBUG_TO_PUART
    // For BCM920706V2_EVAL board make sure SW5.2 and SW5.4 are ON and all other SW5 are off
    wiced_set_debug_uart(WICED_ROUTE_DEBUG_TO_PUART);
#if (defined(CYW20706A2) || defined(CYW20735B0) || defined(CYW20719B0) || defined(CYW43012C0))
    wiced_hal_puart_select_uart_pads(WICED_PUART_RXD, WICED_PUART_TXD, 0, 0);
#endif
#ifndef CYW20706A2
    wiced_hal_gpio_select_function(WICED_P32, WICED_UART_2_TXD);
    wiced_hal_puart_set_baudrate(921600);
    #if (CHIP_SCHEME == CHIP_DEVKIT) && (LOGLEVEL >= LOGLEVEL_VERBOSE)
    wiced_hal_puart_disable_tx();
    wiced_hal_puart_flow_off();
    wiced_hal_puart_register_interrupt(Cmdline_Receive_Handle);
    wiced_hal_puart_set_watermark_level(1);
    wiced_hal_puart_enable_tx();
    wiced_hal_puart_enable_rx();
    #endif
#endif
#else
    // WICED_ROUTE_DEBUG_TO_WICED_UART to send debug strings over the WICED debug interface */
    wiced_set_debug_uart(WICED_ROUTE_DEBUG_TO_WICED_UART);
#endif
#endif
#endif
#ifdef HCI_CONTROL
    wiced_bt_dev_register_hci_trace(mesh_hci_trace_cback);
#endif
}

#ifdef HCI_CONTROL
wiced_bt_mesh_hci_event_t *wiced_bt_mesh_create_hci_event(wiced_bt_mesh_event_t *p_event)
{
    wiced_bt_mesh_hci_event_t *p_hci_event = (wiced_bt_mesh_hci_event_t *)wiced_transport_allocate_buffer(host_trans_pool);
    if (p_hci_event == NULL)
    {
        WICED_BT_TRACE("no trans buffer\n");
    }
    else if (p_event != NULL)
    {
        p_hci_event->src         = p_event->src;
        p_hci_event->dst         = p_event->dst;
        p_hci_event->app_key_idx = p_event->app_key_idx;
        p_hci_event->element_idx = p_event->element_idx;
        p_hci_event->rssi        = p_event->rssi;
        p_hci_event->ttl         = p_event->ttl;
        p_hci_event->company_id  = p_event->company_id;
        p_hci_event->opcode      = p_event->opcode;
    }
    return p_hci_event;
}

wiced_bt_mesh_hci_event_t *wiced_bt_mesh_alloc_hci_event(uint8_t element_idx)
{
    wiced_bt_mesh_hci_event_t *p_hci_event = (wiced_bt_mesh_hci_event_t *)wiced_transport_allocate_buffer(host_trans_pool);
    if (p_hci_event == NULL)
    {
        WICED_BT_TRACE("no trans buffer\n");
    }
    else
    {
        p_hci_event->src         = 0;
        p_hci_event->app_key_idx = 0;
        p_hci_event->element_idx = element_idx;
    }
    return p_hci_event;
}

wiced_result_t mesh_transport_send_data(uint16_t opcode, uint8_t *p_trans_buf, uint16_t data_len)
{
    wiced_result_t result = wiced_transport_send_buffer(opcode, p_trans_buf, data_len);
    if (result != WICED_BT_SUCCESS)
    {
        WICED_BT_TRACE("transport send buffer:%d\n", result);
    }
    return result;
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

void mesh_app_set_local_device(uint8_t* p_data, uint32_t length)
{
    uint16_t addr, net_key_idx;
    uint32_t iv_idx;
    uint8_t  dev_key[16], network_key[16], key_refresh, iv_update, model_level_access;
    uint8_t* p_buffer;
    uint8_t status = HCI_CONTROL_MESH_STATUS_SUCCESS;

    STREAM_TO_UINT16(addr, p_data);
    STREAM_TO_ARRAY(dev_key, p_data, 16);
    STREAM_TO_ARRAY(network_key, p_data, 16);
    STREAM_TO_UINT16(net_key_idx, p_data);
    STREAM_TO_UINT32(iv_idx, p_data);
    STREAM_TO_UINT8(key_refresh, p_data);
    STREAM_TO_UINT8(iv_update, p_data);
    STREAM_TO_UINT8(model_level_access, p_data);

    WICED_BT_TRACE("addr:%x net_key_idx:%x iv_idx:%x key_refresh:%d iv_upd:%d model_access:%d\n", addr, net_key_idx, iv_idx, key_refresh, iv_update, model_level_access);
    if (!wiced_bt_mesh_core_provision_local(addr, dev_key, network_key, net_key_idx, iv_idx, key_refresh, iv_update))
        status = HCI_CONTROL_MESH_STATUS_ERROR;
    if (wiced_bt_mesh_app_func_table.p_mesh_app_init)
        wiced_bt_mesh_app_func_table.p_mesh_app_init(WICED_TRUE);
    if (NULL != (p_buffer = wiced_transport_allocate_buffer(host_trans_pool)))
    {
        p_buffer[0] = status;
        mesh_transport_send_data(HCI_CONTROL_MESH_EVENT_COMMAND_STATUS, p_buffer, 1);
    }
}

uint32_t mesh_application_proc_rx_cmd(uint8_t *p_buffer, uint32_t length)
{
    uint16_t opcode;
    uint16_t payload_len;
    uint8_t *p_data = p_buffer;

    if (p_buffer == NULL)
    {
        return 0;
    }

    // Expected minimum 4 byte as the wiced header
    if (length < 4)
    {
        WICED_BT_TRACE("invalid params\n");
    }
    else
    {
        STREAM_TO_UINT16(opcode, p_data);       // Get OpCode
        STREAM_TO_UINT16(payload_len, p_data);  // Gen Payload Length
#if defined MESH_APPLICATION_MCU_MEMORY || defined PTS
        if (!mesh_application_process_hci_cmd(opcode, p_data, payload_len))
#endif
        {
#ifdef PTS
            WICED_BT_TRACE("***************opcode:  %x\n", opcode);
            WICED_BT_TRACE("***************len:     %d\n", payload_len);
            WICED_BT_TRACE_ARRAY(p_data, length - 4, "***************payload: ");
            if (opcode == HCI_CONTROL_COMMAND_RESET)
            {
                wiced_hal_wdog_reset_system();
            }
            else if (!wiced_bt_mesh_core_test_mode_signal(opcode, p_data, payload_len))
            {
#else
            if ((opcode == HCI_CONTROL_MESH_COMMAND_CORE_SET_SEQ) ||
                (opcode == HCI_CONTROL_MESH_COMMAND_CORE_DEL_SEQ))
            {
                wiced_bt_mesh_core_proc_rx_cmd(opcode, p_data, payload_len);
            }
            else if (opcode == HCI_CONTROL_MESH_COMMAND_TRACE_CORE_SET)
            {
                wiced_bt_mesh_core_set_trace_level(p_data[1] + (p_data[2] << 8) + (p_data[3] << 16) + (p_data[4] << 24), p_data[0]);
            }
            else if (opcode == HCI_CONTROL_MESH_COMMAND_TRACE_MODELS_SET)
            {
                wiced_bt_mesh_models_set_trace_level(p_data[0]);
            }
            else
            {
#endif
#if defined HCI_CONTROL
                if (opcode == HCI_CONTROL_MESH_COMMAND_SET_RAW_ACCESS_LAYER_MODE)
                {
                    mesh_app_set_raw_access_layer_mode_handler(p_data[0] == 0 ? NULL : mesh_app_raw_access_layer_mode_handler);
                }
                else if (opcode == HCI_CONTROL_MESH_COMMAND_SET_RAW_ACCESS_LAYER_CLNT_DEVKEY)
                {
                    mesh_app_on_set_raw_access_layer_clnt_devkey(p_data, payload_len);
                }
                else if (opcode == HCI_CONTROL_MESH_COMMAND_SEND_RAW_ACCESS_LAYER_PDU)
                {
                    mesh_app_on_send_raw_access_layer_pdu(p_data, payload_len);
                }
                else if (opcode == HCI_CONTROL_MESH_COMMAND_CANCEL_RAW_ACCESS_LAYER_PDU)
                {
                    mesh_app_cancel_raw_access_layer_pdu();
                }
                else
#endif
                {
                    if (opcode == HCI_CONTROL_MESH_COMMAND_SET_LOCAL_DEVICE)
                    {
                        mesh_app_set_local_device(p_data, payload_len);
                    }
                    else if (wiced_bt_mesh_app_func_table.p_mesh_app_proc_rx_cmd)
                    {
                        wiced_bt_mesh_app_func_table.p_mesh_app_proc_rx_cmd(opcode, p_data, payload_len);
                    }
                }
            }
        }
    }
    wiced_transport_free_buffer(p_buffer);
    return 0;
}

/*
 * This function can be used to process WICED HCI header common all WICED HCI commands and create corresponing mesh event
 */
wiced_bt_mesh_event_t *wiced_bt_mesh_create_event_from_wiced_hci(uint16_t opcode, uint16_t company_id, uint16_t model_id, uint8_t **p_data, uint32_t *len)
{
    uint16_t dst;
    uint16_t app_key_idx;
    uint8_t  element_idx, ttl;
    int i;
    uint8_t  *p = *p_data;
    wiced_bt_mesh_event_t *p_event;

    STREAM_TO_UINT16(dst, p);
    STREAM_TO_UINT16(app_key_idx, p);
    STREAM_TO_UINT8(element_idx, p);

    *len = *len - 5;

    p_event = wiced_bt_mesh_create_event(element_idx, company_id, model_id, dst, app_key_idx);
    if (p_event != NULL)
    {
        p_event->hci_opcode = opcode;

        STREAM_TO_UINT8(p_event->reply, p);
        STREAM_TO_UINT8(p_event->send_segmented, p);

        STREAM_TO_UINT8(ttl, p);
        if(ttl <= 0x7F)
            p_event->ttl = ttl;

        STREAM_TO_UINT8(p_event->retrans_cnt, p);
        STREAM_TO_UINT8(p_event->retrans_time, p);
        STREAM_TO_UINT8(p_event->reply_timeout, p);

        *len = *len - 6;
    }
    *p_data = p;
    return p_event;
}

/*
 * This function can be used to skip mesh WICED HCI header
 */
void wiced_bt_mesh_skip_wiced_hci_hdr(uint8_t **p_data, uint32_t *len)
{
    // skip dst (2), app_key_idx(2), element_idx(1), reply(1), send_segmented(1), ttl(1), retrans_cnt(1), retrans_time(1), reply_timeout(1)
    *len = *len - 11;
    *p_data = (*p_data + 11);
}

/*
 * This function can be used to parse WICED HCI header and extract needed info
 */
uint8_t wiced_bt_mesh_get_element_idx_from_wiced_hci(uint8_t **p_data, uint32_t *len)
{
    uint8_t  element_idx;
    int i;
    uint8_t  *p = *p_data;
    wiced_bt_mesh_event_t *p_event;

    p += 4;  // skip dst(2) and app_key_idx(2)
    STREAM_TO_UINT8(element_idx, p);
    p += 6;             // reply(1), send_segmented(1), ttl(1), retrans_cnt(1), retrans_time(1), reply_timeout(1)
    *len = *len - 11;
    *p_data = p;
    return element_idx;
}

/*
 * This function can be used to send TX Complete event to the MCU
 */
void wiced_bt_mesh_send_hci_tx_complete(wiced_bt_mesh_hci_event_t *p_hci_event, wiced_bt_mesh_event_t *p_event)
{
    uint8_t *p = p_hci_event->data;

    UINT16_TO_STREAM(p, p_event->hci_opcode);
    UINT8_TO_STREAM(p, p_event->status.tx_flag);
    UINT16_TO_STREAM(p, p_event->dst);
    mesh_transport_send_data(HCI_CONTROL_MESH_EVENT_TX_COMPLETE, (uint8_t *)p_hci_event, (uint16_t)(p - (uint8_t *)p_hci_event));
}

void mesh_app_hci_send_seq_changed(wiced_bt_mesh_core_state_seq_t *p_seq)
{
    uint8_t *p_hci_event;
    uint8_t *p;
    p_hci_event = wiced_transport_allocate_buffer(host_trans_pool);
    if (p_hci_event != NULL)
    {
        p = p_hci_event;
        UINT16_TO_STREAM(p, p_seq->addr);
        UINT32_TO_STREAM(p, p_seq->seq);
        UINT8_TO_STREAM(p, p_seq->previous_iv_idx);
        UINT16_TO_STREAM(p, p_seq->rpl_entry_idx);
        mesh_transport_send_data(HCI_CONTROL_MESH_EVENT_CORE_SEQ_CHANGED, p_hci_event, (uint16_t)(p - p_hci_event));
    }
}

wiced_result_t mesh_application_send_hci_event(uint16_t opcode, const uint8_t *p_data, uint16_t data_len)
{
    uint8_t                *p_hci_event;
    p_hci_event = wiced_transport_allocate_buffer(host_trans_pool);
    if (p_hci_event != NULL)
    {
        if (data_len != 0)
            memcpy(p_hci_event, p_data, data_len);
        mesh_transport_send_data(opcode, p_hci_event, data_len);
    }
    return WICED_BT_SUCCESS;
}

typedef struct
{
    void        *next;
    uint16_t    id;
    uint16_t    len;
    uint8_t     data[1];
} mesh_nvram_block_t;

mesh_nvram_block_t  *mesh_application_mcu_memory_first_block = NULL;

mesh_nvram_block_t *mesh_application_mcu_memory_find(uint16_t id)
{
    mesh_nvram_block_t  *p;
    for (p = mesh_application_mcu_memory_first_block; p != NULL; p = (mesh_nvram_block_t*)p->next)
    {
        if (p->id == id)
            break;
    }
    return p;
}

uint16_t mesh_application_mcu_memory_read(uint16_t id, uint16_t buf_len, uint8_t *p_buf)
{
    mesh_nvram_block_t  *p;
    if ((p = mesh_application_mcu_memory_find(id)) == NULL)
    {
        WICED_BT_TRACE("mcu_memory_read: no NVRAM id:%d\n", id);
        return 0;
    }
    WICED_BT_TRACE("mcu_memory_read: id:%d buf_len:%d len:%d\n", id, buf_len, p->len);
    if (buf_len > p->len)
        buf_len = p->len;
    memcpy(p_buf, p->data, buf_len);
    return buf_len;
}

uint16_t mesh_application_mcu_memory_write(uint16_t id, uint16_t data_len, const uint8_t *p_data)
{
    uint8_t buf[257];// data_len can't be > 255. 0 data_len means delete NVRAM ID
    WICED_BT_TRACE("mcu_memory_write: id:%d data_len:%d\n", id, data_len);
    buf[0] = (uint8_t)id;
    buf[1] = (uint8_t)(id >> 8);
    if(data_len)
        memcpy(&buf[2], p_data, data_len);
    mesh_application_send_hci_event(HCI_CONTROL_MESH_EVENT_NVRAM_DATA, buf, data_len + 2);
    return data_len;
}

void command_complete_callback(wiced_bt_dev_vendor_specific_command_complete_params_t *p_command_complete_params)
{
    uint8_t buf[257];

    WICED_BT_TRACE("HCI complete callback: opcode:%04x data_len:%d\n", p_command_complete_params->opcode, p_command_complete_params->param_len);

    buf[0] = (uint8_t)p_command_complete_params->opcode;
    buf[1] = (uint8_t)(p_command_complete_params->opcode >> 8);
    buf[2] = p_command_complete_params->param_len;
    memcpy(&buf[3], p_command_complete_params->p_param_buf, p_command_complete_params->param_len);
    mesh_application_send_hci_event(HCI_CONTROL_TEST_EVENT_ENCAPSULATED_HCI_EVENT, buf, p_command_complete_params->param_len + 3);
}

wiced_result_t mesh_application_process_hci_cmd(uint16_t opcode, const uint8_t *p_data, uint16_t data_len)
{
    wiced_result_t      ret = WICED_TRUE;
    uint16_t            id;
    mesh_nvram_block_t  *p;

    switch (opcode)
    {
    case HCI_CONTROL_TEST_COMMAND_ENCAPSULATED_HCI_COMMAND:
        if ((data_len < 3) || (data_len - 3 != p_data[2]))
        {
            WICED_BT_TRACE("hci_cmd: bad len %d\n", data_len);
            break;
        }
        id = (uint16_t)p_data[0] | (((uint16_t)p_data[1]) << 8);
        ret = wiced_bt_dev_vendor_specific_command(id, data_len - 3, (uint8_t *)&p_data[3], command_complete_callback);
        break;

    case HCI_CONTROL_COMMAND_PUSH_NVRAM_DATA:
        // make sure it contains NVRAM ID and at least one byte of data
        if (data_len <= 2)
        {
            WICED_BT_TRACE("mesh_application_process_hci_cmd: invalid NVRAM len %d\n", data_len);
            break;
        }
        // make sure NVRAM ID is correct
        id = (uint16_t)p_data[0] | (((uint16_t)p_data[1]) << 8);
        p_data += 2;
        data_len -= 2;

        // make sure this MVRAM ID hasn't been pushed already
        if (NULL != mesh_application_mcu_memory_find(id))
        {
            WICED_BT_TRACE("mesh_application_process_hci_cmd: already pushed NVRAM id %d\n", id);
            break;
        }
        // allocate buffer for that NVRAM ID and place it to the queue
        if ((p = (mesh_nvram_block_t*)wiced_bt_get_buffer(sizeof(mesh_nvram_block_t) + data_len)) == NULL)
        {
            WICED_BT_TRACE("mesh_application_process_hci_cmd: get_buffer(%d) failed\n", sizeof(mesh_nvram_block_t) + data_len);
            break;
        }
        WICED_BT_TRACE("mesh_application_process_hci_cmd: id:%d len:%d\n", id, data_len);
        p->next = mesh_application_mcu_memory_first_block;
        p->id = id;
        p->len = data_len;
        memcpy(p->data, p_data, data_len);
        mesh_application_mcu_memory_first_block = p;
        break;

    case HCI_CONTROL_MESH_COMMAND_APP_START:
        // Start application and then delete all allocated NVRAM chunks
        mesh_application_init();
        while (mesh_application_mcu_memory_first_block != NULL)
        {

            p = mesh_application_mcu_memory_first_block;
            mesh_application_mcu_memory_first_block = (mesh_nvram_block_t*)p->next;
            wiced_bt_free_buffer(p);
        }
        break;

    case HCI_CONTROL_MESH_COMMAND_GATEWAY_CONN_STATUS:
        WICED_BT_TRACE("HCI_CONTROL_MESH_COMMAND_GATEWAY_CONN_STATUS\n");
        wiced_bt_mesh_core_connection_status(p_data[0] + (p_data[1] << 8), p_data[2], 0, 75);
        break;

    case HCI_CONTROL_MESH_COMMAND_SEND_PROXY_DATA:
        WICED_BT_TRACE("SEND_PROXY_DATA\n");
        wiced_bt_mesh_core_proxy_packet(p_data, data_len);
        break;

#ifdef MESH_DFU_OOB_SUPPORTED
    case HCI_CONTROL_MESH_COMMAND_FW_DISTRIBUTION_UPLOAD_STARTED:
        {
            wiced_bt_mesh_fw_upload_start_t upload_start;

            if (data_len == 4)
            {
                STREAM_TO_UINT32(upload_start.fw_size, p_data);
                wiced_bt_mesh_model_fw_upload_event_handler(WICED_BT_MESH_FW_UPLOAD_START, &upload_start);
            }
            else
                wiced_bt_mesh_model_fw_upload_event_handler(WICED_BT_MESH_FW_UPLOAD_START, NULL);
        }
        break;

    case HCI_CONTROL_MESH_COMMAND_FW_DISTRIBUTION_UPLOAD_FWID:
        {
            mesh_dfu_fw_id_t fw_id;

            fw_id.fw_id_len = data_len;
            memcpy(fw_id.fw_id, p_data, data_len);

            wiced_bt_mesh_model_fw_upload_event_handler(WICED_BT_MESH_FW_UPLOAD_FW_ID, &fw_id);
        }
        break;

    case HCI_CONTROL_MESH_COMMAND_FW_DISTRIBUTION_UPLOAD_METADATA:
        {
            mesh_dfu_metadata_t metadata;

            metadata.len = data_len;
            memcpy(metadata.data, p_data, data_len);

            wiced_bt_mesh_model_fw_upload_event_handler(WICED_BT_MESH_FW_UPLOAD_METADATA, &metadata);
        }
        break;

    case HCI_CONTROL_MESH_COMMAND_FW_DISTRIBUTION_UPLOAD_DATA:
        {
            wiced_bt_mesh_fw_upload_data_t upload_data;
            uint8_t status = 0;

            STREAM_TO_UINT32(upload_data.offset, p_data);
            upload_data.data_len = data_len - 4;
            upload_data.p_data = (uint8_t *)p_data + 4;
            wiced_bt_mesh_model_fw_upload_event_handler(WICED_BT_MESH_FW_UPLOAD_DATA, &upload_data);
            mesh_application_send_hci_event(HCI_CONTROL_MESH_EVENT_COMMAND_STATUS, &status, 1);
        }
        break;

    case HCI_CONTROL_MESH_COMMAND_FW_DISTRIBUTION_UPLOAD_PROGRESS:
        {
            wiced_bt_mesh_fw_upload_progress_t upload_progress;

            STREAM_TO_UINT8(upload_progress.progress, p_data);
            wiced_bt_mesh_model_fw_upload_event_handler(WICED_BT_MESH_FW_UPLOAD_PROGRESS, &upload_progress);
        }
        break;

    case HCI_CONTROL_MESH_COMMAND_FW_DISTRIBUTION_UPLOAD_FINISH:
        {
            wiced_bt_mesh_fw_upload_finish_t upload_finish;

            STREAM_TO_UINT8(upload_finish.result, p_data);
            wiced_bt_mesh_model_fw_upload_event_handler(WICED_BT_MESH_FW_UPLOAD_FINISH, &upload_finish);
        }
        break;
#endif

    default:
        ret = WICED_FALSE;
    }
    return ret;
}


wiced_bool_t mesh_app_raw_access_layer_mode_handler(wiced_bt_mesh_event_t* p_event, const uint8_t* params, uint16_t params_len)
{
    wiced_bool_t    res = WICED_FALSE;
    uint8_t         *p_hci_event;
    uint8_t         *p;
    WICED_BT_TRACE("raw_access_layer_mode_handler: ttl:%x element_idx:%x src:%x app_key_idx:%x company_id:%x opcode:%x params_len:%d\n",
        p_event->ttl, p_event->element_idx, p_event->src, p_event->app_key_idx, p_event->company_id, p_event->opcode, params_len);

    if (NULL == (p_hci_event = wiced_transport_allocate_buffer(host_trans_pool)))
    {
        WICED_BT_TRACE(" wiced_transport_allocate_buffer failed\n");
    }
    else
    {
        p = p_hci_event;
//        UINT8_TO_STREAM(p, p_event->flags);
        UINT8_TO_STREAM(p, 0);
        UINT8_TO_STREAM(p, p_event->rssi);
        UINT8_TO_STREAM(p, p_event->ttl);
        UINT8_TO_STREAM(p, p_event->element_idx);
        UINT16_TO_STREAM(p, p_event->src);
        UINT16_TO_STREAM(p, p_event->app_key_idx);
        UINT16_TO_STREAM(p, p_event->company_id);
        UINT16_TO_STREAM(p, p_event->opcode);
        memcpy(p, params, params_len);
        p += params_len;
        if (WICED_BT_SUCCESS == mesh_transport_send_data(HCI_CONTROL_MESH_EVENT_RAW_ACCESS_LAYER_PDU, p_hci_event, (uint16_t)(p - p_hci_event)))
            res = WICED_TRUE;
    }
    return res;
}

void mesh_app_on_send_raw_access_layer_pdu(uint8_t* p_data, uint16_t len)
{
    uint8_t     flags, ttl, element_idx, retrans_cnt, retrans_interval;
    uint16_t    addr, app_key_idx, company_id, model_id, opcode;
    uint8_t     *p = p_data;
    //WICED_BT_TRACE("mesh_app_on_send_raw_access_layer_pdu: len:%d\n", len);
    if (len >= 13)
    {
        STREAM_TO_UINT8(flags, p);
        STREAM_TO_UINT8(ttl, p);
        STREAM_TO_UINT8(element_idx, p);
        STREAM_TO_UINT16(addr, p);
        STREAM_TO_UINT16(app_key_idx, p);
        STREAM_TO_UINT16(company_id, p);
        STREAM_TO_UINT16(model_id, p);
        STREAM_TO_UINT16(opcode, p);
        STREAM_TO_UINT8(retrans_cnt, p);
        STREAM_TO_UINT8(retrans_interval, p);
        mesh_app_send_raw_access_layer_pdu(flags, ttl, element_idx, addr, app_key_idx, company_id, model_id, opcode, retrans_cnt, retrans_interval, p, p_data + len - p);
    }
}

void mesh_app_on_set_raw_access_layer_clnt_devkey(uint8_t* p_data, uint16_t len)
{
    uint16_t    addr, netkey_idx;
    if (len == 20)
    {
        STREAM_TO_UINT16(addr, p_data);
        STREAM_TO_UINT16(netkey_idx, p_data);
        mesh_app_set_raw_access_layer_clnt_devkey(addr, netkey_idx, p_data);
    }
}
#else

wiced_result_t mesh_transport_send_data(uint16_t opcode, uint8_t *p_trans_buf, uint16_t data_len)
{
    wiced_result_t result = wiced_transport_send_buffer(opcode, p_trans_buf, data_len);
    if (result != WICED_BT_SUCCESS)
    {
        WICED_BT_TRACE("transport send buffer:%d\n", result);
    }
    return result;
}

uint32_t mesh_application_proc_rx_cmd(uint8_t *p_buffer, uint32_t length)
{
    #include "wiced_rtos.h"

    uint16_t opcode;
    uint16_t payload_len;
    uint8_t *p_data = p_buffer;

    if (p_buffer == NULL)
    {
        return 0;
    }

    // Expected minimum 4 byte as the wiced header
    if (length < 4)
    {
        WICED_BT_TRACE("invalid params\n");
    }
    else
    {
        STREAM_TO_UINT16(opcode, p_data);       // Get OpCode
        STREAM_TO_UINT16(payload_len, p_data);  // Gen Payload Length
        if(opcode == HCI_CONTROL_ANDON_EVENT_WRITEDID)
        {

            extern uint32_t flash_app_write_mem(uint32_t addr, uint8_t *data, uint32_t len);
            extern uint32_t flash_app_read_mem(uint32_t addr, uint8_t *data, uint32_t len);
            uint32_t spiaddr;
            uint8_t *data_temp;
            uint8_t read_data[100];

            data_temp = p_data;
            STREAM_TO_UINT32(spiaddr, p_data);      // Get addr
            STREAM_TO_UINT16(payload_len, p_data);  // Get did length
            WICED_BT_TRACE("spiaddr:%08x payload_len:%d!!!!!\n",spiaddr,payload_len);
            if(payload_len == flash_app_write_mem(spiaddr,p_data,payload_len))
            {
                WICED_BT_TRACE("write DID ok !!!!!\n");
                wiced_hal_wdog_restart();
                wiced_rtos_delay_milliseconds(200, KEEP_THREAD_ACTIVE);
                WICED_BT_TRACE("flash_read spiaddr:%08x payload_len:%d!!!!!\n",spiaddr,payload_len);
                payload_len = flash_app_read_mem(spiaddr,read_data,payload_len);
                WICED_BT_TRACE("flash_read spiaddr:%08x payload_len:%d!!!!!\n",spiaddr,payload_len);
                wiced_rtos_delay_milliseconds(20, KEEP_THREAD_ACTIVE);
                memcpy(p_data,p_data,payload_len);
            }
            else
            {
                WICED_BT_TRACE("write DID err!!!!!\n");
                payload_len = 0;
            }
            mesh_transport_send_data(HCI_CONTROL_ANDON_EVENT_WRITEDID,data_temp,payload_len+6);
        }
        else if(HCI_CONTROL_ANDON_EVENT_RESET == opcode)
        {
            mesh_transport_send_data(HCI_CONTROL_ANDON_EVENT_WRITEDID,p_data,payload_len);
            wiced_rtos_delay_milliseconds(200, KEEP_THREAD_ACTIVE);
            WICED_BT_TRACE("write Reset cmd!!!!!\n");
            wdog_generate_hw_reset();
            while(1);
        }
    }
    wiced_transport_free_buffer(p_buffer);
    return 0;
}
#endif
