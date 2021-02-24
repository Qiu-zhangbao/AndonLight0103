/*
 * Copyright 2018, Cypress Semiconductor Corporation or a subsidiary of Cypress Semiconductor 
 *  Corporation. All rights reserved. This software, including source code, documentation and  related 
 * materials ("Software"), is owned by Cypress Semiconductor  Corporation or one of its 
 *  subsidiaries ("Cypress") and is protected by and subject to worldwide patent protection  
 * (United States and foreign), United States copyright laws and international treaty provisions. 
 * Therefore, you may use this Software only as provided in the license agreement accompanying the 
 * software package from which you obtained this Software ("EULA"). If no EULA applies, Cypress 
 * hereby grants you a personal, nonexclusive, non-transferable license to  copy, modify, and 
 * compile the Software source code solely for use in connection with Cypress's  integrated circuit 
 * products. Any reproduction, modification, translation, compilation,  or representation of this 
 * Software except as specified above is prohibited without the express written permission of 
 * Cypress. Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO  WARRANTY OF ANY KIND, EXPRESS 
 * OR IMPLIED, INCLUDING,  BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY 
 * AND FITNESS FOR A PARTICULAR PURPOSE. Cypress reserves the right to make changes to 
 * the Software without notice. Cypress does not assume any liability arising out of the application 
 * or use of the Software or any product or circuit  described in the Software. Cypress does 
 * not authorize its products for use in any products where a malfunction or failure of the 
 * Cypress product may reasonably be expected to result  in significant property damage, injury 
 * or death ("High Risk Product"). By including Cypress's product in a High Risk Product, the 
 *  manufacturer of such system or application assumes  all risk of such use and in doing so agrees 
 * to indemnify Cypress against all liability.
 */

/******************************************************************************
 *                                Includes
 ******************************************************************************/

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

#define   USE_NVRAM (0)

#define   CSUM_XOR_VAL                              0x55
#define   BLOCKNOTUSE                               0xFF   //未使用
#define   BLOCKVISIBLE                              0x55   //当前有效
#define   BLOCKINVISIBLE                            0x00   //已失效

typedef struct
{
    uint16_t   len;
    uint8_t    payload_cksum;
    uint8_t    hdr_cksum;
} flash_item_hdr_t;  //需保证占用空间的最后一个字节为头校验内容，特别是在有字节对齐的时候

uint8_t flash_data_checksum(uint8_t *p_data, uint16_t len)
{
    int i;
    uint8_t cksum = 0;
    for (i = 0; i < len; i++)
    {
        cksum += p_data[i];
    }
    return (cksum ^ CSUM_XOR_VAL);
}

uint16_t ptrReadDataFromSFlash(uint32_t read_from, uint8_t *buf, uint32_t len)
{
    flash_item_hdr_t rhdr;
    if (wiced_hal_sflash_read(read_from, sizeof(flash_item_hdr_t), (uint8_t *)(&rhdr)) == sizeof(flash_item_hdr_t))
    {
        if(rhdr.hdr_cksum == flash_data_checksum((uint8_t *)(&rhdr),sizeof(flash_item_hdr_t)-1))
        {
            if (wiced_hal_sflash_read(read_from+sizeof(flash_item_hdr_t), rhdr.len, buf) == rhdr.len)
            {
                if(rhdr.payload_cksum == flash_data_checksum(buf,rhdr.len)) 
                {
                    return rhdr.len;
                }
            }
        }
    }
    return 0;
}

uint16_t ptrWriteDataToSFlash(uint32_t write_to, uint8_t *buf, uint32_t len)
{
    flash_item_hdr_t rhdr;
    rhdr.len = len;
    
    rhdr.payload_cksum = flash_data_checksum(buf,len);
    rhdr.hdr_cksum = flash_data_checksum((uint8_t *)(&rhdr),sizeof(flash_item_hdr_t)-1);
    if (wiced_hal_sflash_write(write_to+sizeof(flash_item_hdr_t), len, buf) == len)
    {
        if (wiced_hal_sflash_write(write_to, sizeof(flash_item_hdr_t), (uint8_t *)(&rhdr)) == sizeof(flash_item_hdr_t))
        {
            return len;
        }
    }
    return 0;
}

//#define APPLICATION_SPECIFIC_FLASH_RESERVATION 2

//uint32_t flash_app_data_store_addr_base = 0;

void flash_app_init(void)
{
#if !USE_NVRAM
 #define SPIFFY_CLK_FREQ_12MHZ 12000000
    extern void sfi_setSpiffyClk(UINT32 uSpiClkSpeed);

    uint32_t total_flash_size = FLASH_SIZE;

    sfi_setSpiffyClk(SPIFFY_CLK_FREQ_12MHZ);
    wiced_hal_sflash_init();

    wiced_hal_sflash_set_size(total_flash_size);

    //flash_app_data_store_addr_base = (total_flash_size - (APPLICATION_SPECIFIC_FLASH_RESERVATION * FLASH_SECTOR_SIZE));
#endif
}

//Reads the given length of data from SF/EEPROM. If success returns len, else returns 0
uint32_t flash_app_read_mem(uint32_t read_from, uint8_t *buf, uint32_t len)
{
#if USE_NVRAM
    wiced_result_t ret;
    if (read_from == FLASH_ADDR_CONFIG)
    {
        return wiced_hal_read_nvram(0x280, len, buf, &ret);
    }
    else if (read_from == FLASH_ADDR_CNT)
    {
        return wiced_hal_read_nvram(0x281, len, buf, &ret);
    }
    else
    {
        return 0;
    }
#else
    if (ptrReadDataFromSFlash(read_from,buf,len) == len)
    {
        if (ptrReadDataFromSFlash(read_from-FLASH_ADDR_BCK_OFFSET,buf,len) == len)
        {
            return len;
        }
        else
        {
            ptrWriteDataToSFlash(read_from-FLASH_ADDR_BCK_OFFSET,buf,len);
            return len;
        }
    }

    if (ptrReadDataFromSFlash(read_from-FLASH_ADDR_BCK_OFFSET,buf,len) == len)
    {
        ptrWriteDataToSFlash(read_from,buf,len);
        return len;
    }
    
    LOG_ERROR("Read failure.\n");
    return 0;
#endif
}

uint32_t flash_app_write_mem(uint32_t addr, uint8_t *data, uint32_t len)
{
#if USE_NVRAM
    wiced_result_t ret;
    if (addr == FLASH_ADDR_CONFIG)
    {
        return wiced_hal_write_nvram(0x280, len, data, &ret);
    }
    else if (addr == FLASH_ADDR_CNT)
    {
        return wiced_hal_write_nvram(0x281, len, data, &ret);
    }
    else
    {
        return 0;
    }
#else
    // int retry = 0;
    // while (retry < 5)
    {
        wiced_hal_sflash_erase(addr&0xFFFFF000, FLASH_SECTOR_SIZE);
        if(ptrWriteDataToSFlash(addr, data, len) == len) 
        {
            wiced_hal_sflash_erase((addr-FLASH_ADDR_BCK_OFFSET)&0xFFFFF000, FLASH_SECTOR_SIZE);
            if (ptrWriteDataToSFlash((addr-FLASH_ADDR_BCK_OFFSET), data, len) == len) 
            {
                LOG_VERBOSE("Write OK, times: %d\n", retry);
            }
            return len;
        }
        // retry++;
    }

    LOG_ERROR("Write failure.\n");
    return 0;
#endif
}

uint32_t flash_write_erase(uint32_t addr, uint8_t *data, uint32_t len,uint8_t erase)
{
    // int retry = 0;
    // while (retry < 5)
    {
        if(erase == WICED_TRUE)
        {
            wiced_hal_sflash_erase(addr&0xFFFFF000, FLASH_SECTOR_SIZE);
        }
        if(ptrWriteDataToSFlash(addr, data, len) == len) 
        {
            if(erase == WICED_TRUE)
            {
                wiced_hal_sflash_erase((addr-FLASH_ADDR_BCK_OFFSET)&0xFFFFF000, FLASH_SECTOR_SIZE);
            }
            ptrWriteDataToSFlash((addr-FLASH_ADDR_BCK_OFFSET), data, len); 
            LOG_VERBOSE("Write OK, times: %d\n", retry);
            return len;
        }
        // if(erase != WICED_TRUE)
        // {
        //     break;
        // }
        // retry++;
    }

    LOG_ERROR("Write failure.\n");
    return 0;
}



//*****************************************************************************
// 函数名称: flashReadBlockSave
// 函数描述: 从存储区中查找有效的块并读取数据，如果一个数据块读取的内容失效，从另外一个数据块读取数据，两个数据都失效，判断读取失败
//          采用此存储方式时，每个扇区仅存储一个配置内容,数据长度不得超过分块的数据长度
// 函数输入: read_from: 读取地址，必须为扇区的起始地址 buf：读取缓存  len：数据长度不得超过分块的数据长度-4(4字节校验的长度)
// 函数返回值: 
//*****************************************************************************/
uint32_t flashReadBlockSave(uint32_t read_from, uint8_t *buf, uint32_t len)
{
    uint8_t sectorheadinfo[MAXBLOCKFORDATAPERSECTOR] = {0};
    uint8_t i;
    if(LENPERBLOCK < len)
    {
        return 0;
    }

    //读取扇区头 ,不要使用带校验的形式读取
    if (wiced_hal_sflash_read(read_from&0xFFFFF000,MAXBLOCKFORDATAPERSECTOR,sectorheadinfo) == MAXBLOCKFORDATAPERSECTOR)
    {
        WICED_BT_TRACE_ARRAY(sectorheadinfo,MAXBLOCKFORDATAPERSECTOR,"Read from %d:",read_from);
        for(i=0; i<MAXBLOCKFORDATAPERSECTOR; i++)
        {
            if(sectorheadinfo[i] == BLOCKVISIBLE)
            {
                break;
            }
        }
        if(i != MAXBLOCKFORDATAPERSECTOR)
        {
            if (ptrReadDataFromSFlash(read_from + (i+1)*LENPERBLOCK,buf,len) == len)
            {
                return len;
            }
        }
    }

    //读取扇区头 ,不要使用带校验的形式读取
    read_from -= FLASH_ADDR_BCK_OFFSET;
    if (wiced_hal_sflash_read(read_from&0xFFFFF000,MAXBLOCKFORDATAPERSECTOR,sectorheadinfo) == MAXBLOCKFORDATAPERSECTOR)
    {
        WICED_BT_TRACE_ARRAY(sectorheadinfo,MAXBLOCKFORDATAPERSECTOR,"Read from %d:",read_from);
        for(i=0; i<MAXBLOCKFORDATAPERSECTOR; i++)
        {
            if(sectorheadinfo[i] == BLOCKVISIBLE)
            {
                break;
            }
        }
        if(i != MAXBLOCKFORDATAPERSECTOR)
        {
            if (ptrReadDataFromSFlash(read_from + (i+1)*LENPERBLOCK,buf,len) == len)
            {
                return len;
            }
        }
    }
    
    LOG_ERROR("Read failure.\n");
    return 0;
}

//*****************************************************************************
// 函数名称: flashReadBlockSave
// 函数描述: 写入数据到存储区，每次会两份数据到当前扇区及对应的备份扇区，默认都写入成功
//          采用此存储方式时，每个扇区仅存储一个配置内容
// 函数输入:  addr: 写入地址，必须为扇区的起始地址 data：读取缓存  len：数据长度不得超过分块的数据长度-4(4字节校验的长度)
// 函数返回值: 
//*****************************************************************************/
uint32_t flashWriteBlockSave(uint32_t addr, uint8_t *data, uint32_t len)
{
    uint8_t sectorheadinfo[MAXBLOCKFORDATAPERSECTOR] = {0};
    uint8_t erasesector = 0;
    uint8_t i=0;
    uint32_t addr_write;
    
    if(LENPERBLOCK < len)
    {
        return 0;
    }

    if(MAXBLOCKFORDATAPERSECTOR != wiced_hal_sflash_read(addr,MAXBLOCKFORDATAPERSECTOR,sectorheadinfo))  //
    {
        //读取失败，擦除整个扇区后重新存储
        wiced_hal_sflash_erase(addr&0xFFFFF000, FLASH_SECTOR_SIZE);
        memset(sectorheadinfo,BLOCKNOTUSE,MAXBLOCKFORDATAPERSECTOR);
    }
    //查找有效的扇区 写入数据时先写入扇区头，所以不为BLOCKNOTUSE,一定是被占用了
    for(i = 0; i < MAXBLOCKFORDATAPERSECTOR; i++)
    {
        if(sectorheadinfo[i] == BLOCKNOTUSE)
        {
            break;
        }
    }
    //数据存满了 擦除整个扇区
    if(i == MAXBLOCKFORDATAPERSECTOR)
    {
        wiced_hal_sflash_erase(addr&0xFFFFF000, FLASH_SECTOR_SIZE);
        memset(sectorheadinfo,BLOCKNOTUSE,MAXBLOCKFORDATAPERSECTOR);
        erasesector = 1;
        i = 0;
    }
    //写入扇区头 ,不要使用带校验的形式写入
    if(i > 0)
    {
        sectorheadinfo[i-1] = BLOCKINVISIBLE;
    }
    sectorheadinfo[i] = BLOCKVISIBLE;
    wiced_hal_sflash_write(addr&0xFFFFF000,MAXBLOCKFORDATAPERSECTOR,sectorheadinfo);
    //写入数据到存储区
    addr_write = (addr&0xFFFFF000) + (i+1)*LENPERBLOCK;
    ptrWriteDataToSFlash(addr_write,data,len);

    // 读取备份扇区的数据  备份扇区的值在写入时永远保持与第一扇区一致
    addr -= FLASH_ADDR_BCK_OFFSET; 
    //数据存满了 擦除整个扇区
    if(1 == erasesector)
    {
        wiced_hal_sflash_erase(addr&0xFFFFF000, FLASH_SECTOR_SIZE);
    }
    //写入扇区头,不要使用带校验的形式写入
    wiced_hal_sflash_write(addr&0xFFFFF000,MAXBLOCKFORDATAPERSECTOR,sectorheadinfo);
    //写入数据到存储区
    addr_write = (addr&0xFFFFF000) + (i+1)*LENPERBLOCK;
    ptrWriteDataToSFlash(addr_write,data,len);

    return len;
}

