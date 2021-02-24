#if _ANDONDEBUG
#include <string.h>
#include <stdio.h>
#include "log.h"

#include "wiced_bt_gatt.h"

#define HANDLE_ANDON_DATA_OUT_VALUE 0x62

extern uint16_t conn_id;
static char header[128];
static char log_buffer[128];
static char total_buffer[256];

char *file_name_of(char *file)
{
    // const total_len = strlen(file);
    char *p = file + strlen(file);

    while ((p > file) && (*p != '/') && (*p != '\\'))
    {
        p--;
    }

    if ((*p == '/') || (*p == '\\'))
        p++;

    return p;
}

void my_sprintf(char *str, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vsprintf(str, format, args);
    va_end(args);
}

void write_multi_routine(const char *format, const char *file, int line, ...)
{
    va_list args;
    my_sprintf(header, "%s, %d: ", file_name_of((char *)file), line);
	//my_sprintf(header, "%s, %d: ", file, line);
    va_start(args, line);
    vsprintf(log_buffer, format, args);
    va_end(args);

    my_sprintf(total_buffer, "%s %s", header, log_buffer);

    if (conn_id != 0)
    {
        //TODO: send to gatt client
        // wiced_bt_gatt_send_notification(conn_id, HANDLE_ANDON_DATA_OUT_VALUE, strlen(total_buffer), total_buffer);
    }

    wiced_printf(NULL,0,total_buffer);
}

#if LOGLEVEL >= LOGLEVEL_VERBOSE
extern void PrintfRemote(void);
extern void PrintfAutoBrightness(void);
extern void PrintfXxteakeyandsn(void);
extern void PrintfReceiveMeshMessageCnt(void);
// extern void ClearReceiveMeshMessageCnt(void);
// extern void ReadReplayMeshMessageCnt(void);
// extern void CheckNeighborNodeRssi(void);
void PrintfBufUse(void);

typedef struct
{  
    char const *cmd_name;                        //命令字符串  
    int32_t max_args;                            //最大参数数目  
    void (*handle)(int argc,void * cmd_arg);     //命令回调函数  
    char  *help;                                 //帮助信息  
 }cmd_list_struct;

const cmd_list_struct cmd_list[]=
{
	{"ReadBufUse",0,PrintfBufUse,""},
    {"ReadRemote",0,PrintfRemote,""},
    {"ReadAutoBrightness",0,PrintfAutoBrightness,""},
	{"ReadXxteaInfo",0,PrintfXxteakeyandsn,""},
	// {"ReadReceiveCnt",0,PrintfReceiveMeshMessageCnt,""},
	// {"ClearReceiveCnt",0,ClearReceiveMeshMessageCnt,""},
	// {"ReadRelayCnt",0,ReadReplayMeshMessageCnt,""},
	// {"ReadNodeRssi",0,CheckNeighborNodeRssi,""},
};

//******************************************************************************
//*
//* 函数名称 : ulGetTrueCharFromStream
//* 函数描述 : 使用SecureCRT串口收发工具,在发送的字符流中可能带有不需要的字符以及控制字符, 
//*             比如退格键,左右移动键等等,在使用命令行工具解析字符流之前,需要将这些无用字符以 
//*             及控制字符去除掉. 
//*             支持的控制字符有: 
//*             上移:1B 5B 41 
//*             下移:1B 5B 42 
//*             右移:1B 5B 43 
//*             左移:1B 5B 44 
//*             回车换行:0D 0A 
//*             Backspace:08 
//*             Delete:7F      
//* 函数输入 : dest：去除不需要字符后命令存放地址 src：原始接收字符命令
//* 函数输出 : 无
//* 函数返回值 : 无
//* 使用说明 : 无
//* 作    者 ：zhw
//*                   
//******************************************************************************
static uint32_t ulGetTrueCharFromStream(char *dest,const char *src)  
{
	uint32_t dest_count=0;  
	uint32_t src_count=0;  
  
	while(src[src_count] != '\r' && src[src_count+1] != '\n')  
    //while(src[src_count] != '\n')  
	{
		if((src[src_count]>0x1F)&&(src[src_count]<0x7F))  //是可打印字符进行保存
		{  
		    dest[dest_count++]=src[src_count++];  
		}  
		else  
		{  
            switch(src[src_count])  
            {  
                case 0x08:                          //退格键键值  
				{  
					if(dest_count>0)  
					{  
						dest_count --;  
					}  
					src_count ++;  
					break;
				}  
				case 0x1B:  
				{  
				    if(src[src_count+1]==0x5B)  
				    {  
				        if(src[src_count+2]==0x41 || src[src_count+2]==0x42)  
				        {  
				            src_count +=3;              //上移和下移键键值  
				        }  
				        else if(src[src_count+2]==0x43)  
				        {  
				            dest_count++;               //右移键键值  
				            src_count+=3;  
				        }  
				        else if(src[src_count+2]==0x44)  
				        {  
				            if(dest_count >0)           //左移键键值  
				            {  
				                dest_count --;  
				            }  
				           src_count +=3;  
				        }  
				        else  
				        {  
				            src_count +=3;  
				        }  
				    }  
				    else  
				    {  
				        src_count ++;  
				    } 
				    break; 
				}  
				default:  
				{  
				    src_count++;  
				    break; 
				} 
			}  
		}  
	}
	  
	dest[dest_count++]=0;  
	//dest[dest_count++]=src[src_count++];  
	return dest_count;  
}  

uint8_t cmdlinecache[50]={0};
uint8_t cmdlinecacheindex = 0;
void Cmdline_Receive_Handle(void *data)
{
    uint8_t i;
    wiced_hal_puart_read(cmdlinecache+cmdlinecacheindex);
    wiced_hal_puart_reset_puart_interrupt();
    //i = *data;
    //LOG_VERBOSE("%s\n",cmdlinecache);
    if(cmdlinecacheindex>2)
    {
        //找到了结尾符
        if('\n'==cmdlinecache[cmdlinecacheindex] && '\r'==cmdlinecache[cmdlinecacheindex-1])  
        //if('\n'==cmdlinecache[cmdlinecacheindex])
        {
            uint8_t ulrec_num;
            ulrec_num=ulGetTrueCharFromStream(cmdlinecache,cmdlinecache);
            LOG_DEBUG("%s \n",cmdlinecache);
            for(i=0;i<sizeof(cmd_list)/sizeof(cmd_list[0]);i++)
        	{
        	    if(!strcmp(cmdlinecache,cmd_list[i].cmd_name))       //字符串相等
        	    {
        	        cmd_list[i].handle(0,0);
        	    }
            }
        	if(i>sizeof(cmd_list)/sizeof(cmd_list[0]))
        	{
        	    LOG_DEBUG("not support!\n");
        	}
            cmdlinecacheindex = 0;
            memset(cmdlinecache,0,sizeof(cmdlinecache));
            return;
        }
    }
    cmdlinecacheindex ++;
    if(cmdlinecacheindex > 49)
        cmdlinecacheindex = 0;
}

void PrintfBufUse(void) 
{
	_deb_print_buf_use();
}
#endif
#endif

