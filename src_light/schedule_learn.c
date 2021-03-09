//******************************************************************************
//*
//* 文 件 名 : schedule_learn.c
//* 文件描述 : 亮度模型学习相关处理     
//* 作    者 : 李芹/Andon Health CO.Ltd
//* 版    本 : V0.0
//* 日    期 : 
//*
//* 更新历史 : 
//*     日期       作者    版本     描述
//*         
//*          
//******************************************************************************
#include "schedule_learn.h" //lq20200616
#include "light_model.h"
#include "wiced_timer.h"
#include "storage.h"
#include "vendor.h"
#include "systimer.h"
#include "wiced_platform.h"

///*****************************************************************************
///*                         函数声明区
///*****************************************************************************
static uint16_t APlusBModX(uint32_t A, uint32_t B, uint32_t X);
static uint16_t AMinusBModX(uint32_t A, uint32_t B, uint32_t X);
static uint8_t optrXPlus(uint8_t optr, uint8_t a, uint8_t X);
static uint8_t optrXMinus(uint8_t optr, uint8_t a, uint8_t X);
static void CenterUpdate(void);
static uint8_t CenterPos(uint16_t buffertime);
static void ModelTimeFilter(uint8_t pos, uint8_t filtvalue);
static void BrightStateFilter(uint8_t Brightfilter);
static void NewmodelPreprocess(void);
static void Compress(void);
static void CalTbulb1(void);

static void BrightGradualChange(uint32_t);//lq20200622 定时器
// static int32_t brightness_procedure_0(int32_t tick, int32_t period, int32_t initiate, int32_t final);//lq20200622 定时器
// static int32_t liner_transfer0(int32_t tick, int32_t period, int32_t initiate, int32_t final); //lq20200622 定时器

extern uint16_t percentage_to_uint16(int8_t percentage);
extern int32_t liner_transfer(int32_t tick, int32_t period, int32_t initiate, int32_t final);
///*****************************************************************************
///*                         变量定义区
///*****************************************************************************
int AxisDis = 0;
uint16_t modeltimenew = 0;               //当前点模型时间，=0-1439
uint8_t  BrightStatenew = 255;              //当前点亮度，=0-100     
uint8_t centerlen = 0;   //“中心”实际长度  
uint8_t bufferlog_optr = 0; //=0\1\2 
uint8_t flagdayone = 1;  // 第一天数据的标志位, 初始化为第1天
uint8_t Flagnewmodel = 0; //下载模型时标志位，新下载时=1
uint32_t SystemUTCtimer0 = 0; //lq20200616 

BufData_def center[CENTERLENMAX];
BufData_def bufferlog[BUFLOGMAXSIZE];
AutoBrightnessSet_def AutoBrightnessSet;
const AutoBrightnessSet_def DEFAULT_AUTOLIGHTNESS_CONFIG ={ 
    .Item = {                          
        .AutoBrightnessNum = 0,    
        .Flag = 0,     //1,   //lq20200420 初始化   
        .Tbulb1 = 0,          //lq20200420 初始化
        .Tbulb2 = 0,          //lq20200420 初始化
        .flagdayone = 0,
        .AutoBrightnessPoint = {0}                        
    }                                  
}; 
wiced_timer_t BrightCheckTimer;//lq20200622 定时器
uint16_t BrightDuration = 0;//lq20200622 定时器
//uint8_t lightnessLevel0Flag = 0;//lq20200622 定时器 0/1, 1:lightnessLevel=0
wiced_timer_t BrightGradualTimer;//lq20200622 定时器
// Animination ani_table0[] = {     //lq20200622 定时器
//     liner_transfer0};
struct  //lq20200622 定时器
{
    Animination anim;
    int32_t tick;
    int32_t period;
    int32_t initiate;
    int32_t final;
    //uint16_t PrePowerTick;
} ctx0 = {0};


//lq20200420 
// void AutoAdjustBrightness(void)    //zhw 20200828 
uint8_t AutoAdjustBrightness(void)    //zhw 20200828 
{
    uint16_t i=0;
    uint16_t pro_minute;
    //uint32_t timetemp;
    //uint64_t timetemp1;
    uint16_t operationtime;  //lq20200420
    wiced_bool_t mesh_app_node_is_provisioned(void);

    //lq20200618 -最近模型时间段内的亮度----
    uint16_t distmp1=0, distmp2=0;       
    uint8_t postmp1=255, postmp2=255;
    uint8_t j=0;
    uint8_t autolightnesspercent=0;  //zhw 20200828 
    //-------------------

    autolightnesspercent = 0;    //zhw 20200828 
    //lq20200420
    if((AutoBrightnessSet.Item.AutoBrightnessNum < 1) || (AutoBrightnessSet.Item.FlagModelOn == 0))  
    {
        return 0;
    }
    uint32_t modeltimetmp = 0; //lq20200420
    //timetemp1 = SystemUTCtimer0;
    //timetemp1 = timetemp1 + 1000000000 + 8*60*60;
    modeltimetmp = (SystemUTCtimer0 + 30) / 60;          //lq20200420
    operationtime = APlusBModX(modeltimetmp, 0, 1440);   //lq20200420
    if (AxisDis >= 0)   //lq20200420
    {
        operationtime = APlusBModX((uint32_t)operationtime, (uint32_t)AxisDis, 1440); 
    }
    else
    {
        operationtime = AMinusBModX((uint32_t)operationtime, (uint32_t)( 0 - AxisDis), 1440); 
    }    

    //timetemp = SystemUTCtimer+(1000000000%(60*1440));
    //timetemp = (uint32_t)((timetemp1/60)%1440); 
    LOG_DEBUG("operationtime = %d AxisDis =%d\n",operationtime,AxisDis);
    
    if(AutoBrightnessSet.Item.AutoBrightnessNum > LIGHT_AUTOBRIGHTNESSMAXNUM)
    {
        AutoBrightnessSet.Item.AutoBrightnessNum = LIGHT_AUTOBRIGHTNESSMAXNUM;
    }

    i = AutoBrightnessSet.Item.AutoBrightnessNum - 1;
    if(i == 0)
    {
        if(AutoBrightnessSet.Item.AutoBrightnessPoint[0][1] > 0)
        {
            //zhw 20200828 start
            autolightnesspercent = AutoBrightnessSet.Item.AutoBrightnessPoint[0][1];
            // LightConfig.lightnessLevel = percentage_to_uint16(AutoBrightnessSet.Item.AutoBrightnessPoint[0][1]); 
            //zhw 20200828 end
            //lightnessLevel0Flag = 0; //lq20200622 定时器
        }
        // else
        // {
        //      //LightConfig.lightnessLevel = percentage_to_uint16(1);  //lq20200602 1%            
        //      //lightnessLevel0Flag = 1; //lq20200622 定时器
        // }

        //zhw 20200828 start
        // LOG_VERBOSE("lightnessLevel = %d\n",LightConfig.lightnessLevel);
        // return;

        LOG_VERBOSE("lightnessLevel = %d\n",autolightnesspercent);
        return autolightnesspercent;
        //zhw 20200828 end
    }
    pro_minute = AutoBrightnessSet.Item.AutoBrightnessPoint[0][0];
    //pro_minute = pro_minute *10;//lq20200618 精度1min时屏蔽

    if(operationtime < pro_minute)  //lq20200420
    {
        if(AutoBrightnessSet.Item.AutoBrightnessPoint[i][1] > 0)
        {
            //zhw 20200828 start
            // LightConfig.lightnessLevel = percentage_to_uint16(AutoBrightnessSet.Item.AutoBrightnessPoint[i][1]);
            //lightnessLevel0Flag = 0; //lq20200622 定时器

            autolightnesspercent = AutoBrightnessSet.Item.AutoBrightnessPoint[i][1];
            //zhw 20200828 end
        }
        else
        {
            //LightConfig.lightnessLevel = percentage_to_uint16(1);  //lq20200602 1%
            //lq20200618  最近模型时间段内的亮度------------            
            while(i >= 1)
            {
                if(AutoBrightnessSet.Item.AutoBrightnessPoint[i-1][1] > 0)
                {
                    postmp1 = i-1;
                    break;
                }
                i--;
            } 
            j = 0;
            while(j <= AutoBrightnessSet.Item.AutoBrightnessNum - 1)
            {
                if(AutoBrightnessSet.Item.AutoBrightnessPoint[j][1] > 0)
                {
                    postmp2 = j;
                    break;
                }
                j++;
            }
            if((postmp1<AutoBrightnessSet.Item.AutoBrightnessNum)&&(postmp2<AutoBrightnessSet.Item.AutoBrightnessNum)){
                distmp1 = AMinusBModX(operationtime, AutoBrightnessSet.Item.AutoBrightnessPoint[postmp1 + 1][0], 1440);
                distmp2 = AMinusBModX(AutoBrightnessSet.Item.AutoBrightnessPoint[postmp2][0], operationtime, 1440);   
            }

            //zhw 20200828 start         
            // if(distmp1 <= distmp2)
            // {
            //     LightConfig.lightnessLevel = percentage_to_uint16(AutoBrightnessSet.Item.AutoBrightnessPoint[postmp1][1]);            
            // }
            // else
            // {
            //     LightConfig.lightnessLevel = percentage_to_uint16(AutoBrightnessSet.Item.AutoBrightnessPoint[postmp2][1]); 
            // }
            
            if(distmp1 <= distmp2)
            {
                autolightnesspercent = AutoBrightnessSet.Item.AutoBrightnessPoint[postmp1][1];           
            }
            else
            {
                autolightnesspercent = AutoBrightnessSet.Item.AutoBrightnessPoint[postmp2][1];      
            }
            //zhw 20200828 end

            //distmp1 = AMinusBModX(operationtime, AutoBrightnessSet.Item.AutoBrightnessPoint[i][0], 1440);
            //distmp2 = AMinusBModX(AutoBrightnessSet.Item.AutoBrightnessPoint[0][0], operationtime, 1440);
            // if(distmp1 <= distmp2)
            // {
            //     while(i >= 1)
            //     {
            //         if(AutoBrightnessSet.Item.AutoBrightnessPoint[i-1][1] > 0)
            //         {
            //             LightConfig.lightnessLevel = percentage_to_uint16(AutoBrightnessSet.Item.AutoBrightnessPoint[i-1][1]);                
            //             break;
            //         }
            //         i--;                    
            //     }
            //     //LightConfig.lightnessLevel = percentage_to_uint16(AutoBrightnessSet.Item.AutoBrightnessPoint[i-1][1]);            
            // }
            // else
            // {
            //     uint8_t j = 0;
            //     while(j <= AutoBrightnessSet.Item.AutoBrightnessNum - 1)
            //     {
            //         if(AutoBrightnessSet.Item.AutoBrightnessPoint[j][1] > 0)
            //         {
            //             LightConfig.lightnessLevel = percentage_to_uint16(AutoBrightnessSet.Item.AutoBrightnessPoint[j][1]); 
            //             break;
            //         }
            //         j++;
            //     }
            //     //LightConfig.lightnessLevel = percentage_to_uint16(AutoBrightnessSet.Item.AutoBrightnessPoint[0][1]); 
            // }
            //----------------------------------------
            //lightnessLevel0Flag = 1; //lq20200622 定时器
            
        }
        //zhw 20200828 start
        // LOG_VERBOSE("lightnessLevel = %d\n",LightConfig.lightnessLevel);
        // return;

        LOG_VERBOSE("lightnessLevel = %d\n",autolightnesspercent);
        return autolightnesspercent;
        //zhw 20200828 end
    }

    i = AutoBrightnessSet.Item.AutoBrightnessNum - 1; //lq20200618
    do
    {
        pro_minute = AutoBrightnessSet.Item.AutoBrightnessPoint[i][0];
        //pro_minute = pro_minute *10;  //lq20200618 精度1min时 屏蔽
                
        if(operationtime >= pro_minute)   //lq20200420
        {
            if(AutoBrightnessSet.Item.AutoBrightnessPoint[i][1] > 0)
            {
                //zhw 20200828 start
                // LightConfig.lightnessLevel = percentage_to_uint16(AutoBrightnessSet.Item.AutoBrightnessPoint[i][1]);
                //lightnessLevel0Flag = 0; //lq20200622 定时器

                autolightnesspercent = AutoBrightnessSet.Item.AutoBrightnessPoint[i][1];
                //zhw 20200828 end
            }
            else
            {
                //LightConfig.lightnessLevel = percentage_to_uint16(1);  //lq20200602 1%
                //lq20200618  最近模型时间段内的亮度------------
                while(i >= 1)
                { 
                    if(AutoBrightnessSet.Item.AutoBrightnessPoint[i-1][1] > 0)
                    { 
                        postmp1 = i-1;  
                        break; 
                    }
                    i--;
                } 
                j = optrXPlus(i,1,AutoBrightnessSet.Item.AutoBrightnessNum);                
                while(j <= AutoBrightnessSet.Item.AutoBrightnessNum - 1)
                { 
                    if(AutoBrightnessSet.Item.AutoBrightnessPoint[j][1] > 0)
                    { 
                        postmp2 = j;  
                        break; 
                    }
                    j++;
                }
                if((postmp1<AutoBrightnessSet.Item.AutoBrightnessNum)&&(postmp2<AutoBrightnessSet.Item.AutoBrightnessNum)){
                    distmp1 = AMinusBModX(operationtime, AutoBrightnessSet.Item.AutoBrightnessPoint[postmp1 + 1][0], 1440);
                    distmp2 = AMinusBModX(AutoBrightnessSet.Item.AutoBrightnessPoint[postmp2][0], operationtime, 1440);
                }

                //zhw 20200828 start         
                // if(distmp1 <= distmp2)
                // {
                //     LightConfig.lightnessLevel = percentage_to_uint16(AutoBrightnessSet.Item.AutoBrightnessPoint[postmp1][1]);            
                // }
                // else
                // {
                //     LightConfig.lightnessLevel = percentage_to_uint16(AutoBrightnessSet.Item.AutoBrightnessPoint[postmp2][1]); 
                // }
                
                if(distmp1 <= distmp2)
                {
                    autolightnesspercent = AutoBrightnessSet.Item.AutoBrightnessPoint[postmp1][1];           
                }
                else
                {
                    autolightnesspercent = AutoBrightnessSet.Item.AutoBrightnessPoint[postmp2][1];      
                }
                //zhw 20200828 end

                //next_minute = AutoBrightnessSet.Item.AutoBrightnessPoint[optrXPlus(i,1,AutoBrightnessSet.Item.AutoBrightnessNum)][0];
                // distmp1 = operationtime - pro_minute;
                // distmp2 = AMinusBModX(next_minute, operationtime, 1440);
                // if(distmp1 <= distmp2)
                // {
                //     LightConfig.lightnessLevel = percentage_to_uint16(AutoBrightnessSet.Item.AutoBrightnessPoint[i][1]);            
                // }
                // else
                // {
                //     LightConfig.lightnessLevel = percentage_to_uint16(AutoBrightnessSet.Item.AutoBrightnessPoint[optrXPlus(i,1,AutoBrightnessSet.Item.AutoBrightnessNum)][1]);            
                // }
                //------------------------------------------
                //lightnessLevel0Flag = 1; //lq20200622 定时器
            }
            //zhw 20200828 start
            // LOG_VERBOSE("lightnessLevel = %d\n",LightConfig.lightnessLevel);
            // return;

            LOG_VERBOSE("lightnessLevel = %d\n",autolightnesspercent);
            return autolightnesspercent;
            //zhw 20200828 end
        }
    }while(i--);
    // LOG_VERBOSE("lightnessLevel = %d\n",LightConfig.lightnessLevel);
    LOG_VERBOSE("AutolightnessLevel = 0\n");
    return 0;
}

//lq20200420新增函数
static uint16_t APlusBModX(uint32_t A, uint32_t B, uint32_t X)
{
   uint16_t result;
   A = A + B;
   while(A >= X)
   {
       A = A - X;
   }
   result = (uint16_t)A;
   return result;
}

//lq20200420新增函数
static uint16_t AMinusBModX(uint32_t A, uint32_t B, uint32_t X)
{
   uint16_t result;
   while (A < B)
   {
       A = A + X;
   }
   result =  (uint16_t)(A - B);
   return result;
}

//lq20200420新增函数
void BrightModelLearning(uint32_t SystemTnew, uint8_t Bnew)
{
    //lq20200617
    BrightStatenew = Bnew;
    modeltimenew = Calmodeltimenew(SystemTnew);
    

    LOG_DEBUG("modeltimenew= %d min SystemUTCtimer0= %d s  BrightStatenew= %d\n", modeltimenew, SystemUTCtimer0,BrightStatenew);
    
    if (centerlen > CENTERLENMAX)
    {
        return;
    }
    //第一天，第1点
    if ((flagdayone > 0) && (centerlen == 0))
    {
        center[0].modeltime = modeltimenew;
        center[0].BrightState = BrightStatenew;
        centerlen = 1;
        bufferlog_optr = 0;
        bufferlog[0].modeltime = modeltimenew;
        bufferlog[0].BrightState = BrightStatenew;
        return;
    }
    //第一天，第2点后
    if (flagdayone > 0)
    {
        if (modeltimenew >= bufferlog[bufferlog_optr].modeltime)
        {
            if (modeltimenew > (bufferlog[bufferlog_optr].modeltime + FILTVALUE))
            {
                center[centerlen].modeltime = modeltimenew;
                center[centerlen].BrightState = BrightStatenew;
                centerlen = optrXPlus(centerlen, 1, CENTERLENMAX);
                bufferlog_optr = optrXPlus(bufferlog_optr, 1, BUFLOGMAXSIZE);
                bufferlog[bufferlog_optr].modeltime = modeltimenew;
                bufferlog[bufferlog_optr].BrightState = BrightStatenew;
            }
            else
            {
                center[optrXMinus(centerlen, 1, CENTERLENMAX)].modeltime = modeltimenew;
                center[optrXMinus(centerlen, 1, CENTERLENMAX)].BrightState = BrightStatenew;
                bufferlog[bufferlog_optr].modeltime = modeltimenew;
                bufferlog[bufferlog_optr].BrightState = BrightStatenew;
            }
        }
        else
        {
            if (centerlen < 2)
            {
                center[0].modeltime = modeltimenew;
                center[0].BrightState = BrightStatenew;
                centerlen = 1;
                bufferlog_optr = 0;
                bufferlog[0].modeltime = modeltimenew;
                bufferlog[0].BrightState = BrightStatenew;
            }
            else
            {
                flagdayone = 0;
                AutoBrightnessSet.Item.flagdayone = flagdayone;
            }
        }
    }
    //第二天以后，非0点的[optr-1]点修正center模型
    if (flagdayone <= 0)
    {
        if (modeltimenew >= bufferlog[bufferlog_optr].modeltime)
        {
            if (modeltimenew > bufferlog[bufferlog_optr].modeltime + FILTVALUE)
            {
                bufferlog_optr = optrXPlus(bufferlog_optr, 1, BUFLOGMAXSIZE);
                bufferlog[bufferlog_optr].modeltime = modeltimenew;
                bufferlog[bufferlog_optr].BrightState = BrightStatenew;
            }
            else
            {
                bufferlog[bufferlog_optr].modeltime = modeltimenew;
                bufferlog[bufferlog_optr].BrightState = BrightStatenew;

                //lq20200512
                LOG_DEBUG("(return) centerlen= %d \n", centerlen);
                for (int i = 0 ; i < centerlen; i++)
                {
                    LOG_DEBUG("centeri= %d centeri.modeltime= %d centeri.BrightState= %d \n", i, center[i].modeltime, center[i].BrightState);
                }
                for (int j = 0; j < BUFLOGMAXSIZE; j++)
                {
                   LOG_DEBUG("buflogj= %d buflogj.modeltime= %d buflogj.BrightState= %d \n", j, bufferlog[j].modeltime, bufferlog[j].BrightState);
                }
                return;  
            }            
        }
        else
        {
            if (modeltimenew + 1440 > bufferlog[bufferlog_optr].modeltime + FILTVALUE) //lq20200610
            {
                bufferlog_optr = optrXPlus(bufferlog_optr, 1, BUFLOGMAXSIZE);
                bufferlog[bufferlog_optr].modeltime = modeltimenew;
                bufferlog[bufferlog_optr].BrightState = BrightStatenew;
            }
            else //lq20200610
            {   
                bufferlog[bufferlog_optr].modeltime = modeltimenew;
                bufferlog[bufferlog_optr].BrightState = BrightStatenew;
                return;
            }
        }
        //if ((bufferlog[optrXMinus(bufferlog_optr, 1, BUFLOGMAXSIZE)].BrightState > 0) &&
        //        (bufferlog[optrXMinus(bufferlog_optr, 1, BUFLOGMAXSIZE)].BrightState <= 100))
        //lq20200512
        if ((bufferlog[optrXMinus(bufferlog_optr, 2, BUFLOGMAXSIZE)].BrightState > 0) &&
                (bufferlog[optrXMinus(bufferlog_optr, 2, BUFLOGMAXSIZE)].BrightState <= 100))
        {
            CenterUpdate();
            
        }
    }
    Compress();

    //LOG_DEBUG("center[len-1].modeltime= %d center[len-1].BrightState= %d  buffer[optr].modeltime= %d buffer[optr].BrightState= %d \n", center[centerlen-1].modeltime, center[centerlen-1].BrightState,bufferlog[bufferlog_optr].modeltime,bufferlog[bufferlog_optr].BrightState);
    LOG_DEBUG("centerlen= %d \n", centerlen);
    for (int i = 0 ; i < centerlen; i++)
    {
        LOG_DEBUG("centeri= %d centeri.modeltime= %d centeri.BrightState= %d \n", i, center[i].modeltime, center[i].BrightState);
    }
    for (int j = 0; j < BUFLOGMAXSIZE; j++)
    {
        LOG_DEBUG("buflogj= %d buflogj.modeltime= %d buflogj.BrightState= %d \n", j, bufferlog[j].modeltime, bufferlog[j].BrightState);
    }

    return;
}


//lq20200420新增函数 
static void CenterUpdate(void)
{
    //亮度中心模型根据新操作log学习更新,只对[optr-2]的亮灯过程进行学习
    //容错
    if ((centerlen < 1)||(centerlen + 2 > CENTERLENMAX))
    {
        return;
    }
    //if (bufferlog[optrXMinus(bufferlog_optr, 1, BUFLOGMAXSIZE)].modeltime <= 0)
    if (bufferlog[optrXMinus(bufferlog_optr, 2, BUFLOGMAXSIZE)].modeltime <= 0)
    {
        return;
    }
    uint8_t centerpos1 = 0;
    uint8_t centerpos2 = 0;
    uint16_t Brighttmp = 0; 
    
    //centerpos1 = CenterPos(bufferlog[optrXMinus(bufferlog_optr, 2, BUFLOGMAXSIZE)].modeltime);
    //centerpos2 = CenterPos(bufferlog[optrXMinus(bufferlog_optr, 1, BUFLOGMAXSIZE)].modeltime);
    
    //if (centerpos1 < centerpos2)
    if (bufferlog[optrXMinus(bufferlog_optr, 2, BUFLOGMAXSIZE)].modeltime < bufferlog[optrXMinus(bufferlog_optr, 1, BUFLOGMAXSIZE)].modeltime) //lq20200602
    {
        //lq20200602
        centerpos1 = CenterPos(bufferlog[optrXMinus(bufferlog_optr, 2, BUFLOGMAXSIZE)].modeltime);
        centerpos2 = CenterPos(bufferlog[optrXMinus(bufferlog_optr, 1, BUFLOGMAXSIZE)].modeltime);
    
        //第centerpos1~centerpos2-1
        for (int i = centerpos1; i < centerpos2; i++)
        {
            if (center[i].BrightState > 0)
            {
                //Brighttmp = center[i].BrightState * PARTWEIGHT0 + bufferlog[optrXMinus(bufferlog_optr, 1, BUFLOGMAXSIZE)].BrightState * PARTWEIGHT1;
                Brighttmp = center[i].BrightState * PARTWEIGHT0 + bufferlog[optrXMinus(bufferlog_optr, 2, BUFLOGMAXSIZE)].BrightState * PARTWEIGHT1;
                center[i].BrightState = Brighttmp / (PARTWEIGHT0 + PARTWEIGHT1);
            }
            else
            {
                //center[i].BrightState = bufferlog[optrXMinus(bufferlog_optr, 1, BUFLOGMAXSIZE)].BrightState;
                center[i].BrightState = bufferlog[optrXMinus(bufferlog_optr, 2, BUFLOGMAXSIZE)].BrightState;
            }
        }
    }
    //else if (centerpos1 > centerpos2)
    else if (bufferlog[optrXMinus(bufferlog_optr, 2, BUFLOGMAXSIZE)].modeltime > bufferlog[optrXMinus(bufferlog_optr, 1, BUFLOGMAXSIZE)].modeltime)//lq20200602
    {
        //lq20200602
        centerpos2 = CenterPos(bufferlog[optrXMinus(bufferlog_optr, 1, BUFLOGMAXSIZE)].modeltime);
        centerpos1 = CenterPos(bufferlog[optrXMinus(bufferlog_optr, 2, BUFLOGMAXSIZE)].modeltime);
            
        //第centerpos1~centerlen-1
        for (int i = centerpos1; i < centerlen; i++)
        {
            if (center[i].BrightState > 0)
            {
                //Brighttmp = center[i].BrightState * PARTWEIGHT0 + bufferlog[optrXMinus(bufferlog_optr, 1, BUFLOGMAXSIZE)].BrightState * PARTWEIGHT1;
                Brighttmp = center[i].BrightState * PARTWEIGHT0 + bufferlog[optrXMinus(bufferlog_optr, 2, BUFLOGMAXSIZE)].BrightState * PARTWEIGHT1;
                center[i].BrightState = Brighttmp / (PARTWEIGHT0 + PARTWEIGHT1);
            }
            else
            {
                //center[i].BrightState = bufferlog[optrXMinus(bufferlog_optr, 1, BUFLOGMAXSIZE)].BrightState;
                center[i].BrightState = bufferlog[optrXMinus(bufferlog_optr, 2, BUFLOGMAXSIZE)].BrightState;
            }
        }
        //第0~centerpos2-1
        for (int i = 0; i < centerpos2; i++)
        {
            if (center[i].BrightState > 0)
            {
                //Brighttmp = center[i].BrightState * PARTWEIGHT0 + bufferlog[optrXMinus(bufferlog_optr, 1, BUFLOGMAXSIZE)].BrightState* PARTWEIGHT1;
                Brighttmp = center[i].BrightState * PARTWEIGHT0 + bufferlog[optrXMinus(bufferlog_optr, 2, BUFLOGMAXSIZE)].BrightState* PARTWEIGHT1;
                center[i].BrightState = Brighttmp / (PARTWEIGHT0 + PARTWEIGHT1);
            }
            else
            {
                //center[i].BrightState = bufferlog[optrXMinus(bufferlog_optr, 1, BUFLOGMAXSIZE)].BrightState;
                center[i].BrightState = bufferlog[optrXMinus(bufferlog_optr, 2, BUFLOGMAXSIZE)].BrightState;
            }
        }
    }

    //center模型后处理
    //(1)对新增点centerpos1、centerpos2，10min平滑,先删后面点
    if (centerpos1 < centerpos2)
    {
        ModelTimeFilter(centerpos2, FILTVALUE);
        ModelTimeFilter(centerpos1, FILTVALUE);
    }
    else if (centerpos1 > centerpos2)
    {
        ModelTimeFilter(centerpos1, FILTVALUE);
        ModelTimeFilter(centerpos2, FILTVALUE);
    }
    //(2)center模型整体亮度平滑,
    //若连续两点亮度>0，亮度偏差<+-BRIGHTFILTER，则前一点亮度=两段亮度积分平均,并删除后一点。循环判断。
    BrightStateFilter(BRIGHTFILTER);
    return;
}

//lq20200420新增函数
static uint8_t optrXPlus(uint8_t optr, uint8_t a, uint8_t X)
{
    optr = optr + a;
    while (optr >= X)
    {
        optr = optr - X;
    }
    return optr;
}

//lq20200420新增函数
static uint8_t optrXMinus(uint8_t optr, uint8_t a, uint8_t X)
{
    while (optr < a)
    {
        optr = optr + X;
    }
    optr = optr - a;
    return optr;
}
//lq20200420新增函数
static uint8_t CenterPos(uint16_t buffertime)
{
    uint8_t centerpos = 0;
    for (int i = 0; i < centerlen; i++)
    {
        if (center[i].modeltime == buffertime)
        {
            centerpos = i;
            break;
        }
        else if(center[i].modeltime > buffertime)
        {
            centerlen = centerlen + 1;
            for (int j = centerlen - 1; j > i; j--)
            {
                center[j].modeltime = center[j - 1].modeltime;
                center[j].BrightState = center[j - 1].BrightState;
            } 
            center[i].modeltime = buffertime;
            center[i].BrightState = center[optrXMinus(i, 1, centerlen)].BrightState; 
            centerpos = i;
            break;
        }
        else if(i == centerlen - 1)
        {
            center[centerlen].modeltime = buffertime;
            center[centerlen].BrightState = center[optrXMinus(centerlen, 1, centerlen)].BrightState;
            centerlen = centerlen + 1;
            centerpos = centerlen - 1;
        }
    }
    return centerpos;  
}

//lq20200420新增函数
static void ModelTimeFilter(uint8_t pos, uint8_t filtvalue)
{
    //对center模型上新增的pos时间点，进行filtvalue min平滑，
    if ((centerlen < 2) || (centerlen + 2 > CENTERLENMAX))
    {
        return;
    }
    if (pos == centerlen-1)
    {        
        if (center[pos].modeltime <= center[pos-1].modeltime + filtvalue)
        {
            //center[centerlen-2].modeltime = center[centerlen-1].modeltime;
            //center[centerlen-2].BrightState = center[centerlen-1].BrightState;
            center[pos-1].modeltime = center[pos].modeltime;
            center[pos-1].BrightState = center[pos].BrightState;
            centerlen = centerlen - 1;
        }
        if (center[0].modeltime + 1440 <= center[pos].modeltime + filtvalue)
        {
            centerlen = centerlen - 1;
        }
    }
    else if(pos == 0)
    {
        if (center[0].modeltime + 1440 <= center[centerlen - 1].modeltime + filtvalue)
        {
            centerlen = centerlen - 1;
        }
        if (center[1].modeltime <= center[0].modeltime + filtvalue)
        {
            for (int j = 0; j < centerlen - 1; j++)
            {
                center[j].modeltime = center[j + 1].modeltime;
                center[j].BrightState = center[j + 1].BrightState;
            }
            centerlen = centerlen - 1;
        }        
    }
    else
    {        
        if (center[pos].modeltime <= center[pos-1].modeltime + filtvalue)
        {
            for (int j = pos-1; j < centerlen-1; j++)
            {
                center[j].modeltime = center[j+1].modeltime;
                center[j].BrightState = center[j+1].BrightState;
            }
            centerlen = centerlen - 1;
        }
        if (center[pos+1].modeltime <= center[pos].modeltime + filtvalue)
        {
            for (int j = pos; j < centerlen - 1; j++)
            {
                center[j].modeltime = center[j + 1].modeltime;
                center[j].BrightState = center[j + 1].BrightState;
            }
            centerlen = centerlen - 1;
        }
    }
    return;
}

//lq20200420新增函数
static void BrightStateFilter(uint8_t Brightfilter)
{
    //center模型整体亮度平滑, 若连续两点亮度>0，亮度偏差<+-15，
    //则前一点亮度=两段亮度积分平均,并删除后一点
    uint8_t inext = 0;
    uint8_t inext2 = 0;
    uint16_t dis1 = 0;
    uint16_t dis2 = 0;
    uint16_t dis = 0;
    uint16_t centertemp = 0;
    int i = 0;
    while (i < centerlen)
    {
        inext = optrXPlus(i, 1, centerlen);
        inext2 = optrXPlus(i, 2, centerlen);
        if (center[i].BrightState > 0)
        {
            while ((center[inext].BrightState > 0) && (centerlen > 2) &&
             (center[inext].BrightState <= (center[i].BrightState + Brightfilter)) &&
             ((center[inext].BrightState + Brightfilter) >= center[i].BrightState))
            {
                if (inext > i)
                {
                    dis1 = center[inext].modeltime - center[i].modeltime;
                    dis1 = dis1>>3;
                }
                else
                {
                    dis1 = 1440 + center[inext].modeltime - center[i].modeltime;
                    dis1 = dis1>>3;
                }

                if (inext2 > inext)
                {
                    dis2 = center[inext2].modeltime - center[inext].modeltime;
                    dis2 = dis2>>3;
                }
                else
                {
                    dis2 = 1440 + center[inext2].modeltime - center[inext].modeltime;
                    dis2 = dis2>>3;
                }
                if (dis1 <= 0) //lq20200618
                {  
                    dis1 = 1; 
                }
                if (dis2 <= 0) //lq20200618
                {  
                    dis2 = 1; 
                }
                centertemp = center[i].BrightState* dis1 + center[inext].BrightState * dis2;
                dis = dis1 + dis2;
                center[i].BrightState = centertemp / dis;

                //lq20200512
                for (int i = 0 ; i < centerlen; i++)
                {
                    LOG_DEBUG("centeri= %d centeri.modeltime= %d centeri.BrightState= %d \n", i, center[i].modeltime, center[i].BrightState);
                }
                LOG_DEBUG("i= %d centertemp= %d \n", i, centertemp);              

                if (inext < centerlen - 1)
                {
                    for (int jj = inext; jj < centerlen - 1 ; jj++)
                    {
                        center[jj].modeltime = center[jj + 1].modeltime;
                        center[jj].BrightState = center[jj + 1].BrightState;
                    }
                    centerlen = centerlen - 1;
                }
                else
                {
                    centerlen = centerlen - 1;
                }    
                //lq20200512
                for (int i = 0 ; i < centerlen; i++)
                {
                    LOG_DEBUG("centeri= %d centeri.modeltime= %d centeri.BrightState= %d \n", i, center[i].modeltime, center[i].BrightState);
                }

                if (i > inext)  //lq20200528
                {
                    i--;
                }
                inext = optrXPlus(i, 1, centerlen);  //lq20200512
                inext2 = optrXPlus(i, 2, centerlen); //lq20200512      
            }
        }
        else
        {
            while ((center[inext].BrightState == 0) && (centerlen > 2))
            {
                if (inext < centerlen - 1)
                {
                    for (int jj = inext; jj < centerlen - 1; jj++)
                    {
                        center[jj].modeltime = center[jj + 1].modeltime;
                        center[jj].BrightState = center[jj + 1].BrightState;
                    }                         
                    centerlen = centerlen - 1;
                }                     
                else
                {
                    centerlen = centerlen - 1;
                }   
                if (i > inext)  //lq20200528
                {
                    i--;
                }
                inext = optrXPlus(i, 1, centerlen);  //lq20200512
                inext2 = optrXPlus(i, 2, centerlen); //lq20200512                  
            }
        }
        i = i + 1;
    }
    return;    
}

void bubbleSort(void){
    for(uint8_t i = 0; i < AutoBrightnessSet.Item.AutoBrightnessNum - 1; i++) {
        for(uint8_t j = 0; j < AutoBrightnessSet.Item.AutoBrightnessNum - 1 - i; j++) {
            if(AutoBrightnessSet.Item.AutoBrightnessPoint[j][0] > AutoBrightnessSet.Item.AutoBrightnessPoint[j+1][0]) {        // 相邻元素两两对比
                uint16_t temp = AutoBrightnessSet.Item.AutoBrightnessPoint[j+1][0];        // 元素交换
                AutoBrightnessSet.Item.AutoBrightnessPoint[j+1][0] = AutoBrightnessSet.Item.AutoBrightnessPoint[j][0];
                AutoBrightnessSet.Item.AutoBrightnessPoint[j][0] = temp;
                temp = AutoBrightnessSet.Item.AutoBrightnessPoint[j+1][1];        // 元素交换
                AutoBrightnessSet.Item.AutoBrightnessPoint[j+1][1] = AutoBrightnessSet.Item.AutoBrightnessPoint[j][1];
                AutoBrightnessSet.Item.AutoBrightnessPoint[j][1] = temp;
            }
        }
    }
}

//lq20200420新增函数
static void NewmodelPreprocess(void)
{
    uint32_t modeltimetmp = 0;
    uint16_t modeltime = 0;
    AutoBrightnessSet.Item.FlagModelOn = 1;
    //if (1==Flagnewmodel)
    if (AutoBrightnessSet.Item.AutoBrightnessNum > 0)
    {
        modeltimetmp = (SystemUTCtimer0 + 30) / 60;
        modeltime = APlusBModX(modeltimetmp, 0, 1440);
        //AxisDis = AutoBrightnessSet.Item.Tbulb2 * 10 - modeltime;
        AxisDis = AutoBrightnessSet.Item.Tbulb2 - modeltime; //lq20200618 精度1min时
        Flagnewmodel = 0;
        flagdayone = 0;
        AutoBrightnessSet.Item.flagdayone = flagdayone;
        for (int i = 0; i < BUFLOGMAXSIZE; i++)
        {
            bufferlog[i].modeltime = 0;
            bufferlog[i].BrightState = 255;
        }
        //bufferlog_optr = 0;
        centerlen = AutoBrightnessSet.Item.AutoBrightnessNum;
        for (int i = 0; i < centerlen; i++)
        {
           //center[i].modeltime = AutoBrightnessSet.Item.AutoBrightnessPoint[i][0] * 10;
           center[i].modeltime = AutoBrightnessSet.Item.AutoBrightnessPoint[i][0]; //lq20200618 精度1min时
           center[i].BrightState = AutoBrightnessSet.Item.AutoBrightnessPoint[i][1];
        }
    }
    LOG_DEBUG("AxisDis= %d \n", AxisDis);
    return;    
}

//lq20200420新增函数
static void Compress(void)
{
    uint16_t intermin;
    uint8_t posmin;
    while (centerlen > CENTERLENMAX - 2)
    {
        intermin = center[1].modeltime - center[0].modeltime;
        posmin = 0;
        for (int i = 1; i < centerlen - 1; i++)
        {
            if (center[i + 1].modeltime - center[i].modeltime < intermin)
            {
                intermin = center[i + 1].modeltime - center[i].modeltime;
                posmin = i;
            }
        }
        for (int j = posmin; j < centerlen - 1; j++)
        {
            center[j].modeltime = center[j + 1].modeltime;
            center[j].BrightState = center[j + 1].BrightState;
        }
        centerlen = centerlen - 1;
    }
    if (centerlen > 0)
    {
        for (int i = 0; i < centerlen; i++)
        {
            //AutoBrightnessSet.Item.AutoBrightnessPoint[i][0]=(center[i].modeltime + 5)/ 10;
            //AutoBrightnessSet.Item.AutoBrightnessPoint[i][1]=center[i].BrightState;
            AutoBrightnessSet.Item.AutoBrightnessPoint[i][0]=center[i].modeltime; //lq20200618 精度1min时
            AutoBrightnessSet.Item.AutoBrightnessPoint[i][1]=(uint16_t)center[i].BrightState; //lq20200618 精度1min时
        }
    }
    AutoBrightnessSet.Item.AutoBrightnessNum = centerlen;
    return;   
}

//lq20200420新增函数
static void CalTbulb1(void)
{
    uint32_t modeltimetmp;
    uint16_t modeltimenewtmp;
    modeltimetmp = (SystemUTCtimer0 + 30) / 60;
    modeltimenewtmp = APlusBModX(modeltimetmp, 0, 1440);
    if (AxisDis >= 0)
    {
        modeltimenewtmp = APlusBModX((uint32_t)modeltimenewtmp, (uint32_t)AxisDis, 1440); 
    }
    else
    {
        modeltimenewtmp = AMinusBModX((uint32_t)modeltimenewtmp, (uint32_t)( 0 - AxisDis), 1440); 
    }   
    //AutoBrightnessSet.Item.Tbulb1 = (uint8_t)((modeltimenewtmp + 5) / 10);
    AutoBrightnessSet.Item.Tbulb1 = modeltimenewtmp;//lq20200618 精度1min时
    LOG_DEBUG("SystemUTCtimer0= %d modeltimenewtmp= %d AxisDis= %d Tbulb1= %d \n", SystemUTCtimer0,modeltimenewtmp,AxisDis,AutoBrightnessSet.Item.Tbulb1);
    
    return;
}

//lq20200616 新增
uint16_t Calmodeltimenew(uint32_t SystemUTCtimer0)
{
    uint32_t modeltimetmp = 0;
    modeltimetmp = (SystemUTCtimer0 + 30) / 60;
    modeltimenew = APlusBModX(modeltimetmp, 0, 1440);
    if (AxisDis >= 0)
    {
        modeltimenew = APlusBModX((uint32_t)modeltimenew, (uint32_t)AxisDis, 1440); 
    }
    else
    {
        modeltimenew = AMinusBModX((uint32_t)modeltimenew, (uint32_t)( 0 - AxisDis), 1440); 
    }
    return modeltimenew;
}


//lq20200616 新增
void LearnBufInitial(void)
{
    if(AutoBrightnessSet.Item.AutoBrightnessNum ==0){
        for (int i = 0; i < CENTERLENMAX; i++)
        {
            center[i].modeltime = 0;
            center[i].BrightState = 255;
        }
        for (int i = 0; i < BUFLOGMAXSIZE; i++)
        {
            bufferlog[i].modeltime = 0;
            bufferlog[i].BrightState = 255;
        }
        modeltimenew = 0;               //当前点模型时间，=0-1439
        BrightStatenew = 255;              //当前点亮度，=0-100     
        centerlen = 0;   //“中心”实际长度  
        bufferlog_optr = 0; //=0\1\2 
        flagdayone = 1;  // 第一天数据的标志位, 初始化为第1天
        AutoBrightnessSet.Item.flagdayone = flagdayone;
        Flagnewmodel = 0; //下载模型时标志位，新下载时=1
    }else{
        for (int i = 0; i < BUFLOGMAXSIZE; i++)
        {
            bufferlog[i].modeltime = 0;
            bufferlog[i].BrightState = 255;
        }
        bufferlog_optr = 0;
        modeltimenew = 0;               //当前点模型时间，=0-1439
        BrightStatenew = 255;           //当前点亮度，=0-100    
        centerlen = AutoBrightnessSet.Item.AutoBrightnessNum;
        for (int i = 0; i < centerlen; i++)
        {
           //center[i].modeltime = AutoBrightnessSet.Item.AutoBrightnessPoint[i][0] * 10;
           center[i].modeltime = AutoBrightnessSet.Item.AutoBrightnessPoint[i][0]; //lq20200618 精度1min时
           center[i].BrightState = AutoBrightnessSet.Item.AutoBrightnessPoint[i][1];
        }
        Flagnewmodel = 0;
        flagdayone = AutoBrightnessSet.Item.flagdayone;
    }
    
    
    // BrightStatenew = 0;
    //lq20200622 定时器---------
    if (wiced_is_timer_in_use(&BrightCheckTimer))
    {
        wiced_stop_timer(&BrightCheckTimer);
        wiced_deinit_timer(&BrightCheckTimer);
    }
    wiced_init_timer(&BrightCheckTimer, BrightCheckChange, 0, WICED_SECONDS_PERIODIC_TIMER); 
    wiced_start_timer(&BrightCheckTimer, BRIGHT_TIMER_LENGTH); 

    // if (wiced_is_timer_in_use(&BrightGradualTimer))
    // {
    //     wiced_stop_timer(&BrightGradualTimer);
    //     wiced_deinit_timer(&BrightGradualTimer);
    // }
    // wiced_init_timer(&BrightGradualTimer, BrightGradualChange, 0, WICED_MILLI_SECONDS_PERIODIC_TIMER);
    

    return;
}

//lq20200617 新增
uint8_t GetCenter(uint8_t *reply,uint8_t *p_data, uint16_t data_len)
{
    uint8_t returnlen = 0;
    uint8_t dateoptr = 0;
    uint8_t dateiptr = 0;
    while(dateoptr < data_len){
        if(p_data[dateoptr] == 1)  //获取功能开关状态
        {
            reply[dateiptr++] = p_data[dateoptr];
            reply[dateiptr++] = AutoBrightnessSet.Item.Flag;
            // reply[1] = (reply[1]<<1)+AutoBrightnessSet.Item.FlagModelOn;
            dateoptr++;
            continue;
        }
        else if(p_data[dateoptr] == 2)  //功能开关设置
        {
            uint8_t replylen = 0;  //模型数据包长度
            reply[dateiptr++] = p_data[dateoptr];   
            data_len--;
            CalTbulb1();

            //lq20200420 精度10min时 ------------------------------
            //reply[1] = AutoBrightnessSet.Item.Tbulb1;  
            //reply[2] = AutoBrightnessSet.Item.FlagModelOn;  
            //memcpy(reply+3,&AutoBrightnessSet.Item.AutoBrightnessPoint,reply[0]*2); 
            //replylen = reply[0]*2+3;      
            
            //lq20200618 精度1min时 ---------------
            reply[dateiptr++] = AutoBrightnessSet.Item.Tbulb1>>8;  
            reply[dateiptr++] = AutoBrightnessSet.Item.Tbulb1 & 0xff;
            reply[dateiptr] = AutoBrightnessSet.Item.AutoBrightnessNum;    
            if(reply[dateiptr] > LIGHT_AUTOBRIGHTNESSMAXNUM)
            {
                reply[dateiptr] = LIGHT_AUTOBRIGHTNESSMAXNUM;
            }
            dateiptr++;
            // uint8_t istart = dateiptr;  
            // for (uint8_t i = 0; i < AutoBrightnessSet.Item.AutoBrightnessNum ; i++)
            // {
            //     reply[4 * i + istart] = AutoBrightnessSet.Item.AutoBrightnessPoint[i][0]>>8;        
            //     reply[4 * i + istart +1] = AutoBrightnessSet.Item.AutoBrightnessPoint[i][0] & 0xff;
            //     reply[4 * i + istart +2] = AutoBrightnessSet.Item.AutoBrightnessPoint[i][1]>>8;  
            //     reply[4 * i + istart +3] = AutoBrightnessSet.Item.AutoBrightnessPoint[i][1] & 0xff;        
            // }
            for (uint8_t i = 0; i < AutoBrightnessSet.Item.AutoBrightnessNum ; i++)
            {
                reply[dateiptr++] = AutoBrightnessSet.Item.AutoBrightnessPoint[i][0]>>8;        
                reply[dateiptr++] = AutoBrightnessSet.Item.AutoBrightnessPoint[i][0] & 0xff;
                reply[dateiptr++] = AutoBrightnessSet.Item.AutoBrightnessPoint[i][1]>>8;  
                reply[dateiptr++] = AutoBrightnessSet.Item.AutoBrightnessPoint[i][1] & 0xff;        
            }
            //-------------------------------------
            LOG_DEBUG("AutoBrightnessNum= %d Tbulb1= %d FlagModelOn= %d AutoBrightnessSet= %d \n", AutoBrightnessSet.Item.AutoBrightnessNum, AutoBrightnessSet.Item.Tbulb1,AutoBrightnessSet.Item.FlagModelOn,AutoBrightnessSet.Item.AutoBrightnessPoint);
            LOG_DEBUG("reply(0+4)= %d reply(1+4)= %d reply(2+4)= %d reply(3+4)= %d \n", reply[dateiptr-3],reply[dateiptr-2],reply[dateiptr-1],reply[dateiptr]); 
            dateoptr++;
            continue;
        }
    }
    return dateiptr;
}

//lq20200617 新增
uint8_t SetCenter(uint8_t *reply, uint8_t *p_data, uint16_t data_len)
{
    uint8_t replylen = 0;
    uint8_t dateoptr = 0;
    uint8_t dateiptr = 0;
    while(dateoptr < data_len){
        // if(data_len !=  (*(p_data+4)*2+5))
        //if(data_len !=  (*(p_data+4)*2+7))   //lq20200420
        //if(data_len !=  (*(p_data)*2+3))   //lq20200617
        if(p_data[dateoptr] == 1){    //功能开关设置
            if(data_len > 1){
                AutoBrightnessSet.Item.AutoBrightnessSetSave = 1;
                reply[dateiptr++] = p_data[dateoptr++];
                AutoBrightnessSet.Item.Flag = p_data[dateoptr++]; 
                reply[dateiptr++] = AutoBrightnessSet.Item.Flag;
                replylen += 2;
            }
        }else if(p_data[dateoptr] == 2){
            if(data_len-dateoptr < 4){
                continue;
            }
            if(p_data[dateoptr+3] == 0){    //设置模型参数时，模型参数个数为0
                AutoBrightnessSet.Item.AutoBrightnessNum =0;
                LearnBufInitial();
                AutoBrightnessSet.Item.Tbulb2  = 0;    
                for (uint8_t i = 0; i<LIGHT_AUTOBRIGHTNESSMAXNUM; i++)
                {
                    AutoBrightnessSet.Item.AutoBrightnessPoint[i][0] = 0;
                    AutoBrightnessSet.Item.AutoBrightnessPoint[i][1] = 0;
                }
                reply[dateiptr++] = p_data[dateoptr];
                reply[dateiptr++] = (uint8_t)(AutoBrightnessSet.Item.Tbulb1 >>8); 
                reply[dateiptr++] = (uint8_t)(AutoBrightnessSet.Item.Tbulb1 >>8); 
                reply[dateiptr++] = p_data[dateoptr+3]; 
                dateoptr += 4;
                replylen += 4;
                AutoBrightnessSet.Item.AutoBrightnessSetSave = 1;
            }else if(data_len-dateoptr <  (*(p_data+dateoptr+3)*4+4))   //lq20200618 精度1min时  数据长度异常 不做处理
            {
            }
            else
            {
                AutoBrightnessSet.Item.AutoBrightnessSetSave = 1;  
                // AutoBrightnessSet.Item.FlagModelOn = p_data[3]; 
                AutoBrightnessSet.Item.AutoBrightnessNum = p_data[dateoptr+3];  
                memset(AutoBrightnessSet.Item.AutoBrightnessPoint, 0,
                    LIGHT_AUTOBRIGHTNESSMAXNUM*(sizeof(AutoBrightnessSet.Item.AutoBrightnessPoint)/sizeof(AutoBrightnessSet.Item.AutoBrightnessPoint[0]))); 
                //lq20200420 精度10min时 ------------------------------ 
                //AutoBrightnessSet.Item.Tbulb2 = p_data[1];       
                //AutoBrightnessSet.Item.FlagModelOn = p_data[2];    
                //memcpy(&AutoBrightnessSet.Item.AutoBrightnessPoint,p_data+3,*(p_data)*2); 
                
                //lq20200927 功能开启或关闭情况 (FlagModelOn == 1、0)(APP发送AutoBrightnessNum=0)
                //不对灯本地模型做任何影响，返回本地模型------
                uint8_t istart = dateoptr+4;   
                //if ((AutoBrightnessSet.Item.FlagModelOn == 1) && (p_data[0] == 0))  
                if (p_data[dateoptr+3] > 0)  
                {
                    //AutoBrightnessSet.Item.AutoBrightnessSetSave = 1;  //lq20200927 移到前面先判断
                    AutoBrightnessSet.Item.Tbulb2  = ((uint16_t)p_data[dateoptr+1]) << 8;        
                    AutoBrightnessSet.Item.Tbulb2  = AutoBrightnessSet.Item.Tbulb2 + p_data[dateoptr+2];    
                    if(p_data[dateoptr+3] > LIGHT_AUTOBRIGHTNESSMAXNUM)   
                    {
                        p_data[dateoptr+3] = LIGHT_AUTOBRIGHTNESSMAXNUM;
                    } 
                    AutoBrightnessSet.Item.AutoBrightnessNum = p_data[dateoptr+3];
                    
                    //lq20200420 精度10min时 ------------------------------ 
                    //AutoBrightnessSet.Item.Tbulb2 = p_data[1];       
                    //AutoBrightnessSet.Item.FlagModelOn = p_data[2];    
                    //memcpy(&AutoBrightnessSet.Item.AutoBrightnessSet,p_data+3,*(p_data)*2);           
                    
                    //uint8_t istart = 4;   
                    for (uint8_t i = 0; i<AutoBrightnessSet.Item.AutoBrightnessNum && i<LIGHT_AUTOBRIGHTNESSMAXNUM; i++)
                    {
                        AutoBrightnessSet.Item.AutoBrightnessPoint[i][0] = ((uint16_t)p_data[4 * i + istart]) << 8;
                        AutoBrightnessSet.Item.AutoBrightnessPoint[i][0] = AutoBrightnessSet.Item.AutoBrightnessPoint[i][0] + p_data[4 * i + istart +1];
                        AutoBrightnessSet.Item.AutoBrightnessPoint[i][1] = ((uint16_t)p_data[4 * i + istart +2]) << 8;
                        AutoBrightnessSet.Item.AutoBrightnessPoint[i][1] = AutoBrightnessSet.Item.AutoBrightnessPoint[i][1] + p_data[4 * i + istart +3];
                    }
                    if (1 == AutoBrightnessSet.Item.FlagModelOn)
                    {
                        //排序
                        bubbleSort();
                        NewmodelPreprocess();
                    } 
                }       
                //---------------------------------------
                // else 
                // {
                //     reply[0] = AutoBrightnessSet.Item.AutoBrightnessNum;
                // }
                reply[dateiptr+3] = AutoBrightnessSet.Item.AutoBrightnessNum;
                
                if(reply[dateiptr+3] > LIGHT_AUTOBRIGHTNESSMAXNUM)
                {
                    reply[dateiptr+3] = LIGHT_AUTOBRIGHTNESSMAXNUM;
                }
                            
                

                CalTbulb1();

                //lq20200420 精度10min时 ------------------------------        
                //reply[1] = AutoBrightnessSet.Item.Tbulb2; 
                //reply[2] = AutoBrightnessSet.Item.FlagModelOn; 
                //memcpy(reply+3,&AutoBrightnessSet.Item.AutoBrightnessSet,reply[0]*2); 
                //replylen = reply[0]*2+3;
                
                //lq20200618 精度1min时 ------------------------------        
                //reply[1] = (uint8_t)(AutoBrightnessSet.Item.Tbulb2 >>8); 
                //reply[2] = (uint8_t)(AutoBrightnessSet.Item.Tbulb2 & 0xff);
                reply[dateiptr+1] = (uint8_t)(AutoBrightnessSet.Item.Tbulb1 >>8); 
                reply[dateiptr+2] = (uint8_t)(AutoBrightnessSet.Item.Tbulb1 & 0xff);
                reply[dateiptr+0] = p_data[dateoptr]; 
                istart = dateiptr+4;
                for (uint8_t i = 0; i<AutoBrightnessSet.Item.AutoBrightnessNum && i<LIGHT_AUTOBRIGHTNESSMAXNUM; i++)
                {
                    // reply[4 * i + istart] = (uint8_t)(AutoBrightnessSet.Item.AutoBrightnessPoint[i][0]>>8);        
                    // reply[4 * i + istart +1] = (uint8_t)(AutoBrightnessSet.Item.AutoBrightnessPoint[i][0] & 0xff);
                    // reply[4 * i + istart +2] = (uint8_t)(AutoBrightnessSet.Item.AutoBrightnessPoint[i][1]>>8);  
                    // reply[4 * i + istart +3] = (uint8_t)(AutoBrightnessSet.Item.AutoBrightnessPoint[i][1] & 0xff);     
                    reply[istart++] = (uint8_t)(AutoBrightnessSet.Item.AutoBrightnessPoint[i][0]>>8);        
                    reply[istart++] = (uint8_t)(AutoBrightnessSet.Item.AutoBrightnessPoint[i][0] & 0xff);
                    reply[istart++] = (uint8_t)(AutoBrightnessSet.Item.AutoBrightnessPoint[i][1]>>8);  
                    reply[istart++] = (uint8_t)(AutoBrightnessSet.Item.AutoBrightnessPoint[i][1] & 0xff);     
                }
                dateiptr = istart;
                replylen += AutoBrightnessSet.Item.AutoBrightnessNum*4+4;
                dateoptr += AutoBrightnessSet.Item.AutoBrightnessNum*4+4;
                //-------------------------

                LOG_DEBUG("AutoBrightnessNum= %d Tbulb2= %d FlagModelOn= %d AutoBrightnessSet= %d \n", AutoBrightnessSet.Item.AutoBrightnessNum, AutoBrightnessSet.Item.Tbulb2,AutoBrightnessSet.Item.FlagModelOn,AutoBrightnessSet.Item.AutoBrightnessPoint);
                LOG_DEBUG("reply(0+4)= %d reply(1+4)= %d reply(2+4)= %d reply(3+4)= %d reply(4+4)= %d \n", reply[0], reply[1],reply[2],reply[3],reply[4]);

            }
        }
    }
    return replylen;
}

//lq20200622 定时器 新增-----
void BrightCheckChange(uint32_t para)
{    
    #include "adv_pack.h"
    
    BrightDuration++;
    LOG_DEBUG("BrightDuration= %d BrightStatenew= %d \n", BrightDuration, BrightStatenew);
     
    if( (AutoBrightnessSet.Item.AutoBrightnessNum < 1) || (AutoBrightnessSet.Item.FlagModelOn == 0) 
        ||(AutoBrightnessSet.Item.FlagAutoAdjustOn == 0) )  
    {
        return;
    }
    if(advpackispairing() == WICED_TRUE){
        return;
    }
    if((BrightStatenew < 1) || (BrightDuration < BRIGHTDURATIONMIN))
    // if(BrightStatenew < 1)
    {
        return;
    }
    if(AutoBrightnessSet.Item.AutoBrightnessNum > LIGHT_AUTOBRIGHTNESSMAXNUM)
    {
        AutoBrightnessSet.Item.AutoBrightnessNum = LIGHT_AUTOBRIGHTNESSMAXNUM;
    }

    uint32_t modeltimetmp = 0; 
    uint16_t operationtime; 
    modeltimetmp = (SystemUTCtimer0 + 30) / 60;        
    operationtime = APlusBModX(modeltimetmp, 0, 1440);   
    if (AxisDis >= 0) 
    {
        operationtime = APlusBModX((uint32_t)operationtime, (uint32_t)AxisDis, 1440); 
    }
    else
    {
        operationtime = AMinusBModX((uint32_t)operationtime, (uint32_t)( 0 - AxisDis), 1440); 
    } 

    uint8_t bright_des = 0;
    for(uint8_t i=0; i < AutoBrightnessSet.Item.AutoBrightnessNum; i++)
    {        
        if((operationtime == AutoBrightnessSet.Item.AutoBrightnessPoint[i][0]) 
            && (AutoBrightnessSet.Item.AutoBrightnessPoint[i][1] > 0))
        {
            bright_des = AutoBrightnessSet.Item.AutoBrightnessPoint[i][1];            
            if(((BrightStatenew >= bright_des + BRIGHTCHANGEMIN) || (bright_des >= BrightStatenew + BRIGHTCHANGEMIN)))
            {
                if(AMinusBModX(AutoBrightnessSet.Item.AutoBrightnessPoint[optrXPlus(i,1,AutoBrightnessSet.Item.AutoBrightnessNum)][0], AutoBrightnessSet.Item.AutoBrightnessPoint[i][0], 1440) > BRIGHTWILLLENMIN)
                {
                    extern uint8_t period_to_transt(uint32_t t);
                    uint8_t transition;
                    transition = period_to_transt(PERIODVALUE*BRIGHTGRADUAL_LENGTH);
                    LOG_DEBUG("BrightStatenew= %d bright_des= %d transition= 0x%02x\n", BrightStatenew,bright_des, transition);    
                    LightModelSetBrightness(bright_des, period_to_transt(PERIODVALUE*BRIGHTGRADUAL_LENGTH), 0);                
                    // //currentCfg.lightnessLevel = percentage_to_uint16(bright_des);  
                    // ctx0.anim = brightness_procedure_0;
                    // ctx0.tick = 1;
                    // //ctx0.period = transt_to_period(transitiontime)/LIGHT_TIMER_UINT;
                    // ctx0.period = PERIODVALUE;
                    // if(ctx0.period == 0)
                    // {
                    //     ctx0.period = 1;
                    // }
                    // ctx0.initiate = percentage_to_uint16(BrightStatenew);
                    // ctx0.final = percentage_to_uint16(bright_des);
                    // if (wiced_is_timer_in_use(&BrightGradualTimer))
                    // {
                    //     wiced_stop_timer(&BrightGradualTimer);
                    // }
                    // wiced_start_timer(&BrightGradualTimer, BRIGHTGRADUAL_LENGTH);

                    // BrightModelLearning(SystemUTCtimer0, bright_des);
                    // LightConfig.lightnessLevel = ctx0.final;
                    BrightStatenew = bright_des;
                    BrightDuration = 0;
                    break;
                }                
            }          
        }
    }

    // if((BrightStatenew > 0) && (BrightDuration >= BRIGHTDURATIONMIN))
    // {
    //     //AutoAdjustBrightness();
    //     bright_des = uint16_to_percentage(LightConfig.lightnessLevel);
    //     //if((lightnessLevel0Flag == 0) && ((BrightStatenew >= bright_des + BRIGHTCHANGEMIN) || (bright_des >= BrightStatenew + BRIGHTCHANGEMIN)))
    //     if(((BrightStatenew >= bright_des + BRIGHTCHANGEMIN) || (bright_des >= BrightStatenew + BRIGHTCHANGEMIN)))
    //     {
    //         currentCfg.lightnessLevel = LightConfig.lightnessLevel;
    //         currentCfg.lightingOn = 1;            
    //         LightUpdate();
    //         //wiced_stop_timer(&BrightCheckTimer);
    //         //wiced_deinit_timer(&BrightCheckTimer);
    //         //wiced_init_timer(&BrightCheckTimer, BrightCheckChange, 0, WICED_SECONDS_PERIODIC_TIMER) 
    //         BrightStatenew = bright_des;
    //         BrightDuration = 0;
    //     }
    // }    
}

// //lq20200622 定时器 新增----------
// void BrightGradualChange(uint32_t para)
// {
//     if(ctx0.anim)
//     {
//         if (ctx0.tick < ctx0.period)
//         {
//             ctx0.anim(ctx0.tick, ctx0.period, ctx0.initiate, ctx0.final);            
//         }
//         else
//         {
//             ctx0.anim(ctx0.tick, ctx0.period, ctx0.initiate, ctx0.final);
//             memset(&ctx0, 0, sizeof(ctx0));           
//         }
//         LOG_DEBUG("tick= %d period= %d initiate= %d final= %d \n", ctx0.tick, ctx0.period, ctx0.initiate, ctx0.final);
//         ctx0.tick ++;
//     }
//     else
//     {
//         wiced_stop_timer(&BrightGradualTimer);
//     }
// }

// //lq20200622 定时器 新增---
// int32_t brightness_procedure_0(int32_t tick, int32_t period, int32_t initiate, int32_t final)
// {
//     if(initiate < percentage_to_uint16(1))
//     {
//         initiate = percentage_to_uint16(1);
//     }
//     currentCfg.lightnessLevel = liner_transfer(tick, period, initiate, final);
//     if (currentCfg.lightnessLevel < 2)
//         currentCfg.lightingOn = 0;
//     else
//         currentCfg.lightingOn = 1;
//     LightUpdate();
//     return 0;
// }



// int32_t liner_transfer0(int32_t tick, int32_t period, int32_t initiate, int32_t final)
// {
//     if (tick < period)
//     {
//         return (((final - initiate) * tick * 2 + 1) / period + initiate * 2) / 2;
//     }
//     else
//     {
//         return final;
//     }
// }


