#include "stm32f10x.h"                  // Device header
#include "BPK.h"

#define BKP_MODE  BKP_DR10 

static void RTC_NVIC_Config(void) {
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = RTCAlarm_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

void RTC_Init(void) {
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_BKP, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    PWR_BackupAccessCmd(ENABLE);

    if(BKP_Read(BKP_DR1) != 0xA5A5) {
        RCC_LSEConfig(RCC_LSE_ON);
        while(RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET);
        RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
        RCC_RTCCLKCmd(ENABLE);
        RTC_WaitForSynchro();
        RTC_WaitForLastTask();

        RTC_SetPrescaler(32767); 
        RTC_WaitForLastTask();
        BKP_Write(BKP_DR1, 0xA5A5);
    } else {
        RCC_RTCCLKCmd(ENABLE);
    }

    RTC_NVIC_Config();
    RTC_WaitForSynchro();
}

/* 将年月日时分秒写入RTC计数器 (以2026-01-01为基准计算秒) */
static uint32_t date_to_seconds(uint16_t y, uint8_t m, uint8_t d,
                                uint8_t h, uint8_t mi, uint8_t s) {
    uint32_t days = 0;
    uint16_t year;
    const uint8_t month_days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    for(year = 2026; year < y; year++) {
        days += 365 + ((year%4==0 && year%100!=0) || (year%400==0) ? 1 : 0);
    }
    for(uint8_t i=0; i<m-1; i++) {
        days += month_days[i];
        if(i == 1 && ((y%4==0 && y%100!=0) || (y%400==0))) days++;
    }
    days += d - 1;
    return days*86400 + h*3600 + mi*60 + s;
}

static void sec2date(uint32_t sec, uint16_t *y, uint8_t *m, uint8_t *d,
                            uint8_t *h, uint8_t *mi, uint8_t *s) {
    *y = 2026;
    while(1) {
        uint32_t year_sec = 365*86400;
        if((*y%4==0 && *y%100!=0) || (*y%400==0)) year_sec += 86400;
        if(sec >= year_sec) { sec -= year_sec; (*y)++; }
        else break;
    }
    const uint8_t mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    *m = 1;
    while(1) {
        uint32_t m_sec = mdays[*m-1]*86400;
        if(*m==2 && ((*y%4==0 && *y%100!=0) || (*y%400==0))) m_sec += 86400;
        if(sec >= m_sec) { sec -= m_sec; (*m)++; }
        else break;
    }
    *d = sec/86400 + 1;
    sec %= 86400;
    *h = sec / 3600;
    sec %= 3600;
    *mi = sec / 60;
    *s = sec % 60;
}

void RTC_SetTime(uint16_t year, uint8_t mon, uint8_t day,
                 uint8_t hour, uint8_t min, uint8_t sec) {
    uint32_t ts = date_to_seconds(year, mon, day, hour, min, sec);
    RTC_WaitForLastTask();
    RTC_SetCounter(ts);
    RTC_WaitForLastTask();
}

void RTC_GetTime(uint16_t *year, uint8_t *mon, uint8_t *day,
                 uint8_t *hour, uint8_t *min, uint8_t *sec) {
    uint32_t ts = RTC_GetCounter();
    sec2date(ts, year, mon, day, hour, min, sec);
}

uint32_t RTC_GetMyTimestamp(void) {
    RTC_WaitForSynchro(); 
	return RTC_GetCounter();
}

void RTC_ConvertTimestamp(uint32_t ts, uint16_t *y, uint8_t *m, uint8_t *d,
                         uint8_t *h, uint8_t *mi, uint8_t *s) {
    sec2date(ts, y, m, d, h, mi, s);
}

void RTC_SetAlarmForWakeup(uint32_t seconds) {
    uint32_t now = RTC_GetCounter();
    uint32_t alarm = now + seconds;
    RTC_SetAlarm(alarm);
	RTC_WaitForSynchro();
    RTC_WaitForLastTask();
}

void RTC_DisableAlarm(void) {
    RTC_ITConfig(RTC_IT_ALR, DISABLE);
    RTC_WaitForLastTask();
}

void Enter_StandbyMode(void) {
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    PWR_WakeUpPinCmd(ENABLE);
    PWR_EnterSTANDBYMode();
}
