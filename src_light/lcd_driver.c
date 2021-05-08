
#include "log.h"
#include "wiced.h"
#include "wiced_platform.h"


/******************************************************
 *                      Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/

/******************************************************
 *                    Structures
 ******************************************************/

/******************************************************
 *               Static Function Declarations
 ******************************************************/

/******************************************************
 *               Variable Definitions
 ******************************************************/

#define GPIO_SCL    WICED_P02
#define GPIO_SDA    WICED_P03

#define	IIC_NoSendAck 0
/******************************************************
 *               Function Definitions
 ******************************************************/

extern void utilslib_delayUs(uint32_t delay);


void SDA_OUT(void)
{ 
    wiced_hal_gpio_select_function(GPIO_SDA,WICED_GPIO);
    wiced_hal_gpio_configure_pin(GPIO_SDA,( GPIO_OUTPUT_ENABLE | GPIO_PULL_DOWN ),GPIO_PIN_OUTPUT_HIGH);
}

void SDA_IN(void)
{
    wiced_hal_gpio_select_function(GPIO_SDA,WICED_GPIO);
    wiced_hal_gpio_configure_pin(GPIO_SDA,( GPIO_INPUT_ENABLE | GPIO_PULL_UP ),GPIO_PIN_OUTPUT_LOW);
}

void SCL_OUT(void)
{
    wiced_hal_gpio_select_function(GPIO_SCL,WICED_GPIO);
    wiced_hal_gpio_configure_pin(GPIO_SCL,( GPIO_OUTPUT_ENABLE | GPIO_PULL_DOWN ),GPIO_PIN_OUTPUT_HIGH);
}

void SDA_SET_HIGH(void)
{
   
    wiced_hal_gpio_set_pin_output( GPIO_SDA,1 );
}

void SDA_SET_LOW(void)
{

    wiced_hal_gpio_set_pin_output( GPIO_SDA ,0);
}

void SCL_SET_HIGH(void)
{

    wiced_hal_gpio_set_pin_output( GPIO_SCL,1 );
}

void SCL_SET_LOW(void)
{

    wiced_hal_gpio_set_pin_output( GPIO_SCL ,0);
}

////////////////////////////////////////////////////////////////////////

void i2c_init(void)     
{               
    LOG_DEBUG( "i2c_init\n") ;                         
    SCL_OUT();
    SDA_OUT();

    SCL_SET_HIGH();
    SDA_SET_HIGH();
}

void Api_IIC_Start(void)  
{
    SDA_SET_HIGH();
    utilslib_delayUs(10);
    SCL_SET_HIGH();
    utilslib_delayUs(10);   
    SDA_SET_LOW();
    utilslib_delayUs(10);
    SCL_SET_LOW();
    utilslib_delayUs(10);
}

 void Api_IIC_Stop(void)        
{
    SCL_SET_LOW();				
    utilslib_delayUs(10);			
    SDA_SET_LOW();				
    utilslib_delayUs(10);    	
    SCL_SET_HIGH();			
    utilslib_delayUs(10);			
    SDA_SET_HIGH();
    utilslib_delayUs(10);				
}

void Api_IIC_SetAck(uint8_t i)	
{
    if(i!=0)
    {                  //ack
        utilslib_delayUs(10);	
        SDA_SET_LOW();
        utilslib_delayUs(10);	
        SCL_SET_HIGH();	
        utilslib_delayUs(10);	
        SCL_SET_LOW();	
        utilslib_delayUs(10);	
        SDA_SET_HIGH();
        utilslib_delayUs(10);	
    }
    else
    {                //noack
        utilslib_delayUs(10);
        SDA_SET_HIGH();
        utilslib_delayUs(10);
        SCL_SET_HIGH();	
        utilslib_delayUs(10);
        SCL_SET_LOW();
        utilslib_delayUs(10);
    }	

}

wiced_bool_t READ_SDA(void)
{
    return wiced_hal_gpio_get_pin_input_status(GPIO_SDA);
}

void Api_IIC_WaitAck(void) 
{
    uint8_t i;
    wiced_bool_t sda_val = WICED_TRUE;
    static uint8_t wait_cnt;

    SDA_SET_HIGH();
    SDA_IN();
    utilslib_delayUs(10);	
    SCL_SET_HIGH();

    wait_cnt = 0;	

    do
    {
        utilslib_delayUs(10);
        sda_val = READ_SDA();
        wait_cnt++;
    } while (WICED_TRUE == sda_val && wait_cnt < 5);

    if(5 == wait_cnt){
        LOG_DEBUG(  "Ack failed\n" ) ;
    }
    
    utilslib_delayUs(10);
    SCL_SET_LOW();
    SDA_OUT();

}

void Api_IIC_SendByte(uint8_t senddata) 
{
    uint8_t i,j;

    SCL_SET_LOW();
    for(i = 0; i < 8; i++)
    {
        j = senddata & 0x80;
        if(j == 0)
            SDA_SET_LOW();
        else
            SDA_SET_HIGH();

        utilslib_delayUs(10);
        SCL_SET_HIGH();
        utilslib_delayUs(10);
        SCL_SET_LOW();
        utilslib_delayUs(10);
        senddata<<=1;
    }

}

void Api_HT16C23_CMD(uint8_t Add, uint8_t CMD, uint8_t bData)   
{   
    Api_IIC_SetAck(IIC_NoSendAck);
    Api_IIC_Start();

    Api_IIC_SendByte(Add);                       //写I2C从器件地址和写方式
    Api_IIC_WaitAck();

    Api_IIC_SendByte(CMD);                      //写命令
    Api_IIC_WaitAck() ;
   
    Api_IIC_SendByte(bData);                     //写数据
    Api_IIC_WaitAck() ;

    Api_IIC_Stop();                                  //停止

}

uint8_t Api_HT16C23_Write(uint8_t Add, uint8_t CMD, uint8_t SlaveAdd, uint8_t bData)   
{ 
    Api_IIC_SetAck(IIC_NoSendAck);
    Api_IIC_Start();
    Api_IIC_SendByte(Add);                       //写I2C从器件地址和写方式
    Api_IIC_WaitAck();

    Api_IIC_SendByte(CMD);                  //写命令
    Api_IIC_WaitAck();
    
    Api_IIC_SendByte(SlaveAdd);                  //写8位地址
    Api_IIC_WaitAck();

    Api_IIC_SendByte(bData);                     //写数据
    Api_IIC_WaitAck();

    Api_IIC_Stop();                                  //停止
}



//HT16C21初始化
void HT16C21_Init(void)
{
    Api_HT16C23_CMD(0x70, 0x82, 0x00);  // 1/3偏置 1/4占空比
    Api_HT16C23_CMD(0x70, 0x86, 0x01);  // 帧频率80Hz
    Api_HT16C23_CMD(0x70, 0x8A, 0x00);  // SEG/VLCD
    Api_HT16C23_CMD(0x70, 0x88, 0x00);  // 闪烁除能
    Api_HT16C23_CMD(0x70, 0x84, 0x03);  //内部系统振荡器开启 LCDON 
}

void HT16C21_DISPLAY_DOUBLE_ZERO(void)
{
    Api_HT16C23_Write(0x70, 0x80, 0, 0x01);
}

void HT16C21_DISPLAY_SINGLE_ONE(void)
{
    Api_HT16C23_Write(0x70, 0x80, 6, 0x10);
}

void HT16C21_DISPLAY_CLEAR(void)
{
    Api_HT16C23_Write(0x70, 0x80, 0, 0x00); 
    Api_HT16C23_Write(0x70, 0x80, 1, 0x00); 
    Api_HT16C23_Write(0x70, 0x80, 6, 0x00);    
}

void HT16C21_DISPLAY_DATA(uint8_t value, uint8_t double_zero)
{
    uint8_t val100, val10, val1;
    uint8_t addr0_val = 0;
    uint8_t addr1_val = 0;
    uint8_t addr6_val = 0;

    if(value > 199){
        return;
    }

    val100 = value / 100;
    val10 = (value - val100 * 100) / 10;
    val1 = value - val100 * 100 - val10 * 10;

    if(0x01 == double_zero){
        addr0_val |= 0x01;
    }

    if(val100 != 0){
       addr6_val |= 0x10;
    }

    switch(val10)
    {
        case 0:
            addr6_val |= 0x65;
            addr1_val |= 0x60;
            break;
        case 1:
            addr6_val |= 0x00;
            addr1_val |= 0x60;
            break; 
        case 2:
            addr6_val |= 0x27;
            addr1_val |= 0x40;     
            break;
        case 3:
            addr6_val |= 0x07;
            addr1_val |= 0x60;  
            break;
        case 4:
            addr6_val |= 0x42;
            addr1_val |= 0x60;  
            break; 
        case 5:
            addr6_val |= 0x47;
            addr1_val |= 0x20;  
            break;                                            
        case 6:
            addr6_val |= 0x67;
            addr1_val |= 0x20;  
            break;  
        case 7:
            addr6_val |= 0x04;
            addr1_val |= 0x60;  
            break;  
        case 8:
            addr6_val |= 0x67;
            addr1_val |= 0x60;  
            break;  
        case 9:
            addr6_val |= 0x47;
            addr1_val |= 0x60;  
            break;  
        default:
            break;       
    }

    switch(val1)
    {
        case 0:
            addr0_val |= 0x56;
            addr1_val |= 0x06;  
            break;    
        case 1:
            addr0_val |= 0x06;
            addr1_val |= 0x00;  
            break;  
        case 2:
            addr0_val |= 0x74;
            addr1_val |= 0x02;  
            break; 
        case 3:
            addr0_val |= 0x76;
            addr1_val |= 0x00;  
            break;  
        case 4:
            addr0_val |= 0x26;
            addr1_val |= 0x04;  
            break; 
        case 5:
            addr0_val |= 0x72;
            addr1_val |= 0x04;  
            break;    
        case 6:
            addr0_val |= 0x72;
            addr1_val |= 0x06;  
            break;  
        case 7:
            addr0_val |= 0x46;
            addr1_val |= 0x00;  
            break;   
        case 8:
            addr0_val |= 0x76;
            addr1_val |= 0x06;  
            break;    
        case 9:
            addr0_val |= 0x76;
            addr1_val |= 0x04;  
            break;       
        default:
            break;                         
    }

    Api_HT16C23_Write(0x70, 0x80, 0, addr0_val); 
    Api_HT16C23_Write(0x70, 0x80, 1, addr1_val); 
    Api_HT16C23_Write(0x70, 0x80, 6, addr6_val);


/*
    Api_HT16C23_Write(0x70, 0x80, 0, 0x06); // 个位 1
    Api_HT16C23_Write(0x70, 0x80, 1, 0x00); 


    Api_HT16C23_Write(0x70, 0x80, 0, 0x74); // 个位 2
    Api_HT16C23_Write(0x70, 0x80, 1, 0x02); 


    Api_HT16C23_Write(0x70, 0x80, 0, 0x76); // 个位 3
    Api_HT16C23_Write(0x70, 0x80, 1, 0x00); 

   
    Api_HT16C23_Write(0x70, 0x80, 0, 0x26); // 个位 4
    Api_HT16C23_Write(0x70, 0x80, 1, 0x04); 


    Api_HT16C23_Write(0x70, 0x80, 0, 0x72); // 个位 5
    Api_HT16C23_Write(0x70, 0x80, 1, 0x04); 


    Api_HT16C23_Write(0x70, 0x80, 0, 0x72); // 个位 6
    Api_HT16C23_Write(0x70, 0x80, 1, 0x06);   


    Api_HT16C23_Write(0x70, 0x80, 0, 0x46); // 个位 7
    Api_HT16C23_Write(0x70, 0x80, 1, 0x00); 


    Api_HT16C23_Write(0x70, 0x80, 0, 0x76); // 个位 8
    Api_HT16C23_Write(0x70, 0x80, 1, 0x06); 


    Api_HT16C23_Write(0x70, 0x80, 0, 0x76); // 个位 9
    Api_HT16C23_Write(0x70, 0x80, 1, 0x04); 


    Api_HT16C23_Write(0x70, 0x80, 0, 0x56); // 个位 0
    Api_HT16C23_Write(0x70, 0x80, 1, 0x06);      




    Api_HT16C23_Write(0x70, 0x80, 6, 0x00);
    Api_HT16C23_Write(0x70, 0x80, 1, 0x60); // 十位 1


    Api_HT16C23_Write(0x70, 0x80, 6, 0x27); // 十位 2
    Api_HT16C23_Write(0x70, 0x80, 1, 0x40);


    Api_HT16C23_Write(0x70, 0x80, 6, 0x07); // 十位 3
    Api_HT16C23_Write(0x70, 0x80, 1, 0x60); 


    Api_HT16C23_Write(0x70, 0x80, 6, 0x42); // 十位 4
    Api_HT16C23_Write(0x70, 0x80, 1, 0x60); 


    Api_HT16C23_Write(0x70, 0x80, 6, 0x47); // 十位 5
    Api_HT16C23_Write(0x70, 0x80, 1, 0x20); 


    Api_HT16C23_Write(0x70, 0x80, 6, 0x67); // 十位 6
    Api_HT16C23_Write(0x70, 0x80, 1, 0x20); 


    Api_HT16C23_Write(0x70, 0x80, 6, 0x04); // 十位 7
    Api_HT16C23_Write(0x70, 0x80, 1, 0x60); 


    Api_HT16C23_Write(0x70, 0x80, 6, 0x67); // 十位 8
    Api_HT16C23_Write(0x70, 0x80, 1, 0x60); 


    Api_HT16C23_Write(0x70, 0x80, 6, 0x47); // 十位 9
    Api_HT16C23_Write(0x70, 0x80, 1, 0x60);


    Api_HT16C23_Write(0x70, 0x80, 6, 0x65); // 十位 0
    Api_HT16C23_Write(0x70, 0x80, 1, 0x60);       


*/


}




