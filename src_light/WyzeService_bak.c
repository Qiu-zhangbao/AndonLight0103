//******************************************************************************
//*
//* 文 件 名 : 
//* 文件描述 :        
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
#include "Wyzebase64.h"
#include "WyzeService.h"


#if  WYZE_SERVICE_ENABLE
///*****************************************************************************
///*                         宏定义区
///*****************************************************************************
  
#define WYZE_MSG_CON                                    0X01                              
#define WYZE_MSG_NOCON                                  0X02                               
#define WYZE_MSG_ACK                                    0X03                              
#define WYZE_MSG_RST                                    0X04      

#define WYZE_CMD_BLEENCRYPT                             0xF0
#define WYZE_CMD_GETDEVTOKEN                            0x84
#define WYZE_CMD_SETDEVTOKEN                            0x85
#define WYZE_CMD_GETENCRYPTBINDKEY                      0x86
#define WYZE_CMD_SETENCRYPTBINDKEY                      0x87
#define WYZE_CMD_NOTICEBINDDONE                         0x88
#define WYZE_CMD_NOTICEBINDRESULT                       0x89

#define WYZE_TAG_BLEENCRYPT                             0x00F0
#define WYZE_TAG_BINDTOKEN                              0x0005
#define WYZE_TAG_DEVTOKEN                               0x0006
#define WYZE_TAG_ENCRYPTBINDKEY                         0x0007
#define WYZE_TAG_COMFRIMKEY                             0x0008
#define WYZE_TAG_BINDKEY                                0x0008
#define WYZE_TAG_BINDRESULT                             0x0009

#define wyze_FREEBUFF(p)       do{              \
    wiced_bt_free_buffer(p);                    \
    p = NULL;                                   \
}while(0)   
///*****************************************************************************
///*                         Strcut结构变量定义区
///*****************************************************************************
typedef struct 
{
    union {
        struct {
            uint8_t msgid:4;
            uint8_t encrypt:1;
            uint8_t msgtype:3;
        };
        uint8_t packetinfo;
    };
    uint8_t cmdtype;
    union{
        struct{
            uint8_t frameseq:4;
            uint8_t frametotal:4;
        };
        uint8_t frameinfo;
    };
    uint8_t payloadlen;
    uint8_t *payload;
}wyzepackettype_t;

typedef struct {
    uint16_t      length;
    uint8_t       *tagvalue;
    wiced_bool_t  result;
}wyzeTLVinfo_t;

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
static uint16_t wyzeServiceConfigDescriptor  = 0;
static uint16_t myconn_id                    = 0;
static uint8_t  encryptkey[9]                = {0};
static uint8_t  mymsgid                      = 0;
static uint8_t  remotemsgid                  = 0;

///*****************************************************************************
///*                         文件内部全局变量定义区
///*****************************************************************************

///*****************************************************************************
///*                         函数实现区
///*****************************************************************************

//*****************************************************************************
// 函数名称: WyzeServiceSetEncryptKey
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
uint8_t* WyzeServiceSetEncryptKey(uint8_t *indata, uint8_t inlen, uint8_t *outlen)
{
    st_DF_KEY_CONTENT KeyBody={0};
    uint32_t secretkey ;
    uint8_t *outdata = NULL;
    // uint32_t key1;

    for (int i = 7; i >=0; i--) {
        KeyBody.exchange_key = KeyBody.exchange_key  << 8;
        KeyBody.exchange_key = KeyBody.exchange_key + indata[i];
    }
    LOG_DEBUG("rec ex_key  %x\n",KeyBody.exchange_key);
    KeyBody.random_key = wiced_hal_rand_gen_num();
    secretkey = DHSecretKeyGenerate(KeyBody.random_key, KeyBody.exchange_key, PUBLIC_KEYS_P);
    mylib_sprintf(encryptkey, "%08x", secretkey);
    LOG_DEBUG("secretkey %s\n",encryptkey);
    KeyBody.exchange_key = DHKeyGenerate(KeyBody.random_key, PUBLIC_KEYS_P, PUBLIC_KEYS_G);
    outdata = (uint8_t *)wiced_bt_get_buffer(8); 
    if(outdata == NULL)
    {
        *outlen = 0;
        return NULL; 
    }
    LOG_DEBUG("sen ex_key  %x\n",KeyBody.exchange_key);
    for (int i = 0; i < 8; i++) {
        outdata[i] = KeyBody.exchange_key & 0xff;
        KeyBody.exchange_key = KeyBody.exchange_key >> 8;
    }
    *outlen = 8;
    return outdata; 
}
//*****************************************************************************
// 函数名称: AndonServiceSetClientConfiguration
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
void WyzeServiceSetClientConfiguration(uint16_t client_config)
{
    wyzeServiceConfigDescriptor = client_config;
    LOG_DEBUG("wyze_service_config_descriptor changed: %d\n", wyzeServiceConfigDescriptor);
}

//*****************************************************************************
// 函数名称: WyzeSendMessage
// 函数描述: 通过Wyze服务发送数据
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
void WyzeSendMessage(uint8_t cmdtype, uint8_t msgtype, uint8_t *payload, uint8_t payloadlen, wiced_bool_t encryptflag)
{
    if (wyzeServiceConfigDescriptor & GATT_CLIENT_CONFIG_NOTIFICATION)
    {
        if(mesh_app_gatt_is_connected())
        {
            wyzepackettype_t *wyzesend = NULL;
            uint8_t *sendbuffer = NULL;
            uint8_t *output = NULL;
            size_t  outlen;
            uint8_t templen;

            templen = payloadlen;
            if(encryptflag == WICED_TRUE){
                templen = (templen%8?((templen>>3)+1)<<3:(templen+8));
            }
            if(templen > 240)
            {
                templen = 240;
            }
            sendbuffer = wiced_bt_get_buffer(templen+4);    
            if(sendbuffer == NULL)
            {
                goto WYZESENDERR;
            }
            memset(sendbuffer,0,templen+4);
            memcpy(sendbuffer+4,payload,payloadlen);

            wyzesend = (wyzepackettype_t*)sendbuffer;
            if(encryptflag == WICED_FALSE){
                wyzesend->encrypt = WICED_FALSE;
            }else{
                wyzesend->encrypt = WICED_TRUE;
                // 对数据进行加密处理
                for(uint8_t i=0; i<templen; i+=8)
                {
                    uint8_t *p_temp;
                    p_temp = WYZE_F_xxtea_encrypt(sendbuffer+4+i, 8, encryptkey, &outlen, WICED_FALSE);
                    if(p_temp == NULL){
                        goto WYZESENDERR;
                    }
                    memcpy(sendbuffer+4+i,p_temp,outlen);
                    wyze_FREEBUFF(p_temp);
                }
                // output = WYZE_F_xxtea_encrypt(sendbuffer+4, templen, encryptkey, &outlen, WICED_FALSE);
                // if(output == NULL)
                // {
                //     goto WYZESENDERR;
                // }
                // templen = outlen;
                // memcpy(sendbuffer+4,output,outlen);
            }
            if(msgtype != WYZE_MSG_ACK){
                wyzesend->msgid = mymsgid++;  //这里是不大好的，因为ACK有可能是使用接收的msgid
            }else{
                wyzesend->msgid = remotemsgid;
            }
            wyzesend->msgtype   = msgtype;
            wyzesend->frameinfo = 0; 
            wyzesend->cmdtype   = cmdtype;
            wyzesend->payloadlen = payloadlen;

            LOG_DEBUG("msgid: %d  encrypt:%d  msgtype: %02x \n",
                    wyzesend->msgid,wyzesend->encrypt,wyzesend->msgtype);
            LOG_DEBUG("cmdtype: %02x  frameseq:%d  frametotal: %d \n",
                    wyzesend->cmdtype,wyzesend->frameseq,wyzesend->frametotal);
            LOG_DEBUG("payloadlen: %d  \n",
                    wyzesend->payloadlen);
            WICED_BT_TRACE_ARRAY(sendbuffer,templen+4,"Wyze send: ");
            wiced_bt_gatt_send_notification(myconn_id, HANDLE_WYZE_SERVICE_CHAR_WRITEANDNOTIFY_VAL, templen+4, sendbuffer);
WYZESENDERR:
            wyze_FREEBUFF(output);
            wyze_FREEBUFF(sendbuffer);
        }   
    }
}

//*****************************************************************************
// 函数名称: WyzeTLVSetPayload
// 函数描述: 将数据按照Wyze TLV格式打包
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
wiced_bool_t WyzeTLVSetPayload(uint16_t tag, uint8_t *payload, uint8_t payloadlen, uint8_t *TLVpayload)
{
    uint8_t *TLVPayload = NULL;
    if (NULL == TLVpayload) {
        return WICED_FALSE;
    }
    if (NULL == payload) {
        return WICED_FALSE;
    }

    TLVpayload[0] = tag & 0xff;
    TLVpayload[1] = ((tag & 0xff00) >> 8);
    TLVpayload[2] = payloadlen & 0xff;
    TLVpayload[3] = ((payloadlen & 0xff00) >> 8);
    memcpy(TLVpayload + 4, payload, payloadlen);
    return WICED_TRUE;
   
}

//*****************************************************************************
// 函数名称: WyzeTLVGetPayload
// 函数描述: 从paload中查找tag的位置，找到则返回地址，找不到返回NULL
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
wyzeTLVinfo_t WyzeTLVGetPayload(uint16_t tag, uint8_t *payload, uint8_t payloadlen)
{
    wyzeTLVinfo_t wyzetlvinfo={
        .length = 0,
        .tagvalue = NULL,
        .result   = WICED_FALSE
    };

    uint8_t tag0 = tag&0xff;
    uint8_t tag1 = (tag>>8)&0xff;
    //最少2个字节的数据长度
    for(uint8_t i=0; i<(payloadlen-3); i++)  
    {
        if((tag0 == payload[i]) && (tag1 == payload[i+1]))
        {
            //数据不可能超过255字节，所以仅读取低位
            wyzetlvinfo.length = payload[i+2];
            wyzetlvinfo.result = WICED_TRUE;
            if(wyzetlvinfo.length != 0){
                wyzetlvinfo.tagvalue =  payload+i+4;
            }
        }
    }
    return wyzetlvinfo;
}

//*****************************************************************************
// 函数名称: wyzeSetSecret
// 函数描述: 获取通信加密的key，同时将交换的秘钥打包
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
void wyzeSetSecret(wyzepackettype_t *wyzereceive)
{
    wyzeTLVinfo_t wyzetlvinfo;
    // wyzetlvinfo = WyzeTLVGetPayload(WYZE_TAG_BLEENCRYPT,wyzereceive->payload,wyzereceive->payloadlen);
    //提取数据，交由AndonService提前公钥并计算加密秘钥 
    wyzetlvinfo.tagvalue = wyzereceive->payload;
    wyzetlvinfo.length = wyzereceive->payloadlen;
    // if((wyzetlvinfo.result == WICED_TRUE) && (wyzetlvinfo.tagvalue != NULL))
    {
        uint8_t *exchange_key = NULL;
        uint8_t *replyarray   = NULL;
        uint8_t exchange_key_len;

        exchange_key = WyzeServiceSetEncryptKey(wyzetlvinfo.tagvalue,wyzetlvinfo.length,&exchange_key_len);
        if(exchange_key == NULL)
        {
            goto ENCRYPTERR;
        }
        replyarray = wiced_bt_get_buffer(exchange_key_len+4);
        if(replyarray == NULL)
        {
            goto ENCRYPTERR;
        }
        memcpy(replyarray,exchange_key,8);
        // WyzeTLVSetPayload(WYZE_TAG_BLEENCRYPT,exchange_key,exchange_key_len,replyarray);
        WyzeSendMessage(WYZE_CMD_BLEENCRYPT,WYZE_MSG_NOCON,replyarray,exchange_key_len,WICED_FALSE);
        
ENCRYPTERR:
        wyze_FREEBUFF(exchange_key);
        wyze_FREEBUFF(replyarray);
    }
    // else
    // {
    //     //TODO 不识别的TLV
    // }
}

//*****************************************************************************
// 函数名称: wyzeGetDeviceToken 
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
void wyzeGetDeviceToken(wyzepackettype_t *wyzereceive)
{
    uint8_t tmpBuffer[150] = {0};
    uint16_t templen;
    uint8_t *didandkey = NULL;
    uint8_t *encryptdata = NULL;
    size_t  encryptlen;
    uint32_t randomdev;
    uint8_t *replyarray = NULL;
    uint8_t *base64_data = NULL; 

    wyzeTLVinfo_t wyzetlvinfo;
    wyzetlvinfo = WyzeTLVGetPayload(WYZE_TAG_BINDTOKEN,wyzereceive->payload,wyzereceive->payloadlen);
    if((wyzetlvinfo.result != WICED_TRUE) || (wyzetlvinfo.tagvalue == NULL))
    {
        //TODO 不识别的TLV
        return;
    }

    didandkey = (uint8_t *)wiced_bt_get_buffer(FLASH_XXTEA_SN_LEN+1); 
    if(didandkey == NULL){
        return;
    }
    storage_read_sn(didandkey);
    didandkey[FLASH_XXTEA_SN_LEN] = '\0';

    randomdev = wiced_hal_rand_gen_num();
    didandkey[FLASH_XXTEA_ID_LEN] = '\0';  //devicetoken由DID+KEY构成，DID在前且未字符串，此处为DID增加结束符
    randomdev = randomdev&0x7FFFFFFF;
    // randomdev = 0;
    mylib_sprintf(tmpBuffer, "{\"device_id\":\"%s\",\"timestamp\":\"%d\"}", didandkey, randomdev);
    LOG_DEBUG("%s \n",tmpBuffer);
    templen = strlen(tmpBuffer);
    // LOG_DEBUG("BindToken1: %s \n",wyzetlvinfo.tagvalue);
    WICED_BT_TRACE_ARRAY(wyzetlvinfo.tagvalue,wyzetlvinfo.length,"BindToken1: len = %d\n",wyzetlvinfo.length);
    encryptdata = WYZE_F_xxtea_encrypt(tmpBuffer, templen, wyzetlvinfo.tagvalue, &encryptlen, WICED_TRUE);
    if(NULL == encryptdata){
        goto GETDEVICETOKENERR;
    }
    base64_data = (uint8_t *)(WYZE_base64_encode(encryptdata, encryptlen));
    if(base64_data == NULL){
        goto GETDEVICETOKENERR;
    }
    templen = strlen(base64_data);
    replyarray = wiced_bt_get_buffer(templen+4);
    if(replyarray == NULL)
    {
        goto GETDEVICETOKENERR;
    }
    LOG_DEBUG("deviceToken1 len=%d v=%s \n", templen, base64_data);
    WyzeTLVSetPayload(WYZE_TAG_DEVTOKEN,base64_data,templen,replyarray);
    WyzeSendMessage(WYZE_CMD_SETDEVTOKEN,WYZE_MSG_NOCON,replyarray,templen+4,WICED_TRUE);
GETDEVICETOKENERR:
    wyze_FREEBUFF(didandkey);
    wyze_FREEBUFF(encryptdata);
    wyze_FREEBUFF(base64_data);
    wyze_FREEBUFF(replyarray);
}

//*****************************************************************************
// 函数名称: wyzeGetEncryptBindKey 
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
void wyzeGetEncryptBindKey(wyzepackettype_t *wyzereceive)
{
    uint8_t tmpBuffer[150] = {0};
    uint8_t *didandkey = NULL;
    uint8_t *encryptdata=NULL,*devbindkey=NULL;
    size_t  encryptlen,decryptlen;
    uint8_t *base64_data=NULL;
    uint8_t *replyarray = NULL;
    uint16_t templen;
    uint32_t randomdev;
    wyzeTLVinfo_t wyzetlvinfo;

    wyzetlvinfo = WyzeTLVGetPayload(WYZE_TAG_ENCRYPTBINDKEY,wyzereceive->payload,wyzereceive->payloadlen);
    if((wyzetlvinfo.result != WICED_TRUE) || (wyzetlvinfo.tagvalue == NULL))
    {
        //TODO 不识别的TLV
        return;
    }

    didandkey = (uint8_t *)wiced_bt_get_buffer(FLASH_XXTEA_SN_LEN+1); 
    if(didandkey == NULL){
        goto GETENCRYPTBINDKEYERR;
    }
    storage_read_sn(didandkey);
    didandkey[FLASH_XXTEA_SN_LEN] = '\0';

    LOG_DEBUG("encryptBindTokenB64: %s\n",wyzetlvinfo.tagvalue);
    WICED_BT_TRACE_ARRAY(wyzetlvinfo.tagvalue,wyzetlvinfo.length,"encryptBindTokenB64: len = %d\n",wyzetlvinfo.length);
    base64_data = (uint8_t *)(WYZE_base64_decode(wyzetlvinfo.tagvalue, &templen));
    if(base64_data == NULL)
    {
        LOG_DEBUG("base64_data: no mem!!!\n");
        goto GETENCRYPTBINDKEYERR;
    }
    //devicetoken由DID+KEY构成，DID在前且未字符串，KEY在后
    LOG_DEBUG("encryptBindToken: %s\n",base64_data);
    WICED_BT_TRACE_ARRAY(base64_data,templen,"encryptBindTokenHex templen=%d\n",templen);
    LOG_DEBUG("devkey %s\n",didandkey+FLASH_XXTEA_KEY_OFFSET);
    deb_print_buf_use();
    devbindkey = WYZE_F_xxtea_decrypt(base64_data,templen,didandkey+FLASH_XXTEA_KEY_OFFSET, &decryptlen, WICED_TRUE);
    if(NULL == devbindkey)
    {
        LOG_DEBUG("decryptdata: no mem!!!\n");
        goto GETENCRYPTBINDKEYERR;
    }

    //bindkey 保存失败 则将bindkey置为0 使得绑定失败
    if(WICED_FALSE == StoreBindKey(devbindkey,decryptlen))
    {
        memset(devbindkey,0,decryptlen);
    }
    LOG_DEBUG("devbindkey: %s\n",devbindkey);
    WICED_BT_TRACE_ARRAY(devbindkey,decryptlen,"devbindkeyhex len = %d ",decryptlen);
    randomdev = wiced_hal_rand_gen_num();
    randomdev = randomdev&0x7FFFFFFF; 
    didandkey[FLASH_XXTEA_ID_LEN] = '\0';  //devicetoken由DID+KEY构成，DID在前且未字符串，此处为DID增加结束符
    mylib_sprintf(tmpBuffer, "{\"device_id\":\"%s\",\"timestamp\":%d}", didandkey, randomdev);
    templen = strlen(tmpBuffer);
    LOG_DEBUG("ConfirmKey: %s\n",tmpBuffer);
    encryptdata = WYZE_F_xxtea_encrypt(tmpBuffer,templen,devbindkey, &encryptlen, WICED_TRUE);
    if(NULL == encryptdata)
    {
        LOG_DEBUG("encryptdata: no mem!!!\n");
        goto GETENCRYPTBINDKEYERR;
    }
    base64_data = (uint8_t *)(WYZE_base64_encode(encryptdata, encryptlen));
    if(base64_data == NULL)
    {
        LOG_DEBUG("base64_data: no mem!!!\n");
        goto GETENCRYPTBINDKEYERR;
    }
    templen = strlen(base64_data);
    replyarray = wiced_bt_get_buffer(templen+4);
    if(replyarray == NULL)
    {
        goto GETENCRYPTBINDKEYERR;
    }
    LOG_DEBUG("comfirmkey len=%d v=%s \n", templen, base64_data);
    WyzeTLVSetPayload(WYZE_TAG_COMFRIMKEY,base64_data,templen,replyarray);
    WyzeSendMessage(WYZE_CMD_SETENCRYPTBINDKEY,WYZE_MSG_NOCON,replyarray,templen+4,WICED_TRUE);

GETENCRYPTBINDKEYERR:
    wyze_FREEBUFF(didandkey);
    wyze_FREEBUFF(encryptdata);
    wyze_FREEBUFF(base64_data);
    wyze_FREEBUFF(devbindkey);
    wyze_FREEBUFF(replyarray);
}

//*****************************************************************************
// 函数名称: wyzeNoticeBindDone 
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
void wyzeNoticeBindDone(wyzepackettype_t *wyzereceive)
{
    uint8_t replyarray[5] = {0};
    uint8_t bindresult;
    wyzeTLVinfo_t wyzetlvinfo;

    // wyzetlvinfo = WyzeTLVGetPayload(WYZE_TAG_BINDKEY,wyzereceive->payload,wyzereceive->payloadlen);
    // if(wyzetlvinfo.result != WICED_TRUE) 
    // {
    //     return;
    // }
    //todo 是否需要识别绑定结果
    bindresult = 0x00; //绑定成功
    WyzeTLVSetPayload(WYZE_TAG_BINDRESULT,&bindresult,1,replyarray);
    WyzeSendMessage(WYZE_CMD_NOTICEBINDRESULT,WYZE_MSG_NOCON,replyarray,1+4,WICED_TRUE);
}

//*****************************************************************************
// 函数名称: WyzeServerHandle 
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
wiced_bt_gatt_status_t WyzeServerHandle(uint16_t conn_id, wiced_bt_gatt_write_t * p_data)
{
    wyzepackettype_t wyzereceive;

    wyzereceive.packetinfo = p_data->p_val[0];
    wyzereceive.cmdtype    = p_data->p_val[1]; 
    wyzereceive.frameinfo  = p_data->p_val[2]; 
    wyzereceive.payloadlen = p_data->p_val[3]; 
    
    LOG_DEBUG("msgid: %d  encrypt:%d  msgtype: %02x \n",
              wyzereceive.msgid,wyzereceive.encrypt,wyzereceive.msgtype);

    LOG_DEBUG("cmdtype: %02x  frameseq:%d  frametotal: %d \n",
              wyzereceive.cmdtype,wyzereceive.frameseq,wyzereceive.frametotal);
    LOG_DEBUG("payloadlen: %d  \n",
              wyzereceive.payloadlen);

    remotemsgid = wyzereceive.msgid;

    WICED_BT_TRACE_ARRAY(p_data->p_val,p_data->val_len,"Wyze service rec:");

    if(0 == wyzereceive.encrypt){        //未加密数据仅允许传输加密随机数
        WICED_BT_TRACE_ARRAY(p_data->p_val,p_data->val_len,"Wyze no encrypt:");
        wyzereceive.payload    = p_data->p_val+4; 
        if(wyzereceive.cmdtype == WYZE_CMD_BLEENCRYPT){
            wyzeSetSecret(&wyzereceive);
        }else{
            //TODO 回复数据未加密
        }
        return WICED_BT_GATT_SUCCESS;
    }else{
        //TODO 加密数据 先对payload进行解密
        size_t  outlen;
        // wyzereceive.payload = WYZE_F_xxtea_decrypt(p_data->p_val + 4, wyzereceive.payloadlen, encryptkey, &outlen, WICED_FALSE);
        LOG_DEBUG("secretkey %s\n",encryptkey);
        wyzereceive.payload = wiced_bt_get_buffer(p_data->val_len-4);
        if(wyzereceive.payload == NULL){
            return WICED_BT_GATT_SUCCESS;
        }
        for(uint8_t i=0; i<p_data->val_len-4; i+=8)
        {
            uint8_t *p_temp;

            p_temp = WYZE_F_xxtea_decrypt(p_data->p_val + 4 + i, 8, encryptkey, &outlen, WICED_FALSE);
            if(p_temp == NULL){
                wyze_FREEBUFF(wyzereceive.payload);
                return WICED_BT_GATT_SUCCESS;
            }
            memcpy(wyzereceive.payload+i,p_temp,outlen);
            wyze_FREEBUFF(p_temp);
        }
        // wyzereceive.payload = WYZE_F_xxtea_decrypt(p_data->p_val + 4, p_data->val_len-4, encryptkey, &outlen, WICED_FALSE);
        // if(wyzereceive.payload == NULL){
        //     return WICED_BT_GATT_SUCCESS;
        // }
        
        WICED_BT_TRACE_ARRAY(wyzereceive.payload,wyzereceive.payloadlen,"Wyze decrypt len= %d: ",wyzereceive.payloadlen);
        switch(wyzereceive.cmdtype){
        case WYZE_CMD_GETDEVTOKEN:
            wyzeGetDeviceToken(&wyzereceive);
            break;
        case WYZE_CMD_GETENCRYPTBINDKEY:
            wyzeGetEncryptBindKey(&wyzereceive);
            break;
        case WYZE_CMD_NOTICEBINDDONE:
            wyzeNoticeBindDone(&wyzereceive);
            break;
        default:
            //TODO 回复不识别的指令
            break;
        }
        wyze_FREEBUFF(wyzereceive.payload);
    }
    return WICED_BT_GATT_SUCCESS;
}
//*****************************************************************************
// 函数名称: 
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
wiced_bt_gatt_status_t WyzeServerReadHandle(uint16_t conn_id, wiced_bt_gatt_read_t * p_read_data)
{
    myconn_id = conn_id;
    switch (p_read_data->handle)
    {
    case HANDLE_WYZE_SERVICE_CHAR_CFG_DESC:
        if (p_read_data->offset >= 2)
            return WICED_BT_GATT_INVALID_OFFSET;

        if (*p_read_data->p_val_len < 2)
            return WICED_BT_GATT_INVALID_ATTR_LEN;

        if (p_read_data->offset == 1)
        {
            p_read_data->p_val[0] = wyzeServiceConfigDescriptor >> 8;
            *p_read_data->p_val_len = 1;
        }
        else
        {
            p_read_data->p_val[0] = wyzeServiceConfigDescriptor & 0xff;
            p_read_data->p_val[1] = wyzeServiceConfigDescriptor >> 8;
            *p_read_data->p_val_len = 2;
        }
        return WICED_BT_GATT_SUCCESS;
    }
    return WICED_BT_GATT_INVALID_HANDLE;
}

//*****************************************************************************
// 函数名称: WyzeServerWriteHandle 
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
wiced_bt_gatt_status_t WyzeServerWriteHandle(uint16_t conn_id, wiced_bt_gatt_write_t * p_data)
{
    uint8_t                *attr     = p_data->p_val;
    uint16_t                len      = p_data->val_len;
    uint16_t               temp      = 0;
    
    myconn_id = conn_id;

    switch (p_data->handle )
    {
        case HANDLE_WYZE_SERVICE_CHAR_WRITEANDNOTIFY_VAL:
            return WyzeServerHandle(conn_id, p_data);
        break;

        case HANDLE_WYZE_SERVICE_CHAR_CFG_DESC:
            if (p_data->val_len != 2)
            {
                LOG_VERBOSE("customer service config wrong len %d\n", p_data->val_len);
                return WICED_BT_GATT_INVALID_ATTR_LEN;
            }
            temp = p_data->p_val[1];
            temp = (temp<<8) + p_data->p_val[0];
            WyzeServiceSetClientConfiguration(temp);
            return WICED_BT_GATT_SUCCESS;
        break;

        default:
            return WICED_BT_GATT_INVALID_HANDLE;
        break;
    }
    return WICED_BT_GATT_INVALID_HANDLE;    
}

//*****************************************************************************
// 函数名称: WyzeServiceGattDisConnect
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
void WyzeServiceGattDisConnect(void)
{
    //在这里不清除加密秘钥 在Andon服务中已经清除秘钥了
    WyzeServiceSetClientConfiguration(0);  
    myconn_id = 0;
    mymsgid = 0;
    remotemsgid = 0;
    memset(encryptkey,0,sizeof(encryptkey));
}

#endif

