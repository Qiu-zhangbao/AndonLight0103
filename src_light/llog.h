#pragma once

#include "stdint.h"

typedef struct {
    uint16_t operatetype;             //操作类型
    uint16_t deltatime;               //上次与本次操作的时间间隔
    uint32_t lasttime;                //上次操作的时间，为0表示无效
    uint32_t logvalue;                //log的数值，依据不同log类型确定
} LogConfig_def;

extern uint16_t llogsaveflag;
extern LogConfig_def   LogConfig;

void llog_write(void);
uint16_t llog_read(uint8_t *buffer, uint16_t *len,uint16_t read_time);
