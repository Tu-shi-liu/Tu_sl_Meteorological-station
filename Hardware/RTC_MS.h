#ifndef __RTC_H
#define __RTC_H

void RTC_Init(void);
void RTC_SetTime(uint16_t year, uint8_t mon, uint8_t day,
                 uint8_t hour, uint8_t min, uint8_t sec);
void RTC_GetTime(uint16_t *year, uint8_t *mon, uint8_t *day,
                 uint8_t *hour, uint8_t *min, uint8_t *sec);
uint32_t RTC_GetMyTimestamp(void);
void RTC_SetAlarmForWakeup(uint32_t seconds);
void RTC_DisableAlarm(void);
void Enter_StandbyMode(void);
void RTC_ConvertTimestamp(uint32_t ts, uint16_t *y, uint8_t *m, uint8_t *d,
                         uint8_t *h, uint8_t *mi, uint8_t *s);

#endif
