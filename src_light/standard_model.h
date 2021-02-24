#pragma once

#include "wiced_bt_mesh_models.h"
#include "wiced_bt_mesh_model_defs.h"

#include "config.h"

#if USE_STANDARD_MODEL
#define STANDARD_MODEL WICED_BT_MESH_MODEL_LIGHT_HSL_CTL_SERVER
typedef OperateCB(uint8_t element_idx, uint32_t opcode, uint8_t *parameters, int len);
#else
#define STANDARD_MODEL
#endif
