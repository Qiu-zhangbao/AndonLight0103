#pragma once

#include "stdint.h"
#include "wiced_bt_mesh_models.h"
#include "wiced_bt_mesh_provision.h"
#include "wiced_bt_gatt.h"

#include "config.h"

#define VENDOR_CMD_RPL_SIZE (30)
#define CMD_TIMEOUT         (7) //real ticks number 4*1 = 7s

// This sample shows simple use of vendor get/set/status messages.  Vendor model
// can define any opcodes it wants.
#define MESH_VENDOR_ELEMENT_INDEX       0
#define MESH_VENDOR_OPCODE_GET          1         // Command to Get data   
#define MESH_VENDOR_OPCODE_SET          2         // Command to Set data ack is required
#define MESH_VENDOR_OPCODE_SET_UNACKED  3         // Command to Set data no ack is required
#define MESH_VENDOR_OPCODE_STATUS       4         // Response from the server, or unsolicited data change.


#define _countof(array) (sizeof(array) / sizeof(array[0]))

typedef struct
{
    union {
        struct
        {
            uint16_t src;
            // uint8_t action;
            uint8_t cno;
            // uint8_t resource;
        };
        uint32_t hash;
    };
    uint32_t arrival_time;
} VendorCmdRPLItem;



typedef struct
{
    uint8_t Auth;
    uint8_t CNO;
    union {
        struct
        {
            uint8_t Action;
            uint8_t Resource;
            uint8_t parameters[0];
        };
        uint8_t obfs_stream[0];
    };
} Payload;


extern void OnVendorPktArrive(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint16_t data_len);
extern void vendorSendUnproDevInfoToSuperNode(uint8_t *pdata,uint16_t len,uint16_t dst);
extern void vendorSendReqDBVersion(void);
extern void vendorSendDevStatus(void);
extern void vendorSendDevCountDownStatus(void);
extern void vendorSendDevResetStatus(void);
extern void vendorSendStatusDB(void);
extern void vendorStopDBSync(void);
extern void InitVendor();




