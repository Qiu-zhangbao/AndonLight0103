/*
* $ Copyright 2016-YEAR Cypress Semiconductor $
 */

/** @file
 *
 * This file shows how to implement self-config functions.
 */
#include "wiced_bt_mesh_app.h"
#include "wiced_bt_mesh_core_extra.h"
#include "wiced_bt_mesh_model_utils.h"
#include "wiced_bt_trace.h"

#ifdef ENABLE_SELF_CONFIG_MODEL

#include "wiced_bt_mesh_self_config.h"

#define ENABLE_MESH_APP_SELF_CONFIG_TRACE


static wiced_bool_t mesh_node_has_model(uint16_t company_id, uint16_t model_id);
static void         mesh_self_config_node(uint8_t *p_data, uint16_t data_len);

// Flag to indicate whether self-config has been done to avoud duplicated messages
static wiced_bool_t  self_configured = WICED_FALSE;

static wiced_bt_mesh_self_config_msg_handler_t      msg_handler_db_export  = NULL;
static wiced_bt_mesh_self_config_msg_handler_t      msg_handler_status     = NULL;
static wiced_bt_mesh_self_config_gatt_cmd_handler_t gatt_cmd_handler       = NULL;;

static uint16_t client_config_descriptor = 0;
static uint16_t gatt_conn_id = 0;

// Initialize the message handlers
wiced_bool_t wiced_bt_mesh_self_config_init(wiced_bt_mesh_self_config_init_t* p_init)
{
    if (p_init != NULL)
    {
        msg_handler_db_export  = p_init->msg_handler_db_export;
        msg_handler_status     = p_init->msg_handler_status;
        gatt_cmd_handler       = p_init->gatt_cmd_handler;
    }
}

// Send a self_config message 
void wiced_bt_mesh_self_config_send_msg(uint16_t dst, uint16_t app_key_index, uint16_t opcode, uint8_t *p_data, uint8_t data_len, uint8_t retrans_cnt, uint8_t retrans_time,  wiced_bt_mesh_event_t **p_out_event, wiced_bt_mesh_core_send_complete_callback_t complete_callback)
{
    wiced_bt_mesh_event_t *p_event;
#ifdef ENABLE_MESH_APP_SELF_CONFIG_TRACE
    const char *str = "self_cfg_msg_tx: ";
#endif

    p_event = wiced_bt_mesh_create_event(0, MESH_COMPANY_ID_CYPRESS, WICED_BT_MESH_MODEL_ID_SELF_CONFIG, dst, app_key_index);
    if (p_event == NULL)
    {
#ifdef ENABLE_MESH_APP_SELF_CONFIG_TRACE
        WICED_BT_TRACE("%sno mem\n", str);
#endif
        return;
    }
    p_event->retrans_cnt  = retrans_cnt;
    p_event->retrans_time = retrans_time;

#ifdef ENABLE_MESH_APP_SELF_CONFIG_TRACE
    WICED_BT_TRACE("%sdst:%04X opcode:%d len:%d\n", str, dst, opcode, data_len);
#endif

    wiced_bt_mesh_models_utils_send(p_event, p_out_event, WICED_TRUE, opcode, p_data, data_len, complete_callback);
}

// Send reply status message
void wiced_bt_mesh_self_config_send_reply(wiced_bt_mesh_event_t *p_event, uint16_t opcode, uint8_t *p_data, uint16_t data_len)
{
#ifdef ENABLE_MESH_APP_SELF_CONFIG_TRACE
    WICED_BT_TRACE("self_cfg_status_msg_tx: dst:%04X len:%d\n", p_event->src, data_len);
#endif
    wiced_bt_mesh_models_utils_send(wiced_bt_mesh_create_reply_event(p_event), NULL, WICED_FALSE, opcode, p_data, data_len, NULL);
}

// Handle self-config model messages
wiced_bool_t wiced_bt_mesh_self_config_message_handler(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint16_t data_len)
{
    uint8_t buf[8];
    uint8_t *p = buf;

    if (p_event->model_id == 0xFFFF)
    {
        // Need list all the opcodes for this company here!
        if (p_event->opcode <= WICED_BT_MESH_SELF_CONFIG_OPCODE_STATUS)
        {
            return WICED_TRUE;
        }
        return WICED_FALSE;
    }

#ifdef ENABLE_MESH_APP_SELF_CONFIG_TRACE
    WICED_BT_TRACE("cfg_msg: src:%04X dst:%04X cid:%04X model:%X opcode:%d len:%d code:%d\n", p_event->src, p_event->dst, p_event->company_id, p_event->model_id, p_event->opcode, data_len, p_data[0]);
    WICED_BT_TRACE_ARRAY(p_data, data_len, "");
#endif

    if (wiced_bt_mesh_core_get_local_addr() != p_event->src)
    {
        switch (p_event->opcode) {
        case WICED_BT_MESH_SELF_CONFIG_OPCODE_SELF_CONFIG:
            if (p_event->dst == wiced_bt_mesh_core_get_local_addr())
            {
                mesh_self_config_node(p_data, data_len);

                UINT8_TO_STREAM(p, 0);
                wiced_bt_mesh_self_config_send_reply(p_event, WICED_BT_MESH_SELF_CONFIG_OPCODE_STATUS, buf, (uint16_t)(p - buf));
                // The event is reused for reply, hence shall not be released and just return
                return WICED_TRUE;
            }
            break;
        // case WICED_BT_MESH_SELF_CONFIG_OPCODE_DB_EXPORT:
        //     if (msg_handler_db_export != NULL)
        //     {
        //         (*msg_handler_db_export)(p_event, p_data, data_len);
        //         // The event object is either used for reply, or released in handler.
        //         // Hence shall not be released again and just return
        //         return WICED_TRUE;
        //     }
        //     break;
        // case WICED_BT_MESH_SELF_CONFIG_OPCODE_STATUS:
        //     if (msg_handler_status != NULL)
        //     {
        //         (*msg_handler_status)(p_event, p_data, data_len);
        //     }
        //     break;
        default:
            break;
        }
    }

    wiced_bt_mesh_release_event(p_event);
    return WICED_TRUE;
}

void wiced_bt_mesh_self_config_key_bind(uint16_t app_key_index)
{
#ifdef ENABLE_MESH_APP_SELF_CONFIG_TRACE
    WICED_BT_TRACE("self_cfg_app_key_bind: idx:%04X\n", app_key_index);
#endif
    if (mesh_node_has_model(MESH_COMPANY_ID_CYPRESS, WICED_BT_MESH_MODEL_ID_SELF_CONFIG))
    {
        // Self-config to bind the key with models so that the node can receive
        // model messages
        wiced_bt_mesh_config_model_app_key_bind(app_key_index, WICED_TRUE);
    }
}

// Check whether the node has the specific model
static wiced_bool_t mesh_node_has_model(uint16_t company_id, uint16_t model_id)
{
    int i, j   ;
    for (i = 0; i < mesh_config.elements_num; i++)
    {
        for (j = 0; j < mesh_config.elements[i].models_num; j++)
        {
            if (mesh_config.elements[i].models[j].company_id == company_id &&
                mesh_config.elements[i].models[j].model_id == model_id)
            {
                return WICED_TRUE;
            }
        }
    }
    return WICED_FALSE;
}

// Self-Configure the node
static void mesh_self_config_node(uint8_t *p_data, uint16_t data_len)
{
    int       i;
    uint8_t   state;
    uint8_t   group_num;
    uint16_t  group_addr;
    uint16_t  app_key_index;

    if (self_configured || !mesh_node_has_model(MESH_COMPANY_ID_CYPRESS, WICED_BT_MESH_MODEL_ID_SELF_CONFIG))
    {
        // The device is configured or does not support self-configuration
        return;
    }
    self_configured = WICED_TRUE;

    STREAM_TO_UINT8(group_num, p_data);
    STREAM_TO_UINT16(app_key_index, p_data);

#ifdef ENABLE_MESH_APP_SELF_CONFIG_TRACE
    WICED_BT_TRACE("self_cfg: group_num:%d app_key_idx:%04X\n", group_num);
#endif
    
    // Set default network transmit parameters
    wiced_bt_mesh_config_net_transmit(WICED_BT_MESH_SELF_CONFIG_DEFAULT_NET_TRANSMIT_COUNT, WICED_BT_MESH_SELF_CONFIG_DEFAULT_NET_TRANSMIT_INTERVAL, WICED_FALSE);

    // Set default relay retransmit parameters
    if ((mesh_config.features & WICED_BT_MESH_CORE_FEATURE_BIT_RELAY) != 0)
    {
        state = ((mesh_config.features & WICED_BT_MESH_CORE_FEATURE_BIT_LOW_POWER) == WICED_BT_MESH_CORE_FEATURE_BIT_LOW_POWER) ? 0 : WICED_BT_MESH_SELF_CONFIG_DEFAULT_RELAY_STATE;
        wiced_bt_mesh_config_relay(state, WICED_BT_MESH_SELF_CONFIG_DEFAULT_RELAY_COUNT, WICED_BT_MESH_SELF_CONFIG_DEFAULT_RELAY_INTERVAL, WICED_FALSE);
    }

    // Set default proxy server state
    if ((mesh_config.features & WICED_BT_MESH_CORE_FEATURE_BIT_GATT_PROXY_SERVER) != 0)
    {
        state = ((mesh_config.features & WICED_BT_MESH_CORE_FEATURE_BIT_LOW_POWER) == WICED_BT_MESH_CORE_FEATURE_BIT_LOW_POWER) ? 0 : WICED_BT_MESH_SELF_CONFIG_DEFAULT_PROXY_STATE;
        wiced_bt_mesh_config_proxy(state, WICED_FALSE);
    }

    // Set default friend node state
    if ((mesh_config.features & WICED_BT_MESH_CORE_FEATURE_BIT_FRIEND) != 0)
    {
        state = ((mesh_config.features & WICED_BT_MESH_CORE_FEATURE_BIT_LOW_POWER) == WICED_BT_MESH_CORE_FEATURE_BIT_LOW_POWER) ? 0 : WICED_BT_MESH_SELF_CONFIG_DEFAULT_FRIEND_STATE;
        wiced_bt_mesh_config_friend(state, WICED_FALSE);
    }

    // Set default beacon state
    state = ((mesh_config.features & WICED_BT_MESH_CORE_FEATURE_BIT_LOW_POWER) == WICED_BT_MESH_CORE_FEATURE_BIT_LOW_POWER) ? 0 : WICED_BT_MESH_SELF_CONFIG_DEFAULT_BEACON_STATE;
    wiced_bt_mesh_config_beacon(state, WICED_FALSE);

    // Set default TTL
    wiced_bt_mesh_config_default_ttl(WICED_BT_MESH_SELF_CONFIG_DEFAULT_TTL, WICED_TRUE);

    // Set the subscriptions and publication
    for (i = 0; i < group_num; i++)
    {
        STREAM_TO_UINT16(group_addr, p_data);
    
        if (i == 0)
        {
            wiced_bt_mesh_config_model_pub(group_addr, app_key_index,
                        WICED_BT_MESH_SELF_CONFIG_DEFAULT_PUB_PERIOD,
                        WICED_BT_MESH_SELF_CONFIG_DEFAULT_PUB_TTL,
                        WICED_BT_MESH_SELF_CONFIG_DEFAULT_PUB_XMIT_COUNT,
                        WICED_BT_MESH_SELF_CONFIG_DEFAULT_PUB_XMIT_INTERVAL,
                        WICED_BT_MESH_SELF_CONFIG_DEFAULT_PUB_CREDENTIAL_FLAG,
                        WICED_FALSE);
        }
        wiced_bt_mesh_config_model_sub(group_addr, (i == (group_num - 1)) ? WICED_TRUE :  WICED_FALSE);
    }
    wiced_bt_mesh_app_set_configured();
}

#endif  // ENABLE_SELF_CONFIG_MODEL
