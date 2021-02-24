//******************************************************************************
//*
//* �� �� �� : mesh_vendor_server.h
//* �ļ����� :
//* ��    �� : zhw/Andon Health CO.LTD
//* ��    �� : V0.0
//* ��    �� :
//* �����б� : ��.c
//* ������ʷ : ��.c
//******************************************************************************
#ifndef __MESH_VENDOR_SERVER_H__
#define __MESH_VENDOR_SERVER_H__
///*****************************************************************************
///*                         �����ļ�˵��
///*****************************************************************************

///*****************************************************************************
///*                         �궨����
///*****************************************************************************
//#define MESH_VENDOR_COMPANY_ID          0x804   // Cypress Company ID
//#define MESH_VENDOR_MODEL_ID            1       // ToDo.  This need to be modified

// This sample shows simple use of vendor get/set/status messages.  Vendor model
// can define any opcodes it wants.
#define MESH_VENDOR_OPCODE_GET          1       // Command to Get data
#define MESH_VENDOR_OPCODE_SET          2       // Command to Set data ack is required
#define MESH_VENDOR_OPCODE_SET_UNACKED  3       // Command to Set data no ack is required
#define MESH_VENDOR_OPCODE_STATUS       4       // Response from the server, or unsolicited data change.

#define VENDOR_DELTABRIGHTNESS_CMD   0x0302
#define VENDOR_SETONOFF_CMD          0x0201
#define VENDOR_SETTOGGLEONOFF_CMD    0x0401
#define VENDOR_SETREMOTEACTION_CMD   0x020B

#define VENDOR_ONOFF_ON                      ((uint8_t)(1))
#define VENDOR_ONOFF_OFF                     ((uint8_t)(0))
#define VENDOR_DELTABRIGHTNESSPLUS_STEP      ((uint8_t)(20))
#define VENDOR_DELTABRIGHTNESSLESS_STEP      ((uint8_t)(-20))

///*****************************************************************************
///*                         Strcut�ṹ����������
///*****************************************************************************

///*****************************************************************************
///*                         ����������
///*****************************************************************************
void mesh_vendor_send_cmd(uint16_t user_cmd,uint8_t *pack_load,uint8_t length);

///*****************************************************************************
///*                         �ⲿȫ�ֱ���������
///*****************************************************************************

#endif
