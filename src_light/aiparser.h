#ifndef AIPARSER_H
#define AIPARSER_H

typedef struct
{
    int nYear;
    int nMonth;
    int nDay;
    int nWeek;
    int nHour;
    int nMinute;
    int nSecond;
}StTimeInfo;

/* 调用AI模块时需要先调用该函数初始化AI模块 */
void init_model();
/* 当用户调节灯泡亮度时，通过该函数向AI模块上报相关信息  nBrightness : 1-100 */
void ai_train(StTimeInfo stTimeInfo, int nBrightness);
/* 当用户开启灯泡时，调用该函数来预测当前灯泡亮度值 */
int ai_predict(StTimeInfo stTimeInfo);
/* 当不在使用AI模块时调用词函数释放相关资源 */
void release_model();

#endif
