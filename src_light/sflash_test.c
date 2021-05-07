//******************************************************************************
//*
//* 文 件 名 : sflash_test.c
//* 文件描述 : 测试SFLASH的多次重复写操作，用于检验sflash的验证，量产版中不应包含此文件  
//*           测试过程如下：
//*           1. 所有待测扇区后查空
//*           2. 写入相同的数据到所有待测扇区后，读取数据，检查数据是否与写入一致    
//*           3. 重复1,2直到所有选定写入的待测试的数据都测试一遍 
//*           4. 擦除起始扇区并写0-255，读取后校验，直到设定的测试次数
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
#include "sparcommon.h"
#include "wiced_bt_cfg.h"
#include "wiced_bt_dev.h"
#include "wiced_bt_trace.h"
#include "wiced_gki.h"
#include "wiced_platform.h"
#include "wiced_hal_puart.h"
#include "wiced_timer.h"
#include "wiced_bt_stack.h"
#include "raw_flash.h"
#include "log.h"
#include "wiced_hal_nvram.h"
#include "wiced_rtos.h"
#include "wiced_hal_wdog.h"
#include "wiced_hal_rand.h"

///*****************************************************************************
///*                         宏定义区
///*****************************************************************************
#define  SFLASH_TEST_FLASHSIZE          0x100000
#define  SFLASH_TEST_BASE_ADDR          0x4000         //测试存储区起始地址，必须为扇区起始地址
//测试存储区长度，必须是整扇区
#define  SFLASH_TEST_BASE_LEN           (SFLASH_TEST_FLASHSIZE-SFLASH_TEST_BASE_ADDR)
// #define  SFLASH_TEST_MAX_COUNT          10000           //测试循环次数 当前没有用
#define  SFLASH_TEST_DEFAULT_DATA       0xFF            //擦除后的存储区中默认数值

#define  SFLASH_WRITEBYTE_PER_ONCE      512

#define  SFLASH_PHASE_ERASE_ALL         1               //擦除
#define  SFLASH_PHASE_CHECKNULL         2               //查空
#define  SFLASH_PHASE_WRITE             3               //写入数据
#define  SFLASH_PHASE_READ              4               //读取数据数据

#define  SFLASH_RESULT_OK               0               //擦除
#define  SFLASH_RESULT_ERASEFAILED      1               //擦除失败
#define  SFLASH_RESULT_CHEAKFAILED      2               //查空未通过
#define  SFLASH_RESULT_WRITEFAILED      4               //写入失败
#define  SFLASH_RESULT_READFAILED       5               //读取失败
#define  SFLASH_RESULT_CMPFAILED        6               //比对未通过

///*****************************************************************************
///*                         Strcut结构变量定义区
///*****************************************************************************

///*****************************************************************************
///*                         函数声明区
///*****************************************************************************

///*****************************************************************************
///*                         常量定义区
///*****************************************************************************
//测试时从此数组中取数，每个测试步骤仅取一个写满所有待测区域，直到所有数据测试完后，执行后续步骤
const uint8_t testmasklist[] = {0x55,0xAA,0xff,0x00};  
///*****************************************************************************
///*                         外部全局变量定义区
///*****************************************************************************

///*****************************************************************************
///*                         文件内部全局变量定义区
///*****************************************************************************
static uint8_t  testbuf[SFLASH_WRITEBYTE_PER_ONCE] = {0};
static uint8_t  testphase  = SFLASH_PHASE_ERASE_ALL;
static uint32_t testcycle  = 0;
static uint32_t testaddr   = 0;
static uint32_t testaddr1   = 0;
///*****************************************************************************
///*                         函数实现区
///*****************************************************************************

//*****************************************************************************
// 函数名称: 
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
void sFlashTest(void)
{
    testaddr = SFLASH_TEST_BASE_ADDR;
    testcycle = 0;
    LOG_DEBUG("test start!!!!!\n");
    while(testcycle < sizeof(testmasklist)){
        wiced_hal_wdog_restart();
        switch (testphase)
        {
        case SFLASH_PHASE_ERASE_ALL:
            if(testaddr%(4*FLASH_SECTOR_SIZE) == 0){
                LOG_DEBUG("test cycle :%d test erase addr :0x%08x \n",testcycle,testaddr);
            }
            wiced_hal_sflash_erase(testaddr&0xFFFFF000, FLASH_SECTOR_SIZE);
            testaddr += FLASH_SECTOR_SIZE;
            if(!(testaddr < (SFLASH_TEST_BASE_ADDR+SFLASH_TEST_BASE_LEN))){
                testphase = SFLASH_PHASE_CHECKNULL;
                testaddr = SFLASH_TEST_BASE_ADDR;
            }
            break;
        case SFLASH_PHASE_CHECKNULL:
            if(testaddr%(4*FLASH_SECTOR_SIZE) == 0){
                LOG_DEBUG("test cycle :%d test check addr :0x%08x \n",testcycle,testaddr);
            }
            if(SFLASH_WRITEBYTE_PER_ONCE != wiced_hal_sflash_read(testaddr, SFLASH_WRITEBYTE_PER_ONCE,testbuf)){
                LOG_DEBUG("test failed cycle:%d  type:%d \n",testcycle, SFLASH_RESULT_CHEAKFAILED); 
                wiced_hal_wdog_disable();
                while(1);
            }
            for(uint16_t i=0; i<SFLASH_WRITEBYTE_PER_ONCE; i++){
                if(SFLASH_TEST_DEFAULT_DATA != testbuf[i]){
                    LOG_DEBUG("test failed cycle:%d  type:%d \n",testcycle, SFLASH_RESULT_CHEAKFAILED); 
                    wiced_hal_wdog_disable();
                    while(1);
                }
            }
            testaddr += SFLASH_WRITEBYTE_PER_ONCE;
            if(!(testaddr < (SFLASH_TEST_BASE_ADDR+SFLASH_TEST_BASE_LEN))){
                testphase = SFLASH_PHASE_WRITE;
                testaddr = SFLASH_TEST_BASE_ADDR;
            }
            break;
        case SFLASH_PHASE_WRITE:
            //设置写入缓存
            if(testaddr%(4*FLASH_SECTOR_SIZE) == 0){
                LOG_DEBUG("test cycle :%d test write addr :0x%08x \n",testcycle,testaddr);
            }
            memset(testbuf,testmasklist[testcycle],SFLASH_WRITEBYTE_PER_ONCE);
            if(SFLASH_WRITEBYTE_PER_ONCE != wiced_hal_sflash_write(testaddr, SFLASH_WRITEBYTE_PER_ONCE,testbuf)){
                LOG_DEBUG("test failed cycle:%d  type:%d \n",testcycle, SFLASH_RESULT_WRITEFAILED); 
                wiced_hal_wdog_disable();
                while(1);
            }
            testaddr += SFLASH_WRITEBYTE_PER_ONCE;
            if(!(testaddr < (SFLASH_TEST_BASE_ADDR+SFLASH_TEST_BASE_LEN))){
                testphase = SFLASH_PHASE_READ;
                testaddr = SFLASH_TEST_BASE_ADDR;
            }
            break;
        case SFLASH_PHASE_READ:
            if(testaddr%(4*FLASH_SECTOR_SIZE) == 0){
                LOG_DEBUG("test cycle :%d test read addr :0x%08x \n",testcycle,testaddr);
            }
            if(SFLASH_WRITEBYTE_PER_ONCE != wiced_hal_sflash_read(testaddr, SFLASH_WRITEBYTE_PER_ONCE,testbuf)){
                LOG_DEBUG("test failed cycle:%d  type:%d \n",testcycle, SFLASH_RESULT_READFAILED); 
                wiced_hal_wdog_disable();
                while(1);
            }
            //比对读取的数据
            for(uint16_t i=0; i<SFLASH_WRITEBYTE_PER_ONCE; i++){
                if(testmasklist[testcycle] != testbuf[i]){
                    LOG_DEBUG("test failed cycle:%d  type:%d \n",testcycle, SFLASH_RESULT_CHEAKFAILED); 
                    wiced_hal_wdog_disable();
                    while(1);
                }
            }
            testaddr += SFLASH_WRITEBYTE_PER_ONCE;
            if(!(testaddr < (SFLASH_TEST_BASE_ADDR+SFLASH_TEST_BASE_LEN))){
                LOG_DEBUG("test ok cycle : %d \n",testcycle);
                testphase = SFLASH_PHASE_ERASE_ALL;
                testaddr = SFLASH_TEST_BASE_ADDR;
                testcycle ++;
            }
            break;
        default:
            LOG_DEBUG("test failed!!! \n");
            wiced_hal_wdog_disable();
            while(1);
            break;
        }
    }

    //随机选一个扇区做寿命测试           
    testaddr = testaddr1 = SFLASH_TEST_BASE_ADDR + (wiced_hal_rand_gen_num()%(SFLASH_TEST_BASE_LEN/FLASH_SECTOR_SIZE))*FLASH_SECTOR_SIZE;
    LOG_DEBUG("test cycle : %d, testaddr %d\n",testcycle,testaddr1);
    while(1){
        wiced_hal_wdog_restart();
        switch (testphase)
        {
        case SFLASH_PHASE_ERASE_ALL:
            LOG_DEBUG("test cycle : %d, testphase %d\n",testcycle,testphase);
            wiced_hal_sflash_erase(testaddr&0xFFFFF000, FLASH_SECTOR_SIZE);
            testphase = SFLASH_PHASE_CHECKNULL;
            break;
        case SFLASH_PHASE_CHECKNULL:
            LOG_DEBUG("test cycle : %d, testphase %d\n",testcycle,testphase);
            if(SFLASH_WRITEBYTE_PER_ONCE != wiced_hal_sflash_read(testaddr, SFLASH_WRITEBYTE_PER_ONCE,testbuf)){
                LOG_DEBUG("test failed cycle:%d  type:%d \n",testcycle, SFLASH_RESULT_CHEAKFAILED); 
                wiced_hal_wdog_disable();
                while(1);
            }
            for(uint16_t i=0; i<SFLASH_WRITEBYTE_PER_ONCE; i++){
                if(SFLASH_TEST_DEFAULT_DATA != testbuf[i]){
                    LOG_DEBUG("test failed cycle:%d  type:%d \n",testcycle, SFLASH_RESULT_CHEAKFAILED); 
                    wiced_hal_wdog_disable();
                    while(1);
                }
            }
            testaddr += SFLASH_WRITEBYTE_PER_ONCE;
            if(!(testaddr < (SFLASH_TEST_BASE_ADDR+FLASH_SECTOR_SIZE))){
                testphase = SFLASH_PHASE_WRITE;
                testaddr = testaddr1;
            }
            break;
        case SFLASH_PHASE_WRITE:
            LOG_DEBUG("test cycle : %d, testphase %d\n",testcycle,testphase);
            for(uint16_t i=0; i<SFLASH_WRITEBYTE_PER_ONCE; i++){
                testbuf[i] = i&0xFF;
            }
            if(SFLASH_WRITEBYTE_PER_ONCE != wiced_hal_sflash_write(testaddr, SFLASH_WRITEBYTE_PER_ONCE,testbuf)){
                LOG_DEBUG("test failed cycle:%d  type:%d \n",testcycle, SFLASH_RESULT_WRITEFAILED); 
                wiced_hal_wdog_disable();
                while(1);
            }
            testaddr += SFLASH_WRITEBYTE_PER_ONCE;
            if(!(testaddr < (SFLASH_TEST_BASE_ADDR+FLASH_SECTOR_SIZE))){
                testphase = SFLASH_PHASE_READ;
                testaddr = testaddr1;
            }
            break;
        case SFLASH_PHASE_READ:
            LOG_DEBUG("test cycle : %d, testphase %d\n",testcycle,testphase);
            if(SFLASH_WRITEBYTE_PER_ONCE != wiced_hal_sflash_read(testaddr, SFLASH_WRITEBYTE_PER_ONCE,testbuf)){
                LOG_DEBUG("test failed cycle:%d  type:%d \n",testcycle, SFLASH_RESULT_READFAILED); 
                wiced_hal_wdog_disable();
                while(1);
            }
            //比对读取的数据
            for(uint16_t i=0; i<SFLASH_WRITEBYTE_PER_ONCE; i++){
                if((i&0xFF) != testbuf[i]){
                    LOG_DEBUG("test failed cycle:%d  type:%d \n",testcycle, SFLASH_RESULT_CHEAKFAILED); 
                    wiced_hal_wdog_disable();
                    while(1);
                }
            }
            testaddr += SFLASH_WRITEBYTE_PER_ONCE;
            if(!(testaddr < (SFLASH_TEST_BASE_ADDR+FLASH_SECTOR_SIZE))){
                LOG_DEBUG("test ok cycle : %d \n",testcycle);
                testphase = SFLASH_PHASE_ERASE_ALL;
                testaddr = testaddr1;
                testcycle ++;
            }
            break;
        default:
            LOG_DEBUG("test failed!!! \n");
            wiced_hal_wdog_disable();
            while(1);
            break;
        }
    }
}