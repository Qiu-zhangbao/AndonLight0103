#include "llog.h"
#include "log.h"
#include "storage.h"
#include "vendor.h"
#include "systimer.h"
#include "config.h"


#if  ANDON_LIGHT_LOG_ENABLE
#define LOGMAXSIZE   100
#define LOGINDEX_PLUS(x,y)  ((x+y)%LOGMAXSIZE)
#define LOGINDEX_MINUS(x,y)  ((x+LOGMAXSIZE-y)%LOGMAXSIZE)
#define LOGNUM_PLUS(x,y)  ((x+y)>LOGMAXSIZE?LOGMAXSIZE:(x+y))


//记录数据结构
typedef union
{
    struct rcd_stru//结构
    {
        uint8_t record_stat;
        uint8_t Reserved[3];
        uint16_t operatetype;             //操作类型
        uint16_t deltatime;               //上次与本次操作的时间间隔
        uint32_t lasttime;                //上次操作的时间，为0表示无效
        uint32_t logvalue;                //log的数值，依据不同log类型确定
    }Item;
    uint8_t data_buf[sizeof(struct rcd_stru)]; //注意大小
}UNION_RCD_DATA_T;

UNION_RCD_DATA_T   LogConfig1 = {
    .Item = {
    .lasttime = 0,
    .operatetype = LOG_IDLETYPE,
    .logvalue    = 0,
    }
};

LogConfig_def LogConfig ={
    .lasttime = 0,
    .operatetype = LOG_IDLETYPE,
    .logvalue    = 0,
};

UNION_RCD_DATA_T logcache[LOGMAXSIZE]={0};
UNION_RCD_DATA_T logrecord;
uint16_t      lognum = 0;
uint16_t      logindex_in = 0;
uint16_t      logindex_out = 0;

uint16_t      llogsaveflag = 0;

void llog_initiate(void)
{

}

void llog_write(void)
{
    uint32_t systimercurrent;

    //非主动保存log
    if(llogsaveflag == 1){
        llogsaveflag = 0;
        return;
    }
    
    if(LogConfig.operatetype == LOG_IDLETYPE){
        return;
    }
    //时钟小于3600*24*366*3（3年的秒数）认为时钟没有同步过则不保存log

    systimercurrent = sysTimerGetSystemUTCTime();
#if !(ANDON_TEST)
    if(systimercurrent < systimer_MINSYSTIMECNT)  
    {
        return;
    }
#endif    
    memcpy(&LogConfig1.Item.operatetype,&LogConfig,sizeof(LogConfig_def));   
    logcache[logindex_in] = LogConfig1;
    //log的时间单位为分钟
    if(logcache[logindex_in].Item.operatetype != LOG_TIMETYPE)
    {
        uint32_t temp;
        //非绝对时间，存储的是与上次log的差值时间
        temp = systimercurrent/60 - logcache[LOGINDEX_MINUS(logindex_in,1)].Item.lasttime/60;
        if(temp > 0x3FFF)  //超过14位时钟宽度，存储一次绝对时间
        {
            logcache[logindex_in].Item.lasttime = systimercurrent;
            logcache[logindex_in].Item.operatetype = LOG_TIMETYPE;
            logcache[logindex_in].Item.logvalue = systimercurrent;
            memcpy(&logrecord,&logcache[logindex_in],sizeof(UNION_RCD_DATA_T));
            //record_save(0,&logrecord);
            logindex_in = LOGINDEX_PLUS(logindex_in,1);
            lognum = LOGNUM_PLUS(lognum,1);
        }
        logcache[logindex_in] = LogConfig1;
        logcache[logindex_in].Item.lasttime = logcache[LOGINDEX_MINUS(logindex_in,1)].Item.lasttime;
        logcache[logindex_in].Item.deltatime = systimercurrent/60 - logcache[logindex_in].Item.lasttime/60;
        
    }
    else
    {
        logcache[logindex_in].Item.deltatime = 0;
        logcache[logindex_in].Item.lasttime = systimercurrent;
        logcache[logindex_in].Item.logvalue = systimercurrent;
    }
    memcpy(&logrecord,&logcache[logindex_in],sizeof(UNION_RCD_DATA_T));
    //record_save(0,&logrecord);
    LogConfig1.Item.operatetype = LOG_IDLETYPE;
    LogConfig.operatetype = LOG_IDLETYPE;
    logindex_in = LOGINDEX_PLUS(logindex_in,1);
    lognum = LOGNUM_PLUS(lognum,1);
    // if(logindex_in == logindex_out)
    // {
    //     logindex_out = LOGINDEX_PLUS(logindex_out,1);
    // }
}

uint16_t llog_read(uint8_t *buffer, uint16_t *len,uint16_t read_time)
{
    static uint16_t log_num = 0;
    uint16_t index;
    uint32_t logtime_minute;
    int8_t   logtime_zone;
    
    // if(read_time &0x01)
    // {
    //     if(RESULT_OK != record_sum(0,&log_num))
    //     {
    //         log_num = 0;
    //         return 0;
    //     }
    // }
    // else if (log_num == 0)
    // {
    //     return 0;
    // }

    if(lognum == 0)
    {
        *len = 0;
        return 0;
    }
    else if(lognum == LOGMAXSIZE)
    {
        logindex_out = logindex_in;
    }
    index = 0;
    logtime_zone = sysTimerGetSystemTimeZone();
    if((read_time &0x01) && (logcache[logindex_out].Item.operatetype != LOG_TIMETYPE))
    {
        LOG_DEBUG("LOGtype: %x\n ",logcache[logindex_out].Item.operatetype);
        logtime_minute = logcache[logindex_out].Item.lasttime/60;
        if(logtime_minute > 0xFFFFFFE)
        {
            logtime_minute = 0xFFFFFFE;
        }
        LOG_DEBUG("LOG: LOG_TIMETYPE ");
        buffer[index++] = 0;
        WICED_BT_TRACE("%02x ",buffer[index-1]);
        buffer[index++] = 0|0x03;
        WICED_BT_TRACE("%02x ",buffer[index-1]);
        buffer[index++] = 0xE0|((logtime_minute>>24)&0x0F);
        WICED_BT_TRACE("%02x ",buffer[index-1]);
        buffer[index++] = (logtime_minute>>16)&0xFF;
        WICED_BT_TRACE("%02x ",buffer[index-1]);
        buffer[index++] = (logtime_minute>>8)&0xFF;
        WICED_BT_TRACE("%02x ",buffer[index-1]);
        buffer[index++] = (logtime_minute)&0xFF;
        WICED_BT_TRACE("%02x \n",buffer[index-1]);
        LOG_VERBOSE("LOG: LOG_TIMEZONE ");
        buffer[index++] = 0;
        WICED_BT_TRACE("%02x ",buffer[index-1]);
        buffer[index++] = 0|0x03;
        WICED_BT_TRACE("%02x ",buffer[index-1]);
        buffer[index++] = 0xF8|((logtime_zone>>6)&0x03);
        WICED_BT_TRACE("%02x ",buffer[index-1]);
        buffer[index++] = (logtime_zone<<2)&0xFF;
        WICED_BT_TRACE("%02x \n",buffer[index-1]);
    }
    while((logindex_in != logindex_out) || (lognum == LOGMAXSIZE))
    {
        // if(log_num > 0)
        // {
        //     if(RESULT_OK != record_read(0,log_num,&logrecord))
        //     {
        //         return log_num;
        //     }
        //     // LOG_VERBOSE("read log from SFLASH");
        //     // switch (logcache[logindex_out].Item.operatetype)
        //     // {
        //     // case LOG_TIMETYPE:
        //     //     logtime_minute = logcache[logindex_out].Item.logvalue/60;
        //     //     if(logtime_minute > 0xFFFFFFE)
        //     //     {
        //     //         logtime_minute = 0xFFFFFFE;
        //     //     }
        //     //     LOG_VERBOSE("LOG: LOG_TIMETYPE ");
        //     //     buffer[index++] = 0;
        //     //     WICED_BT_TRACE("%02x ",buffer[index-1]);
        //     //     buffer[index++] = 0|0x03;
        //     //     WICED_BT_TRACE("%02x ",buffer[index-1]);
        //     //     buffer[index++] = 0xE0|((logtime_minute>>24)&0x0F);
        //     //     WICED_BT_TRACE("%02x ",buffer[index-1]);
        //     //     buffer[index++] = (logtime_minute>>16)&0xFF;
        //     //     WICED_BT_TRACE("%02x ",buffer[index-1]);
        //     //     buffer[index++] = (logtime_minute>>8)&0xFF;
        //     //     WICED_BT_TRACE("%02x ",buffer[index-1]);
        //     //     buffer[index++] = (logtime_minute)&0xFF;
        //     //     WICED_BT_TRACE("%02x \n",buffer[index-1]);
        //     //     break;
        //     // case LOG_ONOFFTYPE:
        //     //     LOG_VERBOSE("LOG: LOG_ONOFFTYPE ");
        //     //     buffer[index++] = (logtime_minute>>6)&0xFF;
        //     //     WICED_BT_TRACE("%02x ",buffer[index-1]);
        //     //     buffer[index] = (logtime_minute<<2)&0xFC;
        //     //     if(logcache[logindex_out].Item.logvalue&0x02)
        //     //     {
        //     //         buffer[index] |= 0x01;
        //     //     }
        //     //     else
        //     //     {
        //     //         buffer[index] &= 0xFE;
        //     //     }
        //     //     WICED_BT_TRACE("%02x \n",buffer[index++]);
        //     //     if(logcache[logindex_out].Item.logvalue&0x01)
        //     //     {
        //     //         buffer[index] = 0x80;
        //     //     }
        //     //     else
        //     //     {
        //     //         buffer[index] = 0x00;
        //     //     }
        //     //     WICED_BT_TRACE("%02x \n",buffer[index++]);
        //     //     break;
        //     // case LOG_BRIGHTNESSTYPE:
        //     //     LOG_VERBOSE("LOG: LOG_BRIGHTNESSTYPE ");
        //     //     buffer[index++] = (logtime_minute>>6)&0xFF;
        //     //     WICED_BT_TRACE("%02x ",buffer[index-1]);
        //     //     buffer[index++] = ((logtime_minute<<2)&0xFC|0x02);
        //     //     WICED_BT_TRACE("%02x ",buffer[index-1]);
        //     //     if(logcache[logindex_out].Item.logvalue > 100)
        //     //         logcache[logindex_out].Item.logvalue = 100;
        //     //     buffer[index++] = (logcache[logindex_out].Item.logvalue&0xFF);
        //     //     WICED_BT_TRACE("%02x \n",buffer[index-1]);
        //     //     break;
        //     // case LOG_DETLATYPE_BRIGHTNESS:
        //     //     LOG_VERBOSE("LOG: LOG_DETLATYPE_BRIGHTNESS ");
        //     //     buffer[index++] = (logtime_minute>>6)&0xFF;
        //     //     WICED_BT_TRACE("%02x ",buffer[index-1]);
        //     //     buffer[index++] = ((logtime_minute<<2)&0xFC|0x03);
        //     //     WICED_BT_TRACE("%02x ",buffer[index-1]);
        //     //     buffer[index++] = (logcache[logindex_out].Item.logvalue>>18)&0x3F;
        //     //     WICED_BT_TRACE("%02x ",buffer[index-1]);
        //     //     buffer[index++] = ((logcache[logindex_out].Item.logvalue>>10)&0xC0) + (((logcache[logindex_out].Item.logvalue&0xFFFF)/100)&0x3F);
        //     //     WICED_BT_TRACE("%02x \n",buffer[index-1]);
        //     //     break;
        //     // default:
        //     //     break;
        //     // }
        //     log_num --;
        // }
        logtime_minute = logcache[logindex_out].Item.deltatime;
        LOG_DEBUG("read log from RAM i:%d\n",lognum);
        switch (logcache[logindex_out].Item.operatetype)
        {
        case LOG_TIMETYPE:
            logtime_minute = logcache[logindex_out].Item.logvalue/60;
            if(logtime_minute > 0xFFFFFFE)
            {
                logtime_minute = 0xFFFFFFE;
            }
            LOG_DEBUG("LOG: LOG_TIMETYPE ");
            buffer[index++] = 0;
            WICED_BT_TRACE("%02x ",buffer[index-1]);
            buffer[index++] = 0|0x03;
            WICED_BT_TRACE("%02x ",buffer[index-1]);
            buffer[index++] = 0xE0|((logtime_minute>>24)&0x0F);
            WICED_BT_TRACE("%02x ",buffer[index-1]);
            buffer[index++] = (logtime_minute>>16)&0xFF;
            WICED_BT_TRACE("%02x ",buffer[index-1]);
            buffer[index++] = (logtime_minute>>8)&0xFF;
            WICED_BT_TRACE("%02x ",buffer[index-1]);
            buffer[index++] = (logtime_minute)&0xFF;
            WICED_BT_TRACE("%02x \n",buffer[index-1]);
            LOG_DEBUG("LOG: LOG_TIMEZONE ");
            buffer[index++] = 0;
            WICED_BT_TRACE("%02x ",buffer[index-1]);
            buffer[index++] = 0|0x03;
            WICED_BT_TRACE("%02x ",buffer[index-1]);
            buffer[index++] = 0xF8|((logtime_zone>>6)&0x03);
            WICED_BT_TRACE("%02x ",buffer[index-1]);
            buffer[index++] = (logtime_zone<<2)&0xFF;
            WICED_BT_TRACE("%02x  \n",buffer[index-1]);
            break;
        case LOG_ONOFFTYPE:
            LOG_DEBUG("LOG: LOG_ONOFFTYPE ");
            buffer[index++] = (logtime_minute>>6)&0xFF;
            WICED_BT_TRACE("%02x ",buffer[index-1]);
            buffer[index] = (logtime_minute<<2)&0xFC;
            if(logcache[logindex_out].Item.logvalue&(0x02<<16))
            {
                buffer[index] |= 0x01;
            }
            else
            {
                buffer[index] &= 0xFE;
            }
            WICED_BT_TRACE("%02x ",buffer[index++]);
            buffer[index] = logcache[logindex_out].Item.logvalue&0x7F;
            if(logcache[logindex_out].Item.logvalue&(0x01<<16))
            {
                buffer[index] |= 0x80;
            }
            else
            {
                buffer[index] |= 0x00;
            }
            WICED_BT_TRACE("%02x \n",buffer[index++]);
            break;
        case LOG_BRIGHTNESSTYPE:
            LOG_DEBUG("LOG: LOG_BRIGHTNESSTYPE ");
            buffer[index++] = (logtime_minute>>6)&0xFF;
            WICED_BT_TRACE("%02x ",buffer[index-1]);
            buffer[index++] = ((logtime_minute<<2)&0xFC|0x02);
            WICED_BT_TRACE("%02x ",buffer[index-1]);
            if(logcache[logindex_out].Item.logvalue > 100)
                logcache[logindex_out].Item.logvalue = 100;
            buffer[index++] = (logcache[logindex_out].Item.logvalue&0xFF);
            WICED_BT_TRACE("%02x \n",buffer[index-1]);
            break;
        case LOG_DETLATYPE_BRIGHTNESS:
            LOG_DEBUG("LOG: LOG_DETLATYPE_BRIGHTNESS ");
            buffer[index++] = (logtime_minute>>6)&0xFF;
            WICED_BT_TRACE("%02x ",buffer[index-1]);
            buffer[index++] = ((logtime_minute<<2)&0xFC|0x03);
            WICED_BT_TRACE("%02x ",buffer[index-1]);
            buffer[index++] = (logcache[logindex_out].Item.logvalue>>18)&0x3F;
            WICED_BT_TRACE("%02x ",buffer[index-1]);
            buffer[index++] = ((logcache[logindex_out].Item.logvalue>>10)&0xC0) + (((logcache[logindex_out].Item.logvalue&0xFFFF)/100)&0x3F);
            WICED_BT_TRACE("%02x \n",buffer[index-1]);
            break;
        default:
            break;
        }
        logindex_out =  LOGINDEX_PLUS(logindex_out,1);
        lognum --;
        if(lognum == 0)
        {
            logindex_out = 0;
            logindex_in  = 0;
            break;
        }
        if(index > 99)
        {
            break;
        }
        
    }
    *len = index;
    LOG_VERBOSE("send log message length: %d   remain log message num: %d\n",index,lognum);
    //还有未上传的log
    if(lognum)  
    {
        return lognum;
    }
}

void llog_size(void)
{
}


// void Printflogs(void)
// {
// #if _DEBUG
//     uint16_t index;
//     uint16_t index1;
//     uint32_t logtime_minute;
//     uint16_t log_num = 0;

//     LOG_DEBUG("log num:%d \n", lognum);
    
//     if(lognum == 0)
//     {
//         return ;
//     }
    
//     // if(RESULT_OK != record_sum(0,&log_num))
//     // {
//     //     return;
//     // }
//     // if(log_num > lognum)
//     // {
//     //     log_num = lognum;
//     // }

//     index = lognum;
//     index1 = logindex_out;
//     while((logindex_in != index1) || (lognum == LOGMAXSIZE))
//     {
//         // if(log_num > 0)
//         // {
//         //     if(RESULT_OK != record_read(0,log_num,&logrecord))
//         //     {
//         //         LOG_WARNING("read log from SFLASH ERR!!!");
//         //     }
//         //     log_num --;
//         //     LOG_DEBUG("read log from SFLASH ");
//         //     switch (logrecord.Item.operatetype)
//         //     {
//         //     case LOG_TIMETYPE:
//         //         LOG_DEBUG("LOG: LOG_TIMETYPE ");
//         //         break;
//         //     case LOG_ONOFFTYPE:
//         //         LOG_DEBUG("LOG: LOG_ONOFFTYPE ");
//         //         break;
//         //     case LOG_BRIGHTNESSTYPE:
//         //         LOG_DEBUG("LOG: LOG_BRIGHTNESSTYPE ");
//         //         break;
//         //     case LOG_DETLATYPE_BRIGHTNESS:
//         //         LOG_DEBUG("LOG: LOG_DETLATYPE_BRIGHTNESS ");
//         //         break;
//         //     default:
//         //         LOG_DEBUG("LOG: Not support ");
//         //         break;
//         //     }
//         //     logtime_minute =  logrecord.Item.deltatime;
//         //     LOG_DEBUG("%06x  %08x  %08x\n",logtime_minute, logrecord.Item.lasttime, logrecord.Item.logvalue);
//         //     LOG_DEBUG("%d  %d\n", logtime_minute, logrecord.Item.logvalue);
//         // }
//         LOG_DEBUG("read log from RAM ");
//         switch (logcache[index1].Item.operatetype)
//         {
//         case LOG_TIMETYPE:
//             LOG_DEBUG("LOG: LOG_TIMETYPE ");
//             break;
//         case LOG_ONOFFTYPE:
//             LOG_DEBUG("LOG: LOG_ONOFFTYPE ");
//             break;
//         case LOG_BRIGHTNESSTYPE:
//             LOG_DEBUG("LOG: LOG_BRIGHTNESSTYPE ");
//             break;
//         case LOG_DETLATYPE_BRIGHTNESS:
//             LOG_DEBUG("LOG: LOG_DETLATYPE_BRIGHTNESS ");
//             break;
//         default:
//             LOG_DEBUG("LOG: Not support ");
//             break;
//         }
//         logtime_minute =  logcache[index1].Item.deltatime;
//         LOG_DEBUG("%06x  %08x  %08x\n",logtime_minute, logcache[index1].Item.lasttime, logcache[index1].Item.logvalue);
//         LOG_DEBUG("%d  %d\n", logtime_minute, logcache[index1].Item.logvalue);
//         index1 = LOGINDEX_PLUS(index1,1);
//         index--;
//         if(index == 0)
//         {
//             break;
//         }
//     }
// #endif
// }

#endif

