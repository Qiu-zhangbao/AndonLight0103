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

/* ����AIģ��ʱ��Ҫ�ȵ��øú�����ʼ��AIģ�� */
void init_model();
/* ���û����ڵ�������ʱ��ͨ���ú�����AIģ���ϱ������Ϣ  nBrightness : 1-100 */
void ai_train(StTimeInfo stTimeInfo, int nBrightness);
/* ���û���������ʱ�����øú�����Ԥ�⵱ǰ��������ֵ */
int ai_predict(StTimeInfo stTimeInfo);
/* ������ʹ��AIģ��ʱ���ôʺ����ͷ������Դ */
void release_model();

#endif
