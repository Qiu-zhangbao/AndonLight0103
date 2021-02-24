#pragma once

#include "wiced_platform.h"
#include "wiced_hal_sflash.h"

//#define FLASH_ADDR (FLASH_SIZE - FLASH_SECTOR_SIZE)
//#define FLASH_ADDR_CNT (FLASH_SIZE - 2 * FLASH_SECTOR_SIZE)

#define FLASH_USERBLOCK_SIZE          128 


#define   MAXBLOCKPERSECTOR                         32
#define   MAXBLOCKFORDATAPERSECTOR                  MAXBLOCKPERSECTOR-1   //空余一个block存储头信息
#define   LENPERBLOCK                               FLASH_SECTOR_SIZE/MAXBLOCKPERSECTOR

#define FLASH_ADDR                    (FLASH_SIZE - FLASH_SECTOR_SIZE)
#define FLASH_ADDR_CNT                (FLASH_ADDR - FLASH_SECTOR_SIZE)
#define FLASH_ADDR_CONFIG             (FLASH_ADDR_CNT - FLASH_SECTOR_SIZE)

#define FLASH_ADDR_XXTEA_SN           (0xFC000)  //此位置不允许更改 固定为0xFC000

#define DID_NEW_TYPE

#ifndef DID_NEW_TYPE
#define FLASH_XXTEA_SN_LEN           34        //byte
#define FLASH_XXTEA_ID_OFFSET        0         //DID在SN中的偏移
#define FLASH_XXTEA_ID_LEN           18        //byte
#define FLASH_XXTEA_KEY_OFFSET       18        //ID和KEY直接由一个逗号分隔符
#define FLASH_XXTEA_KEY_LEN          16        //byte
#else
#define FLASH_XXTEA_SN_LEN           53        //byte
#define FLASH_XXTEA_ID_OFFSET        0         //DID在SN中的偏移
#define FLASH_XXTEA_ID_LEN           20        //byte
#define FLASH_XXTEA_KEY_OFFSET       21        //ID和KEY直接由一个逗号分隔符
#define FLASH_XXTEA_KEY_LEN          32        //byte

// #define FLASH_XXTEA_SN_LEN           41        //byte
// #define FLASH_XXTEA_ID_OFFSET        0         //DID在SN中的偏移
// #define FLASH_XXTEA_ID_LEN           20        //byte
// #define FLASH_XXTEA_KEY_OFFSET       21        //ID和KEY直接由一个逗号分隔符
// #define FLASH_XXTEA_KEY_LEN          20        //byte
#endif

#define FLASH_ADDR_CTRREMOTE          (FLASH_ADDR_XXTEA_SN - FLASH_SECTOR_SIZE)
#define FLASH_ADDR_BINDKEY            (FLASH_ADDR_CTRREMOTE - FLASH_SECTOR_SIZE)
#define FLASH_ADDR_USERTIMER          (FLASH_ADDR_BINDKEY - FLASH_SECTOR_SIZE)

#if LIGHTAI == configLIGHTAIANDONMODE
#define FLASH_ADDR_AUTOBRIGHTNESS     (FLASH_ADDR_USERTIMER - FLASH_SECTOR_SIZE)
#endif
//备份内容的偏移量，应用使用的FLASH从后向前使用，所以备份存储区为存储地址减去偏移地址
#define FLASH_ADDR_BCK_OFFSET         FLASH_SECTOR_SIZE*16   


typedef struct
{
    uint8_t onoff_state;
    uint8_t led_state;
    uint8_t led_num;
} onoff_app_data_t;

extern void flash_app_init(void);

//直接从指定地址读取内容
extern uint32_t flash_app_read_mem(uint32_t read_from, uint8_t *buf, uint32_t len);
//存储内容到指定地址，每次存储时删除整个扇区
extern uint32_t flash_app_write_mem(uint32_t write_to, uint8_t *data, uint32_t len);

extern uint32_t flash_write_erase(uint32_t addr, uint8_t *data, uint32_t len,uint8_t erase);

//写入地址到对应的地址，一个地址对应一个扇区，每个存储内容占用固定的大小，在扇区头部标识当前有效块
//每个扇区被分成32个128字节的块，第一个为扇区头标识，占用31byte，每个byte代表一个块，0xFF表示未使用，0x55表示正在使用，0x00表示无效
extern uint32_t flashReadBlockSave(uint32_t read_from, uint8_t *buf, uint32_t len);
extern uint32_t flashWriteBlockSave(uint32_t addr, uint8_t *data, uint32_t len);

