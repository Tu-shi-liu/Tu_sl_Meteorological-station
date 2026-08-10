#include "stm32f10x.h"
#include "Weather_Common.h"
#include "BPK.h"
#include "Key.h"
#include "OLED.h"
#include "MyI2C.h"
#include "RTC_MS.h"
#include "W25Q64.h"
#include "Time_Setting.h"
#include "AHT20_BMP280.h"
#include <stdio.h>
#include <string.h>

#define BEEP_PIN GPIO_Pin_8
#define LED_PIN  GPIO_Pin_13
#define BEEP_PORT GPIOA
#define LED_PORT  GPIOC
#define PAGE1_TIME 8
#define PAGE2_TIME 8
#define COLLECT_CONT 5
#define COLLECT_LP   30
#define ALARM_TIME   15
#define BKP_MODE  BKP_DR10

extern volatile uint32_t g_msTick;

static void GPIO_Init_Alarm(void);
static void DataCollect(void);
static void CheckAlarm(void);
static void ALARM_Trigger(void);
static void Work_Indicating_LED(void);
static void AlarmProc(void);
static void Page1(void);
static void Page2(void);
static void Page3(void);
static void EnterLowPower(void);

static uint8_t cur_page = 1, auto_cycle = 1;
static uint32_t page_timer = 0;
static uint8_t work_mode = MODE_CONTINUOUS;
static float temp_aht, temp_bmp, humi, press;
static int32_t hist_idx = 0;
static uint32_t hist_total = 0;
static uint8_t alarm_act = 0, alarm_en = 1;
static uint32_t alarm_cnt = 0;
static uint16_t last_y; static uint8_t last_m, last_d, last_h, last_min, last_s, last_w;
static uint8_t p1_first = 1;
static uint8_t p2_first = 1;
static uint32_t last_collect = 0;
static uint8_t lp_display_cnt = 0;

int main(void) {
    SystemInit();
    if (SysTick_Config(SystemCoreClock / 1000)) while (1);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); 
    uint8_t is_user_wakeup = (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == Bit_SET);

    BKP_Init();
    RTC_Init();
	Key_Init();
	OLED_Init();
	MyI2C_Init();
	W25Q64_Init();
    TimeSetting_Init();
    GPIO_Init_Alarm();

    OLED_Clear();
    if (!AHT20_Init() || !BMP280_Init()) {
        OLED_ShowString(2, 1, "Sensor fail!");
        OLED_ShowString(3, 4, "Power off");
        OLED_ShowString(4, 3, "and restart!");
        while (1);
    }
    { float d; BMP280_Read(&d, &d); }
    OLED_Clear();
	
	DataCollect();
	last_collect = RTC_GetMyTimestamp();

    uint16_t mode = BKP_Read(BKP_MODE);
    if (mode == MODE_CONTINUOUS || mode == MODE_LOWPOWER) {
        work_mode = mode;
    } 
	else {
        BKP_Write(BKP_MODE, MODE_CONTINUOUS);
    }

	if (work_mode == MODE_LOWPOWER && PWR_GetFlagStatus(PWR_FLAG_SB) == SET)  {
			PWR_ClearFlag(PWR_FLAG_SB);
			if (is_user_wakeup) {
				last_collect = RTC_GetMyTimestamp();
				lp_display_cnt = 20;
			} 
			else {
				Work_Indicating_LED();
				DataCollect();
				EnterLowPower();
			}
	}
    
    p1_first = 1;
    p2_first = 1;
	ALARM_Trigger();
    Key_ClearAllEvents();

    while (1) {
        if (TimeSetting_Process()) {
            p1_first = 1;
            continue;
        }
        uint8_t e1 = Key_GetEvent(KEY1), e2 = Key_GetEvent(KEY2), e3 = Key_GetEvent(KEY3);

        if (e1 == KEY_EVENT_SINGLE_CLICK) {
            if (cur_page == 3) {
                if (hist_idx < (int32_t)(hist_total - 1)) {
                    hist_idx++;
                    Page3();
                }
            } else {
                cur_page = (cur_page == 1) ? 2 : 1;
                page_timer = 0;
                auto_cycle = 1;
                if (cur_page == 1) {
                    p1_first = 1;
                } else {
                    p2_first = 1;
                    Page2();
                }
            }
        } else if (e1 == KEY_EVENT_DOUBLE_CLICK) {
            if (cur_page == 3) {
                cur_page = 1;
                auto_cycle = 1;
                p1_first = 1;
            } else {
                cur_page = 3;
                auto_cycle = 0;
                hist_total = W25Q64_GetRecordCount();
                hist_idx = 0;
                Page3();
            }
        } else if (e1 == KEY_EVENT_LONG_PRESS) {
            BKP_Write(BKP_MODE, work_mode);
            if (work_mode == MODE_LOWPOWER) {
                RTC_SetAlarmForWakeup(COLLECT_LP);
            }
            OLED_ShowStop();
            EnterLowPower();
        }

        if (e2 == KEY_EVENT_SINGLE_CLICK && cur_page == 3) {
            if (hist_idx > 0) {
                hist_idx--;
                Page3();
            }
        } else if (e2 == KEY_EVENT_DOUBLE_CLICK) {
            if (alarm_act) {
                alarm_act = 0;
                GPIO_ResetBits(BEEP_PORT, BEEP_PIN);
                GPIO_SetBits(LED_PORT, LED_PIN);
            } else {
                alarm_en = !alarm_en;
            }
        } else if (e2 == KEY_EVENT_LONG_PRESS) {
            work_mode = (work_mode == MODE_CONTINUOUS) ? MODE_LOWPOWER : MODE_CONTINUOUS;
            BKP_Write(BKP_MODE, work_mode);
			if (work_mode == MODE_LOWPOWER) {
				lp_display_cnt = 10;
			}
        }

        uint32_t now_sec = RTC_GetMyTimestamp();
        static uint32_t last_sec = 0;
		if (now_sec != last_sec) {
			last_sec = now_sec;
			AlarmProc();
			
			if (lp_display_cnt > 0) {
				lp_display_cnt--;
					if (lp_display_cnt == 0) {
						EnterLowPower();
					}
			}
		
			if (cur_page != 3 && auto_cycle) {
				page_timer++;
				if (cur_page == 1 && page_timer >= PAGE1_TIME) {
					cur_page = 2;
					page_timer = 0;
					p2_first = 1;
					Page2();
				} else if (cur_page == 2 && page_timer >= PAGE2_TIME) {
					cur_page = 1;
					page_timer = 0;
					p1_first = 1;
					Page1();
				}
			}
			
			uint32_t interval = (work_mode == MODE_CONTINUOUS) ? COLLECT_CONT : COLLECT_LP;
			if (now_sec - last_collect >= interval) {
				last_collect = now_sec;
				DataCollect();
			}
		}

        static uint32_t last_ref = 0;
        if (g_msTick - last_ref >= 200) {
            last_ref = g_msTick;
            if (cur_page == 1) {
                Page1();
            } else if (cur_page == 2) {
                Page2();
            }
        }
    }
}

static void GPIO_Init_Alarm(void) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_InitTypeDef Structure;
    Structure.GPIO_Pin = BEEP_PIN;
    Structure.GPIO_Mode = GPIO_Mode_Out_PP;
    Structure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BEEP_PORT, &Structure);

    Structure.GPIO_Pin = LED_PIN;
    Structure.GPIO_Mode = GPIO_Mode_Out_PP;
    Structure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LED_PORT, &Structure);

    GPIO_ResetBits(BEEP_PORT, BEEP_PIN);
    GPIO_SetBits(LED_PORT, LED_PIN);
}

static void DataCollect(void) {
    float t, h, p;
    if (AHT20_Read(&t, &h)) {
        temp_aht = t;
        humi = h;
    }
    if (BMP280_Read(&p, &t)) {
        press = p;
        temp_bmp = t;
    }
    LogRecord r;
    r.timestamp = RTC_GetMyTimestamp();
    r.temperature = (int16_t)(temp_bmp * 10);
    r.humidity = (uint16_t)(humi * 10);
    r.pressure = (uint32_t)(press * 100);
    W25Q64_WriteRecord(&r);
    CheckAlarm();
}

static void CheckAlarm(void) {
    if (!alarm_en) return;
    int16_t t = (int16_t)(temp_bmp * 10);
    uint16_t h = (uint16_t)(humi * 10);
    uint32_t p = (uint32_t)(press * 100);
    if (t > TEMP_HIGH || t < TEMP_LOW || h > HUMI_HIGH || h < HUMI_LOW || p > PRES_HIGH || p < PRES_LOW) {
        if (!alarm_act) {
            alarm_act = 1;
            alarm_cnt = ALARM_TIME;
        }
    }
}

static void ALARM_Trigger(void) {
    if (!alarm_act) {
        alarm_act = 1;
        alarm_cnt = 2; 
        GPIO_ResetBits(LED_PORT, LED_PIN);
        GPIO_SetBits(BEEP_PORT, BEEP_PIN);
    }
}

static void Work_Indicating_LED(void) {
    if (!alarm_act) {
        alarm_act = 1;
        alarm_cnt = 2;
        GPIO_ResetBits(LED_PORT, LED_PIN);
    }
}

static void AlarmProc(void) {
    static uint8_t alarm_state = 0;

    if (!alarm_act) {
        GPIO_ResetBits(BEEP_PORT, BEEP_PIN);
        GPIO_SetBits(LED_PORT, LED_PIN);
        alarm_state = 0;
        return;
    }
    if (alarm_cnt == 0) {
        alarm_act = 0;
        GPIO_ResetBits(BEEP_PORT, BEEP_PIN);
        GPIO_SetBits(LED_PORT, LED_PIN);
        alarm_state = 0;
        return;
    }
    GPIO_WriteBit(BEEP_PORT, BEEP_PIN, (BitAction)alarm_state);
    GPIO_WriteBit(LED_PORT, LED_PIN, (BitAction)alarm_state);
    alarm_state = !alarm_state;
    alarm_cnt--;
}

static void Page1(void) {
    uint16_t y; uint8_t m, d, h, mi, s;
    RTC_GetTime(&y, &m, &d, &h, &mi, &s);
    uint8_t w = ((RTC_GetMyTimestamp() / 86400) + 4) % 7;
    const char *wn[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    char buf[17];

    if (p1_first) {
        OLED_Clear();
        OLED_ShowString(1, 1, "Meteorological");
        sprintf(buf, "%04d-%02d-%02d", y, m, d);
        OLED_ShowString(2, 1, buf);
        sprintf(buf, "Week: %s", wn[w]);
        OLED_ShowString(3, 1, buf);
        sprintf(buf, "%02d:%02d:%02d", h, mi, s);
        OLED_ShowString(4, 1, buf);
        last_y = y; last_m = m; last_d = d; last_h = h; last_min = mi; last_s = s; last_w = w;
        p1_first = 0;
    } else {
        if (y != last_y || m != last_m || d != last_d) {
            sprintf(buf, "%04d-%02d-%02d", y, m, d);
            OLED_ShowString(2, 1, buf);
            last_y = y; last_m = m; last_d = d;
        }
        if (w != last_w) {
            sprintf(buf, "Week: %s", wn[w]);
            OLED_ShowString(3, 1, buf);
            last_w = w;
        }
        if (h != last_h || mi != last_min || s != last_s) {
            sprintf(buf, "%02d:%02d:%02d", h, mi, s);
            OLED_ShowString(4, 1, buf);
            last_h = h; last_min = mi; last_s = s;
        }
    }
}

static void Page2(void) {
    static float last_temp_bmp = -999, last_temp_aht = -999, last_humi = -999, last_press = -999;
    static uint8_t last_mode = 0xFF, last_alarm = 0xFF;
    char buf[17];
    uint8_t len;

    if (p2_first) {
        OLED_Clear();
        sprintf(buf, "T:%.1fC H:%.1f%%", temp_aht, humi);
        OLED_ShowString(1, 1, buf);
        sprintf(buf, "P:%.1fhPa", press);
        OLED_ShowString(2, 1, buf);
        sprintf(buf, "TBMP:%.1fC", temp_bmp);
        OLED_ShowString(3, 1, buf);
        sprintf(buf, "%s Alarm:%s", (work_mode == MODE_LOWPOWER) ? "LP" : "CM",
                alarm_en ? "ON" : "OFF");
        OLED_ShowString(4, 1, buf);

        last_temp_bmp = temp_bmp; last_temp_aht = temp_aht; last_humi = humi; last_press = press;
        last_mode = work_mode; last_alarm = alarm_en;
        p2_first = 0;
        return;
    }

    uint8_t temp_aht_changed = (temp_aht - last_temp_aht > 0.05f) || (last_temp_aht - temp_aht > 0.05f);
    uint8_t temp_bmp_changed = (temp_bmp - last_temp_bmp > 0.05f) || (last_temp_bmp - temp_bmp > 0.05f);
    uint8_t humi_changed = (humi - last_humi > 0.05f) || (last_humi - humi > 0.05f);

    if (temp_aht_changed || humi_changed) {
        sprintf(buf, "T:%.1fC H:%.1f%%", temp_aht, humi);
        OLED_ShowString(1, 1, buf);
        len = strlen(buf);
        for (uint8_t col = len + 1; col <= 16; col++) {
			OLED_ShowChar(1, col, ' ');
		}

        if (temp_aht_changed) {
            last_temp_aht = temp_aht;
        }
        last_humi = humi;
    }

    if (temp_bmp_changed) {
        sprintf(buf, "TBMP:%.1fC", temp_bmp);
        OLED_ShowString(3, 1, buf);
        len = strlen(buf);
        for (uint8_t col = len + 1; col <= 16; col++) {
			OLED_ShowChar(3, col, ' ');
		}
        last_temp_bmp = temp_bmp;
    }

    if ((press - last_press > 0.05f) || (last_press - press > 0.05f)) {
        sprintf(buf, "P:%.1fhPa", press);
        OLED_ShowString(2, 1, buf);
        len = strlen(buf);
        for (uint8_t col = len + 1; col <= 16; col++) {
			OLED_ShowChar(2, col, ' ');
		}
        last_press = press;
    }

    if (work_mode != last_mode || alarm_en != last_alarm) {
        sprintf(buf, "%s Alarm:%s", (work_mode == MODE_LOWPOWER) ? "LP" : "CM",
                alarm_en ? "ON" : "OFF");
        OLED_ShowString(4, 1, buf);
        len = strlen(buf);
        for (uint8_t col = len + 1; col <= 16; col++) {
			OLED_ShowChar(4, col, ' ');
		}
        last_mode = work_mode;
        last_alarm = alarm_en;
    }
}

static void Page3(void) {
    LogRecord r;
    uint32_t total = W25Q64_GetRecordCount();
    if (total == 0) {
        OLED_Clear();
        OLED_ShowString(2, 1, "No records");
        return;
    }
    if (hist_idx >= (int32_t)total) {
        hist_idx = total - 1;
    }
    if (W25Q64_ReadRecord(total - 1 - hist_idx, &r)) {
        uint16_t y; uint8_t m, d, h, mi, s;
        RTC_ConvertTimestamp(r.timestamp, &y, &m, &d, &h, &mi, &s);
        char buf[17];
        OLED_Clear();
        sprintf(buf, "%d/%d %02d:%02d", hist_idx + 1, total, h, mi);
        OLED_ShowString(1, 1, buf);
        sprintf(buf, "%04d-%02d-%02d", y, m, d);
        OLED_ShowString(2, 1, buf);
        sprintf(buf, "T:%.1fC H:%.1f%%", r.temperature / 10.0f, r.humidity / 10.0f);
        OLED_ShowString(3, 1, buf);
        sprintf(buf, "P:%.1fhPa", r.pressure / 100.0f);
        OLED_ShowString(4, 1, buf);
    }
}

static void EnterLowPower(void) {
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    PWR_WakeUpPinCmd(ENABLE);
	uint32_t Alarm = RTC_GetCounter() + COLLECT_LP;	
	RTC_SetAlarm(Alarm);
	RTC_WaitForSynchro();	
	RTC_WaitForLastTask();
	
	OLED_ShowStop();
    PWR_EnterSTANDBYMode();	
}
