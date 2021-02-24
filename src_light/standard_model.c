#include "config.h"
#include "standard_model.h"

#if USE_STANDARD_MODEL
wiced_bool_t mesh_lightness_message_handler(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint16_t data_len)
{
    LOG_VERBOSE("mesh_lightness_message_handler: opcode:%x model_id:%x\n", p_event->opcode, p_event->model_id);
  
    // 0xffff model_id means request to check if that opcode belongs to that model
    if (p_event->model_id == 0xffff)
    {
        switch (p_event->opcode)
        {
        case WICED_BT_MESH_OPCODE_GEN_ONOFF_GET:
        case WICED_BT_MESH_OPCODE_GEN_ONOFF_SET:
        case WICED_BT_MESH_OPCODE_GEN_ONOFF_SET_UNACKED:
        case WICED_BT_MESH_OPCODE_GEN_ONOFF_STATUS:
        case WICED_BT_MESH_OPCODE_GEN_LEVEL_GET:
        case WICED_BT_MESH_OPCODE_GEN_LEVEL_SET:
        case WICED_BT_MESH_OPCODE_GEN_LEVEL_SET_UNACKED:
        case WICED_BT_MESH_OPCODE_GEN_LEVEL_STATUS:
        case WICED_BT_MESH_OPCODE_LIGHT_LIGHTNESS_GET:
        case WICED_BT_MESH_OPCODE_LIGHT_LIGHTNESS_SET:
        case WICED_BT_MESH_OPCODE_LIGHT_LIGHTNESS_SET_UNACKED:
        case WICED_BT_MESH_OPCODE_LIGHT_LIGHTNESS_STATUS:
        case WICED_BT_MESH_OPCODE_GEN_LEVEL_DELTA_SET:
        case WICED_BT_MESH_OPCODE_GEN_LEVEL_DELTA_SET_UNACKED:
            break;
        default:
            return WICED_FALSE;
        }
        return WICED_TRUE;
    }
    
}
#endif
