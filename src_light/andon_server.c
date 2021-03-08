//******************************************************************************
//*
//* 文 件 名 : andon_server.c
//* 文件描述 : Andon自定义协议GATT服务数据处理       
//* 作    者 : 张威/Andon Health CO.Ltd
//* 版    本 : V0.0
//* 日    期 : 
//*
//* 更新历史 : 
//*     日期       作者    版本     描述
//*         
//*          
//******************************************************************************

///*****************************************************************************
///*                         包含文件说明
///*****************************************************************************
#include <stdio.h>
#include "mylib.h"
#include "wiced_hal_rand.h"
#include "wiced_bt_ble.h"
#include "wiced_bt_gatt.h"
#include "andon_server.h"
#include "log.h"
#include "storage.h"
#include "light_model.h"
#include "config.h"
#include "wiced_memory.h"
#include "adv_pack.h"
#include "lightPackage.h"
#include "systimer.h"
#include "src_provisioner/mesh_application.h"
#include "DH.h"
#include "xxtea_F.h"

#ifdef DEF_ANDON_GATT
///*****************************************************************************
///*                         宏定义区
///*****************************************************************************


#define GATT_CMD_TIMEOUT    (7)
#define GATT_CMD_RPL_SIZE   (20)
///*****************************************************************************
///*                         Strcut结构变量定义区
///*****************************************************************************

typedef struct gattRplcno_t
{
    union{
        struct{
            uint8_t gattcno;
            uint8_t gattAction;
        };
        struct{
            uint16_t hash;
        };
    };
    uint32_t arrival_time;
} gattRplcno;

typedef struct DF_KEY_CONTENT
{
    unsigned int random_key;
    unsigned int exchange_key;
} st_DF_KEY_CONTENT;

///*****************************************************************************
///*                         函数声明区
///*****************************************************************************

///*****************************************************************************
///*                         常量定义区
///*****************************************************************************

///*****************************************************************************
///*                         外部全局变量定义区
///*****************************************************************************

///*****************************************************************************
///*                         文件内部全局变量定义区
///*****************************************************************************
static uint16_t andonServiceConfigDescriptor = 0;
static uint8_t  encryptkey[9] = {0};
static uint16_t myconn_id;
static gattRplcno gattCmdRpl[GATT_CMD_RPL_SIZE];
///*****************************************************************************
///*                         函数实现区
///*****************************************************************************
//*****************************************************************************
// 函数名称: AndonGattSendNotification
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
wiced_bool_t AndonGattSendNotification (uint16_t conn_id, uint16_t val_len, uint8_t *p_val, wiced_bool_t encryptflag)
{
    //if ((andonServiceConfigDescriptor & GATT_CLIENT_CONFIG_NOTIFICATION) || (andonServiceConfigDescriptor & GATT_CLIENT_CONFIG_INDICATION)) 
    {
        if(mesh_app_gatt_is_connected())
        {
            wiced_bt_gatt_status_t result;
            uint8_t *p_data;
            uint16_t templen;

            p_data = NULL;
            templen = val_len+1;  //增加一个字节标识有效内容的长度
            if(encryptflag)
            {
                templen = (templen%8?((templen>>3)+1)<<3:(templen+8));
            }
            p_data = (uint8_t*)wiced_bt_get_buffer(templen + 5);
            if(p_data == NULL)
            {
                return WICED_FALSE;
            }
            memset(p_data,0,templen+5);
            
            p_data[1] = 0;  //src Gatt直连 恒为0
            p_data[2] = 0;
            p_data[3] = 0;  //dst Gatt直连 恒为0
            p_data[4] = 0;
            
            
            WICED_BT_TRACE_ARRAY(p_val,val_len, "Andon To App Source \n");
            obfs(p_val,val_len);
            p_val[0] = Auth(p_val,val_len);
            p_data[5] = val_len;
            memcpy(p_data+6,p_val,val_len);
            val_len = templen; //增加了1个字节的数据长度
            if(!encryptflag)
            {
                p_data[0] = 0;  //Type Gatt直接交互非加密数据
            }
            else
            {
                uint8_t *output;
                size_t  outlen;
                // //未交换key时，不给app发送数据
                // if(0 == memcmp(encryptkey,"\x00\x00\x00\x00\x00\x00\x00\x00",9)){
                //     return WICED_FALSE;
                // }
                output = WYZE_F_xxtea_encrypt(p_data+5, val_len, encryptkey, &outlen, WICED_FALSE);
                if(output == NULL)
                {
                    wiced_bt_free_buffer(p_data);
                    return WICED_FALSE;
                }
                val_len = outlen;
                memcpy(p_data+5, output, val_len);
                wiced_bt_free_buffer(output);
                p_data[0] = 2;  //Type Gatt直接交互加密数据
            }
            // LOG_DEBUG("Andon Notification To App\n");
            WICED_BT_TRACE_ARRAY(p_data,val_len+5,"Andon To App EnCode \n");
            if(andonServiceConfigDescriptor & GATT_CLIENT_CONFIG_NOTIFICATION){
                result = wiced_bt_gatt_send_notification(conn_id, HANDLE_ANDON_SERVICE_CHAR_NOTIFY_VAL, val_len+5, p_data);
            }else if(andonServiceConfigDescriptor & GATT_CLIENT_CONFIG_INDICATION){
                result = wiced_bt_gatt_send_indication(conn_id, HANDLE_ANDON_SERVICE_CHAR_NOTIFY_VAL, val_len+5, p_data);
            }
            wiced_bt_free_buffer(p_data);
            if(result == WICED_BT_GATT_SUCCESS)
            {
                return WICED_TRUE;
            }
            else
            {
                return WICED_FALSE;
            }
        }
    }
    return WICED_FALSE;
}
//*****************************************************************************
// 函数名称: AndonServiceSetClientConfiguration
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
void AndonServiceSetClientConfiguration(uint16_t client_config)
{
    void appUpdataCommpara(void);

    andonServiceConfigDescriptor = client_config;
    if(client_config){
        appUpdataCommpara();
    }
    LOG_DEBUG("customer_service_config_descriptor changed: %d\n", andonServiceConfigDescriptor);
}

//*****************************************************************************
// 函数名称: AndonServiceSetClientConfiguration
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
wiced_bool_t appendGattCmdRpl(uint8_t cno, uint8_t action)
{
    gattRplcno temp;
    uint32_t systimer;
    int slot = 0;
    temp.gattAction = action;
    temp.gattcno = cno;
    // LOG_VERBOSE("hash: %x\n", temp.hash);
    systimer = sysTimerGetSystemRunTime();
    for (int i = 0; i < GATT_CMD_RPL_SIZE; i++)
    {
        if ((temp.hash == gattCmdRpl[i].hash) && (gattCmdRpl[i].arrival_time + GATT_CMD_TIMEOUT > systimer))
        {
            // already exists
            return FALSE;
        }
        if (gattCmdRpl[i].arrival_time < gattCmdRpl[slot].arrival_time)
        {
            slot = i;
        }
    }

    gattCmdRpl[slot].gattAction = action;
    gattCmdRpl[slot].gattcno = cno;
    gattCmdRpl[slot].arrival_time = systimer;
    return TRUE;
}

//*****************************************************************************
// 函数名称: AndonServerHandle 
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
wiced_bt_gatt_status_t AndonServerHandle(uint16_t conn_id, wiced_bt_gatt_write_t * p_data)
{
    uint8_t *userData;
    uint8_t userDataLen;
    packageReply replyAck;
    
    if(p_data->p_val[0] == 0xFF)  //测试数据，直接返回
    {
        // if(andonServiceConfigDescriptor & GATT_CLIENT_CONFIG_NOTIFICATION){
        //         wiced_bt_gatt_send_notification(conn_id, HANDLE_ANDON_SERVICE_CHAR_NOTIFY_VAL, p_data->val_len, p_data->p_val);
        // }else if(andonServiceConfigDescriptor & GATT_CLIENT_CONFIG_INDICATION){
        //         wiced_bt_gatt_send_indication(conn_id, HANDLE_ANDON_SERVICE_CHAR_NOTIFY_VAL, p_data->val_len, p_data->p_val);
        // }
        userDataLen = p_data->val_len - 5;
        userData = NULL;
        userData = (uint8_t *)wiced_bt_get_buffer(userDataLen);
        if(userData == NULL)
        {
            return WICED_BT_GATT_SUCCESS;
        }
        memcpy(userData,p_data->p_val + 5,userDataLen);
        WICED_BT_TRACE_ARRAY(userData,userDataLen,"test gatt data: ");
        replyAck = lightpackUnpackMsg(userData,userDataLen);
        if(replyAck.p_data != NULL)
        {
            if(replyAck.result == lightpackageSUCCESS)
            {
                //AndonGattSendNotification(conn_id, replyAck.pack_len, replyAck.p_data, WICED_TRUE);
                wiced_bt_gatt_send_indication(conn_id, HANDLE_ANDON_SERVICE_CHAR_NOTIFY_VAL, replyAck.pack_len, replyAck.p_data);
            }
            wiced_bt_free_buffer(replyAck.p_data);
        }

        wiced_bt_free_buffer(userData);

        return WICED_BT_GATT_SUCCESS;
    }

    if (p_data->val_len < 9)
        return WICED_BT_GATT_INVALID_ATTR_LEN;
    
    if(p_data->p_val[0] == 2)     //GATT直接带加密
    {
        uint8_t *output;
        size_t  outlen;
        WICED_BT_TRACE_ARRAY(p_data->p_val,p_data->val_len,"Gatt Data Len = %d ",p_data->val_len);
        WICED_BT_TRACE_ARRAY(encryptkey,sizeof(encryptkey),"encryptkey: ");
        output = WYZE_F_xxtea_decrypt(p_data->p_val + 5, p_data->val_len - 5, encryptkey, &outlen, WICED_FALSE);
        if(output == NULL)
        {
            LOG_DEBUG("WICED_BT_GATT_INTERNAL_ERROR\n");
            return WICED_BT_GATT_INTERNAL_ERROR;
        }
        p_data->val_len = output[0]+5;
        memcpy(p_data->p_val + 5,output+1,outlen);
        WICED_BT_TRACE_ARRAY(p_data->p_val,p_data->val_len,"Gatt Data Len = %d ",p_data->val_len);
        wiced_bt_free_buffer(output);
    }
    else if(p_data->p_val[0] == 0)  
    {
        WICED_BT_TRACE_ARRAY(p_data->p_val,p_data->val_len,"Gatt Data Len = %d ",p_data->val_len);
        memcpy(p_data->p_val + 5,p_data->p_val + 6,p_data->p_val[5]);
        p_data->val_len = p_data->val_len - 1;
    }

    userDataLen = p_data->val_len - 5;
    if (Auth(p_data->p_val + 5, userDataLen) != 0)
        return WICED_BT_GATT_INTERNAL_ERROR;
    obfs(p_data->p_val + 5, userDataLen);
    
    userData = NULL;
    userData = (uint8_t *)wiced_bt_get_buffer(userDataLen);
    if(userData == NULL)
    {
        return WICED_BT_GATT_SUCCESS;
    }
    memcpy(userData,p_data->p_val + 5,userDataLen);
    LOG_VERBOSE("receive gatt data :");
    #if LOGLEVEL >= LOGLEVEL_DEBUG
    // wiced_bt_trace_array(" ", p_data->p_val,  p_data->val_len);
    WICED_BT_TRACE_ARRAY(p_data->p_val,  p_data->val_len, "Decrypt: ");
    #endif
    
    if(WICED_FALSE == appendGattCmdRpl(userData[1],userData[2]))
    {
        wiced_bt_free_buffer(userData);
        return WICED_BT_GATT_SUCCESS;
    }

    if(p_data->p_val[0] == 0)     //GATT直接无加密
    {
        //数据解析
        replyAck = lightpackUnpackMsgNoEncrypt(userData,userDataLen);
        wiced_bt_free_buffer(userData);
        if(replyAck.p_data != NULL)
        {
            if(replyAck.result == lightpackageSUCCESS) 
            {
                AndonGattSendNotification(conn_id, replyAck.pack_len, replyAck.p_data, WICED_FALSE);
            }
            wiced_bt_free_buffer(replyAck.p_data);
        }
        return WICED_BT_GATT_SUCCESS;

    }
    if(p_data->p_val[0] != 2)     //非GATT直接带加密
    {
        wiced_bt_free_buffer(userData);
        return WICED_BT_GATT_SUCCESS;
    }
    WICED_BT_TRACE_ARRAY(userData,userDataLen,"later data: ");
    replyAck = lightpackUnpackMsg(userData,userDataLen);
    if(replyAck.p_data != NULL)
    {
        if(replyAck.result == lightpackageSUCCESS)
        {
            AndonGattSendNotification(conn_id, replyAck.pack_len, replyAck.p_data, WICED_TRUE);
        }
        wiced_bt_free_buffer(replyAck.p_data);
    }
    
    wiced_bt_free_buffer(userData);
    return WICED_BT_GATT_SUCCESS;

}

//*****************************************************************************
// 函数名称: andonServerSendLightStatus
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
void andonServerSendLightStatus(void)
{
    packageReply reply;
    reply = lightpackNotifyDevStata();
    if(reply.result == lightpackageMEMORYFAILED)
    {
        LOG_DEBUG("2....ERR!!!\n");
        return;
    }
    if(reply.p_data != NULL)
    {
        AndonGattSendNotification(myconn_id, reply.pack_len, reply.p_data, WICED_TRUE);
        wiced_bt_free_buffer(reply.p_data);
    }
}

//*****************************************************************************
// 函数名称: andonServerSendResetStatus
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
void andonServerSendResetStatus(void)
{
    packageReply reply;
    reply = lightpackNotifyDevResetStata();
    if(reply.result == lightpackageMEMORYFAILED)
    {
        LOG_DEBUG("2....ERR!!!\n");
        return;
    }
    if(reply.p_data != NULL)
    {
        AndonGattSendNotification(myconn_id, reply.pack_len, reply.p_data, WICED_TRUE);
        wiced_bt_free_buffer(reply.p_data);
    }
}


//*****************************************************************************
// 函数名称: AndonServerWriteHandle 
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
wiced_bt_gatt_status_t AndonServerWriteHandle(uint16_t conn_id, wiced_bt_gatt_write_t * p_data)
{
    uint8_t                *attr     = p_data->p_val;
    uint16_t                len      = p_data->val_len;
    uint16_t               temp      = 0;

    myconn_id = conn_id;
    switch (p_data->handle )
    {
        case HANDLE_ANDON_SERVICE_WRITE_VAL:
            return AndonServerHandle(conn_id, p_data);
        break;

        case HANDLE_ANDON_SERVICE_CHAR_CFG_DESC:
            if (p_data->val_len != 2)
            {
                LOG_VERBOSE("customer service config wrong len %d\n", p_data->val_len);
                return WICED_BT_GATT_INVALID_ATTR_LEN;
            }
            temp = p_data->p_val[1];
            temp = (temp<<8) + p_data->p_val[0];
            AndonServiceSetClientConfiguration(temp);
            return WICED_BT_GATT_SUCCESS;
        break;

        default:
            return WICED_BT_GATT_INVALID_HANDLE;
        break;
    }
    return WICED_BT_GATT_INVALID_HANDLE;    
}


//*****************************************************************************
// 函数名称: 
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
wiced_bt_gatt_status_t AndonServerReadHandle(uint16_t conn_id, wiced_bt_gatt_read_t * p_read_data)
{
    switch (p_read_data->handle)
    {
    case HANDLE_ANDON_SERVICE_CHAR_CFG_DESC:
        if (p_read_data->offset >= 2)
            return WICED_BT_GATT_INVALID_OFFSET;

        if (*p_read_data->p_val_len < 2)
            return WICED_BT_GATT_INVALID_ATTR_LEN;

        if (p_read_data->offset == 1)
        {
            p_read_data->p_val[0] = andonServiceConfigDescriptor >> 8;
            *p_read_data->p_val_len = 1;
        }
        else
        {
            p_read_data->p_val[0] = andonServiceConfigDescriptor & 0xff;
            p_read_data->p_val[1] = andonServiceConfigDescriptor >> 8;
            *p_read_data->p_val_len = 2;
        }
        return WICED_BT_GATT_SUCCESS;
    }
    return WICED_BT_GATT_INVALID_HANDLE;
}

//*****************************************************************************
// 函数名称: AndonServiceGattDisConnect
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
void AndonServiceGattDisConnect(void)
{
    AndonServiceSetClientConfiguration(0);
    myconn_id = 0;
    memset(encryptkey,0,sizeof(encryptkey));
    lightpackSetAuthDisable();
}

//*****************************************************************************
// 函数名称: AndonServiceSetEncryptKey
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
packageReply AndonServiceSetEncryptKey(uint8_t *data, uint8_t len)
{
    extern void wiced_hal_wdog_disable(void);
    packageReply EncryptKeyReply;
    st_DF_KEY_CONTENT KeyBody={0};
    uint32_t secretkey ;
    // uint32_t key1;

    // for (int i = 0; i < 4; i++) {
    //     KeyBody.exchange_key = KeyBody.exchange_key  << 8;
    //     KeyBody.exchange_key = KeyBody.exchange_key + data[i];
    // }
    // LOG_DEBUG("rec ex_key  %x\n",KeyBody.exchange_key);
    // KeyBody.random_key = wiced_hal_rand_gen_num();
    // key1 = wiced_hal_rand_gen_num();
    // KeyBody.exchange_key = DHKeyGenerate(key1, PUBLIC_KEYS_P, PUBLIC_KEYS_G);
    // LOG_DEBUG("rec ex_key  %x\n",KeyBody.exchange_key);
    // secretkey = DHSecretKeyGenerate(KeyBody.random_key, KeyBody.exchange_key, PUBLIC_KEYS_P);
    // mylib_sprintf(encryptkey, "%x", secretkey);
    // LOG_DEBUG("secretkey %s\n",encryptkey);
    // KeyBody.exchange_key = DHKeyGenerate(KeyBody.random_key, PUBLIC_KEYS_P, PUBLIC_KEYS_G);
    // LOG_DEBUG("rec ex_key  %x\n",KeyBody.exchange_key);
    // key1 = DHSecretKeyGenerate(key1, KeyBody.exchange_key, PUBLIC_KEYS_P);
    // mylib_sprintf(encryptkey, "%x", key1);
    // LOG_DEBUG("secretkey %s\n",encryptkey);
    // wiced_hal_wdog_disable();
    // while(1);


    for (int i = 0; i < 4; i++) {
        KeyBody.exchange_key = KeyBody.exchange_key  << 8;
        KeyBody.exchange_key = KeyBody.exchange_key + data[i];
    }
    LOG_DEBUG("rec ex_key  %x\n",KeyBody.exchange_key);
    KeyBody.random_key = wiced_hal_rand_gen_num();
    secretkey = DHSecretKeyGenerate(KeyBody.random_key, KeyBody.exchange_key, PUBLIC_KEYS_P);
    mylib_sprintf(encryptkey, "%08x", secretkey);
    LOG_DEBUG("secretkey %s\n",encryptkey);
    KeyBody.exchange_key = DHKeyGenerate(KeyBody.random_key, PUBLIC_KEYS_P, PUBLIC_KEYS_G);
    EncryptKeyReply.pack_len = 0;
    EncryptKeyReply.p_data = (uint8_t *)wiced_bt_get_buffer(4); 
    if(EncryptKeyReply.p_data == NULL)
    {
        EncryptKeyReply.result = lightpackageMEMORYFAILED;
        return EncryptKeyReply; 
    }
    LOG_DEBUG("sen ex_key  %x\n",KeyBody.exchange_key);
    EncryptKeyReply.p_data[0] = (KeyBody.exchange_key>>24)&0xFF;
    EncryptKeyReply.p_data[1] = (KeyBody.exchange_key>>16)&0xFF;
    EncryptKeyReply.p_data[2] = (KeyBody.exchange_key>>8)&0xFF;
    EncryptKeyReply.p_data[3] = KeyBody.exchange_key&0xFF;
    // memcpy(EncryptKeyReply.p_data,KeyBody.exchange_key,4);

    // for (int i = 0; i < 4; i++) {
    //     exchageKey[i]        = KeyBody.exchange_key & 0xff;
    //     KeyBody.exchange_key = KeyBody.exchange_key >> 8;
    // }
    EncryptKeyReply.pack_len = 4;
    EncryptKeyReply.result = lightpackageSUCCESS;
    return EncryptKeyReply; 
}

#ifdef DEF_ANDON_OTA
//*****************************************************************************
// 函数名称: AndonServiceUpgradeEncrypt
// 函数描述: 使用者应负责释放返回值使用的内存
// 函数输入:  
// 函数返回值: 返回数据解密后的存储指针
//*****************************************************************************/
void* AndonServiceUpgradeDecrypt(uint8_t *indata, uint16_t inlen, uint16_t *outlen)
{
    uint8_t *decryptdata;
    size_t  outlen1;

    if(indata[0] == 0)  //未加密，直接返回
    {
        decryptdata = (uint8_t *)wiced_bt_get_buffer(inlen-2);
        if(decryptdata == NULL)
        {
            *outlen = 0;
            return NULL;
        }
        *outlen = inlen-2;
        memcpy(decryptdata,indata+2,*outlen);
        return decryptdata;
    }
    if(indata[0] != 2)  //非加密，直接返回
    {
        return NULL;
    }
    decryptdata = WYZE_F_xxtea_decrypt(indata+1, inlen-1, encryptkey, &outlen1, WICED_FALSE);
    if(decryptdata == NULL)
    {
        *outlen = 0;
        return NULL;
    }
    *outlen = decryptdata[0];
    memcpy(decryptdata,decryptdata+1,*outlen);
    return decryptdata;
}

//*****************************************************************************
// 函数名称: AndonServiceUpgradeEncrypt
// 函数描述: 使用者应负责释放返回值使用的内存
// 函数输入:  
// 函数返回值: 返回数据解密后的存储指针
//*****************************************************************************/
void* AndonServiceUpgradeEncrypt(uint8_t *indata, uint16_t inlen, uint16_t *outlen)
{
    uint8_t *output;
    uint8_t *encryptdata;  
    uint16_t temp;
    size_t  outlen1;

    temp = inlen;
    temp = temp+1;
    temp = (temp%8?((temp>>3)+1)<<3:temp);
    //加密算法不会增加数据长度，但加密包标识会占用一个字节长度
    encryptdata = (uint8_t *)wiced_bt_get_buffer(temp+1);
    if(encryptdata == NULL)
    {
        *outlen = 0;
        return NULL;
    }
    memset(encryptdata,0,temp);
    memcpy(encryptdata+2,indata,temp);
    encryptdata[1] = inlen&0xff;
    LOG_DEBUG("Upgrade len=%d\n",inlen);
    output = WYZE_F_xxtea_encrypt(encryptdata+1, temp, encryptkey, &outlen1, WICED_FALSE);
    if(output == NULL)
    {
        *outlen = 0;
        wiced_bt_free_buffer(encryptdata);
        return NULL;
    }
    memcpy(encryptdata+1,output,outlen1);
    wiced_bt_free_buffer(output);
    encryptdata[0] = 2;
    *outlen = outlen1 + 1;
    return encryptdata;
}

#endif

#endif

