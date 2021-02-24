#include "wiced_rtos.h"

#include "wiced_memory.h"
#include "vendor.h"
#include "systimer.h"
#include "lightPackage.h"
#include "src_provisioner/mesh_provisioner_node.h"

#define vendorFreeBuffer(p)       do{           \
    wiced_bt_free_buffer(p);                    \
    p = NULL;                                   \
}while(0)

wiced_timer_t vendorPackDelayTimer;

VendorCmdRPLItem *VendorCmdRpl;
wiced_bt_device_address_t  own_bd_addr;
packageReply waitforSendPack;
wiced_bool_t SendVendorMsg = WICED_FALSE;
uint16_t SendVendorMsgCnt = 0;

// wiced_timer_t SystemUpCounterTimer;


void send_callback(wiced_bt_mesh_event_t *p_event)
{
    SendVendorMsg = WICED_FALSE;
    LOG_VERBOSE("release event\n");
    wiced_bt_mesh_release_event(p_event);
    p_event = NULL;
    SendVendorMsgCnt = 0;
}


void mesh_vendor_server_send_data(wiced_bt_mesh_event_t *p_event, uint16_t opcode, uint8_t *p_data, uint16_t data_len)
{
    LOG_DEBUG("op:%02x,dst:%04x\n",opcode,p_event->dst);
    p_event->opcode = opcode;
    p_event->retrans_cnt  = 0;
    WICED_BT_TRACE_ARRAY(p_data,data_len,"\nAndon Send ");
    obfs(p_data, data_len);
    p_data[0] = Auth(p_data, data_len);
    WICED_BT_TRACE_ARRAY(p_data,data_len,"\nAndon Send ");
    wiced_bt_mesh_core_send(p_event, p_data, data_len, send_callback);
}

void vendorPackDelayTimerCb(uint32_t para)
{
    static wiced_bt_mesh_event_t *p_event_rep = NULL;

    if(0 == wiced_bt_mesh_core_get_local_addr()) //未入网的时候，不允许发送mesh msg
    {
        if(waitforSendPack.p_data != NULL)
        {
            vendorFreeBuffer(waitforSendPack.p_data);
        }
        return;
    }
    
    if((SendVendorMsg == WICED_TRUE))
    {
        SendVendorMsgCnt++;
        if(SendVendorMsgCnt > 10)
        {
            SendVendorMsg    = WICED_FALSE;
            SendVendorMsgCnt = 0;
        }
        else
        {
            wiced_start_timer(&vendorPackDelayTimer,50);  //等待50ms再次尝试发送
            return;
        }
    }

    if(NULL != p_event_rep)   //如果之前没有被释放内存 先释放之前申请的内存
    {
        wiced_bt_mesh_release_event(p_event_rep);
    }
    wiced_stop_timer(&vendorPackDelayTimer);
    
    SendVendorMsgCnt = 0;
    p_event_rep = wiced_bt_mesh_create_event(MESH_VENDOR_ELEMENT_INDEX, MESH_VENDOR_COMPANY_ID, MESH_VENDOR_MODEL_ID, 0, 0);
    if (p_event_rep == NULL) 
    {
        LOG_DEBUG("create event error\n");
    }
    else
    {
        if(waitforSendPack.p_data != NULL)
        {
            SendVendorMsg = WICED_TRUE;
            p_event_rep->dst    = waitforSendPack.dst ;
            p_event_rep->reply  = 0;
            mesh_vendor_server_send_data(p_event_rep, waitforSendPack.opcode, waitforSendPack.p_data, waitforSendPack.pack_len);
        }
    }
    if(waitforSendPack.p_data != NULL)
    {
        vendorFreeBuffer(waitforSendPack.p_data);
    }
}


wiced_bool_t appendVendorCmdRpl(uint16_t src, uint8_t cno)
{
    VendorCmdRPLItem temp;
    uint32_t systimer;
    int slot = 0;
    temp.src = src;
    temp.cno = cno;
    // LOG_VERBOSE("hash: %x\n", temp.hash);
    systimer = sysTimerGetSystemRunTime();
    for (int i = 0; i < VENDOR_CMD_RPL_SIZE; i++)
    {
        if ((temp.hash == VendorCmdRpl[i].hash) && (VendorCmdRpl[i].arrival_time + CMD_TIMEOUT > systimer))
        {
            // already exists
            return FALSE;
        }
        if (VendorCmdRpl[i].arrival_time < VendorCmdRpl[slot].arrival_time)
        {
            slot = i;
        }
    }

    VendorCmdRpl[slot].src = src;
    VendorCmdRpl[slot].cno = cno;
    VendorCmdRpl[slot].arrival_time = systimer;
    return TRUE;
}



void InitVendor()
{
    //SystemUTCtimer = 0;

    if(VendorCmdRpl == NULL)
    {
        VendorCmdRpl =  (VendorCmdRPLItem *)wiced_memory_permanent_allocate(sizeof(VendorCmdRPLItem) * VENDOR_CMD_RPL_SIZE);
    }
    memset(VendorCmdRpl, 0, sizeof(VendorCmdRPLItem) * VENDOR_CMD_RPL_SIZE);
    wiced_init_timer(&vendorPackDelayTimer, vendorPackDelayTimerCb, 0, WICED_MILLI_SECONDS_PERIODIC_TIMER);
    wiced_bt_dev_read_local_addr(own_bd_addr);
}




void OnVendorPktArrive(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint16_t data_len)
{
    LOG_DEBUG("src: 0x%04x  dst:0x%04x  cid:0x%04x\n", p_event->src, p_event->dst, p_event->company_id);
    
    if (data_len < 4)
    {
        LOG_DEBUG("message length short\n");
        return;
    }    
    if (Auth(p_data, data_len) != 0)
    {
        LOG_DEBUG("message auth err\n");
        return;
    }
    #if LOGLEVEL >= LOGLEVEL_DEBUG
    {
        LOG_DEBUG("message: ");
        WICED_BT_TRACE_ARRAY(p_data, data_len, "\n");
    }
    #endif
    obfs(p_data, data_len);
    
    #if LOGLEVEL >= LOGLEVEL_DEBUG
    {
        LOG_DEBUG("message: ");
        WICED_BT_TRACE_ARRAY(p_data, data_len, "\n");
    }
    #endif
    
    //收到了自己给自己的回复，此处增加处理是由于appendVendorCmdRpl会过滤掉相同的seq的数据包
    if( p_event->src == wiced_bt_mesh_core_get_local_addr()) 
    {
        return;
    }
    if(appendVendorCmdRpl(p_event->src, p_data[1]))
    {
        // LOG_VERBOSE("rssi: %d\n", p_event->rssi);
        // LOG_VERBOSE("src: %d CNO: %d\n", p_event->src, p_data[1]);
       
        packageReply replyAck;
        replyAck = lightpackUnpackMsg(p_data,data_len);
        if(replyAck.p_data != NULL)
        {
            if(replyAck.result == lightpackageSUCCESS)
            {
                if((p_event->opcode == MESH_VENDOR_OPCODE_GET) || (p_event->opcode == MESH_VENDOR_OPCODE_SET) ) 
                {
                    if (wiced_is_timer_in_use(&vendorPackDelayTimer))
                    {
                        wiced_stop_timer(&vendorPackDelayTimer);
                    }
                    if(waitforSendPack.p_data != NULL)  //前面的还在等待发送，取消发送，不发了
                    {
                        vendorFreeBuffer(waitforSendPack.p_data);
                    }
                    waitforSendPack.p_data   = replyAck.p_data;
                    waitforSendPack.pack_len = replyAck.pack_len;
                    waitforSendPack.dst      = p_event->src; 
                    waitforSendPack.opcode   = MESH_VENDOR_OPCODE_STATUS; 
                    wiced_start_timer(&vendorPackDelayTimer,own_bd_addr[5]%50);
                    //mesh_vendor_server_send_data(wiced_bt_mesh_create_reply_event(p_event),MESH_VENDOR_OPCODE_STATUS, replyAck.p_data, replyAck.pack_len);
                }
                else if(p_event->opcode == MESH_VENDOR_OPCODE_STATUS)  //如果是状态通知，通过GATT转发
                {
                    // //不会收到自己被动发送（回复其他设备）的状态信息，会由于seq相同被过滤
                    // #ifdef DEF_ANDON_GATT
                    // AndonGattSendMeshMsg(data_len,replyAck.p_data, p_event->src);
                    // #endif
                    vendorFreeBuffer(replyAck.p_data);
                }
                else
                {
                    vendorFreeBuffer(replyAck.p_data);
                }
                // //开关控制灯使用此OPCODE，当开关控制灯改变状态时会向网络发送消息
                // else if(p_event->opcode == MESH_VENDOR_OPCODE_SET_UNACKED)
                // {   
                //     vendorSendDevStatus();
                // }
            }
            
        }
    }
    else
    {
        LOG_VERBOSE("Resend message\n");
    }
    
}

extern wiced_bt_mesh_vendor_group_t *wiced_bt_mesh_vendor_get_group(uint8_t group);
//*****************************************************************************
// 函数名称: vendorSendDevStatus
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
void vendorSendDevStatus(void)
{
    packageReply reply;
    
    reply = lightpackNotifyDevStata();
    if(reply.result == lightpackageMEMORYFAILED)
    {
        return;
    }

    if (wiced_is_timer_in_use(&vendorPackDelayTimer))
    {
        wiced_stop_timer(&vendorPackDelayTimer);
    }
    if(waitforSendPack.p_data != NULL)  //前面的还在等待发送，取消发送，不发了
    {
        vendorFreeBuffer(waitforSendPack.p_data);
    }
    waitforSendPack.p_data   = reply.p_data;
    waitforSendPack.pack_len = reply.pack_len;
    waitforSendPack.dst      = wiced_bt_mesh_vendor_get_group(MESH_VENDOR_GROUP_PROVISIONER)->addr; 
    waitforSendPack.opcode   = MESH_VENDOR_OPCODE_STATUS; 
    wiced_start_timer(&vendorPackDelayTimer,own_bd_addr[5]%500+3500);

    
    // wiced_bt_mesh_event_t *p_event_rep;
    // p_event_rep = wiced_bt_mesh_create_event(MESH_VENDOR_ELEMENT_INDEX, MESH_VENDOR_COMPANY_ID, MESH_VENDOR_MODEL_ID, 0, 0);
    // if (p_event_rep == NULL) 
    // {
    //     LOG_DEBUG("create event error\n");
    // }
    // else
    // {
    //     wiced_bt_mesh_vendor_group_t *ptrgroup;
    //     ptrgroup = wiced_bt_mesh_vendor_get_group(MESH_VENDOR_GROUP_PROVISIONER);
    //     p_event_rep->dst    = ptrgroup->addr;
    //     p_event_rep->reply  = 0;
    //     mesh_vendor_server_send_data(p_event_rep, MESH_VENDOR_OPCODE_STATUS, reply.p_data, reply.pack_len);
    // }
    //wiced_bt_free_buffer(reply.p_data);
}

//*****************************************************************************
// 函数名称: vendorSendDevStatus
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
void vendorSendDevResetStatus(void)
{
    packageReply reply;
    
    reply = lightpackNotifyDevResetStata();
    if(reply.result == lightpackageMEMORYFAILED)
    {
        return;
    }

    if (wiced_is_timer_in_use(&vendorPackDelayTimer))
    {
        wiced_stop_timer(&vendorPackDelayTimer);
    }
    if(waitforSendPack.p_data != NULL)  //前面的还在等待发送，取消发送，不发了
    {
        vendorFreeBuffer(waitforSendPack.p_data);
    }
    waitforSendPack.p_data   = reply.p_data;
    waitforSendPack.pack_len = reply.pack_len;
    waitforSendPack.dst      = wiced_bt_mesh_vendor_get_group(MESH_VENDOR_GROUP_PROVISIONER)->addr; 
    waitforSendPack.opcode   = MESH_VENDOR_OPCODE_STATUS; 
    wiced_start_timer(&vendorPackDelayTimer,own_bd_addr[5]%500+3500);

}



