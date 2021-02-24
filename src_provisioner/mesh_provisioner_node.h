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
 * mesh_provisioner_node.h : Header file for mesh vendor specific model.
 *
 */

#ifndef __MESH_PROVISIONER_NODE_H__
#define __MESH_PROVISIONER_NODE_H__

// #define MESH_VENDOR_COMPANY_ID          0x486   // Vendor Company ID
// #define MESH_VENDOR_MODEL_ID            1       // ToDo.  This need to be modified

// // This sample shows simple use of vendor data messages.  Vendor model
// // can define any opcodes it wants.
// #define MESH_VENDOR_OPCODE1             1
// #define MESH_VENDOR_OPCODE2             2


#define MESH_NODE_DEFAULT_TTL            0x3F
#define MESH_NODE_NET_TRANSMIT_COUNT     3
#define MESH_NODE_NET_TRANSMIT_INTERVAL  100
#define MESH_NODE_RELAY_STATE            1
#define MESH_NODE_RELAY_COUNT            3
#define MESH_NODE_RELAY_INTERVAL         100
#define MESH_NODE_PROXY_STATE            1
#define MESH_NODE_FRIEND_STATE           1
#define MESH_NODE_BEACON_STATE           1


enum
{
    MESH_VENDOR_GROUP_ALL = 0,
    MESH_VENDOR_GROUP_PROVISIONER,
    MESH_VENDOR_GROUP_DFU,
    MESH_VENDOR_GROUP_NUM
};

typedef struct
{
    uint16_t    addr;
    const char* name;
} wiced_bt_mesh_vendor_group_t;

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef __cplusplus
}
#endif

#endif  // __MESH_PROVISIONER_NODE_H__
