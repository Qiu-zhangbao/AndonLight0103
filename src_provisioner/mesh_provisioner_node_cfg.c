#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <stdlib.h>

#include "wiced_bt_trace.h"
#include "wiced_bt_mesh_model_defs.h"
#include "wiced_bt_mesh_models.h"
#include "wiced_bt_mesh_provision.h"
#include "wiced_bt_mesh_client.h"
#include "wiced_bt_mesh_db.h"

typedef struct
{
    char *name;
    uint16_t index;
    uint8_t  key[WICED_MESH_DB_KEY_SIZE];
    uint8_t  phase;
    uint8_t  min_security;
} wiced_bt_mesh_cfg_net_key_t;

typedef struct
{
    char *name;
    uint16_t index;
    uint16_t bound_net_key_index;
    uint8_t  key[WICED_MESH_DB_KEY_SIZE];
} wiced_bt_mesh_cfg_app_key_t;

typedef struct
{
    char *name;
    uint8_t uuid[WICED_MESH_DB_UUID_SIZE];
    uint32_t timestamp;
    uint32_t iv_index;
    uint8_t iv_update;
    uint8_t num_net_keys;
    wiced_bt_mesh_cfg_net_key_t *net_key;
    uint8_t num_app_keys;
    wiced_bt_mesh_cfg_app_key_t *app_key;
    uint8_t num_provisioners;
    wiced_bt_mesh_db_provisioner_t *provisioner;
    uint16_t unicast_addr;
    uint16_t num_nodes;
    wiced_bt_mesh_db_node_t *node;
    uint16_t num_groups;
    wiced_bt_mesh_db_group_t *group;
    uint16_t num_scenes;
    wiced_bt_mesh_db_scene_t *scene;
} wiced_bt_mesh_cfg_t;

wiced_bt_mesh_cfg_t provisioner_mesh_cfg;

// send cfg to gateway via SPI or UART
void mesh_provisioner_node_send_cfg(wiced_bt_mesh_cfg_t *mesh_cfg)
{
}

// receive cfg from gateway via SPI or UART
void mesh_provisioner_node_receive_cfg(wiced_bt_mesh_cfg_t *mesh_cfg)
{

}

// Init local mesh database use cfg from gateway
void mesh_provisioner_init_mesh_db_use_cfg(wiced_bt_mesh_cfg_t *mesh_cfg)
{

}
