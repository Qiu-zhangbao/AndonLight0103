//******************************************************************************
//*
//* 文 件 名 : adv_pack.h
//* 文件描述 : 
//* 作    者 : zhw/Andon Health CO.LTD
//* 版    本 : V0.0
//* 日    期 : 
//* 函数列表 : 见.c
//* 更新历史 : 见.c           
//******************************************************************************
#pragma once

#ifndef __ADV_PACK_H__
#define __ADV_PACK_H__
///*****************************************************************************
///*                         包含文件说明
///*****************************************************************************

///*****************************************************************************
///*                         宏定义区
///*****************************************************************************
#define ADV_CMD_PARI              0x0200
// #define ADV_PARILOAD_DOPAIR     00
// #define ADV_PARILOAD_PAIRACK    01
// #define ADV_PARILOAD_PAIRED     02
// #define ADV_PARILOAD_SELECT     03

#define ADV_PARILOAD_DOPAIR       0x00
#define ADV_PARILOAD_PAIRACK      0x01
#define ADV_PARILOAD_PAIRED       0x02
#define ADV_PARILOAD_SELECT       0x03
#define ADV_PARILOAD_SPAIREDACK   0x04
#define ADV_PARILOAD_WAKEUP       0x07
#define ADV_PARILOAD_RESET        0xFF


#define ADV_RECE_FIFO_LEN         40
#define ADV_SEND_FIFO_LEN         40
#define ADV_PACK_LEN              28

#define ADV_PAIRADV_INDEX             10
#define ADV_MANUDEVADV_INDEX          11

#define REMOTE_MAX_NUM                20     //最多可以绑定20个遥控器
#define PAIRED_TIMER_LEN              10     //配对定时器定时时长

#define ADV_CMD_RESET             0x02FF

#define ADV_CMD_TOOLING           0xFF01
#define ADV_TOOLING_BURNIN        0x01    //无线功能测试 检测接收信号强度并配合检测灯的性能
#define ADV_TOOLING_AFTERBURN     0x02    
#define ADV_TOOLING_POWER_TEST    0x03    //测试最大亮度和最低亮度灯的功率
#define ADV_TOOLING_UPGRADE_VER   0x04    //批量升级版本
//#define ADV_CMD_PARIED           0x0200

#define advMAXPAIRNUM            20
///*****************************************************************************
///*                         Strcut结构变量定义区
///*****************************************************************************
typedef void (*wiced_ble_adv_cmd_cback_t)(void *, uint16_t, void *, uint8_t);
typedef struct
{
    wiced_timer_t adv_pair_timer;                                //定时器
    wiced_bool_t adv_pair_enable;                                //是否允许配对
    wiced_bool_t adv_pair_wakeup;                                //是否处于配网过程中
    wiced_bool_t adv_pair_select;                                //是否被选中
    uint32_t adv_pair_runcount;                                  //配对运行时间
    wiced_bt_device_address_t adv_remotectr_mac[REMOTE_MAX_NUM]; //远程遥控MAC
    uint8_t adv_remotectr_pairindex;                             //下一个远程遥控MAC存储地址
    uint8_t adv_remotectr_pairsum;                               //共存储了多少远程遥控MAC
} adv_pair_handler_t;

typedef struct
{
    uint8_t pair_stata;
    wiced_bt_device_address_t dst_mac;
} mesh_btn_pairload_t;

//选则配置的数据包结构
typedef struct
{
    wiced_bt_device_address_t mac;
    uint16_t node_id;        //未入网为0，已入网为nodeID
    uint16_t group_id;       //如果是开关，未入网为0xFFFF，已入网为groupid 如果是灯此项填写灯的nodeid
    int16_t  pro_adv_rssi;   //选举阶段接收的RSSI值的和
    int16_t  pro_adv_num;    //选举阶段接收广播的数量
}adv_pro_select_t;

//extern adv_pair_handler_t adv_pair_params;
///*****************************************************************************
///*                         函数声明区
///*****************************************************************************
void adv_pack_init(void);
void adv_pair_enable(void);
void adv_pair_disable(void);
void adv_vendor_send_cmd(uint16_t user_cmd, uint8_t *pack_load, uint8_t length, uint8_t);
void adv_manuDevAdvStop(void);
uint16_t adv_packReadRemoteMac(uint8_t *data);
void adv_manuDevAdvStart(uint8_t *data, uint8_t len);
void advRestartPair(void);
wiced_bool_t advpackispairing(void);
///*****************************************************************************
///*                         外部全局变量定义区
///*****************************************************************************

#endif
