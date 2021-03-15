//******************************************************************************
//*
//* 文 件 名 : 
//* 文件描述 :        
//* 作    者 : 张威/Andon Health CO.Ltd
//* 版    本 : V0.0
//* 日    期 : 
//******************************************************************************
#ifndef LIGHT_PACKAGE_H
#define LIGHT_PACKAGE_H
///*****************************************************************************
///*                         包含文件说明
///*****************************************************************************
#include "stdint.h"
#include "config.h"

///*****************************************************************************
///*                         宏定义区
///*****************************************************************************
#define  lightpackageSUCCESS            0
#define  lightpackageOPCODEFAILED       1                   //操作码异常
#define  lightpackageMEMORYFAILED       2                   //未申请到内存
#define  lightpackageACTIONFAILED       3                   //执行动作异常

///*****************************************************************************
///*                         Strcut结构变量定义区
///*****************************************************************************
typedef struct packagereply_t
{
    uint8_t *p_data;                 //数据处理后返回内存的存储地址，使用后应释放内存 NULL表示无数据
    uint16_t pack_len;               //数据包长度
    uint16_t result;                 //数据包长度
    uint16_t dst;                    //目标地址
    uint8_t  opcode;                 //操作码
} packageReply;

///*****************************************************************************
///*                         外部全局常量声明区
///*****************************************************************************

///*****************************************************************************
///*                         外部全局变量声明区
///*****************************************************************************

///*****************************************************************************
///*                         函数声明区
///*****************************************************************************
void obfs(uint8_t *data, uint16_t len);
uint8_t Auth(uint8_t *data, uint16_t len);
packageReply lightpackNotifyDevStata(void);
packageReply  lightpackNotifyCountdownStata(void);
packageReply lightpackNotifyDevResetStata(void);
packageReply lightpackUnpackMsgNoEncrypt(uint8_t *p_data, uint16_t data_len);
packageReply lightpackUnpackMsg(uint8_t *p_data, uint16_t data_len);
uint8_t lightpackGetSendCno(void);
void lightpackSetSendCno(uint8_t seq);
void lightpackSetAuthDisable(void);
void lightpackSetAuthEnable(void);

#endif

