#include "stm32f10x.h"                  // Device header
#include "Time_Setting.h"
#include "RTC_MS.h"        
#include "Key.h"        
#include "OLED.h"      
#include <stdio.h>

static uint8_t  setting_mode = 0;    // 0=正常 1=设置模式
static uint8_t  field_index = 0;     // 0:年,1:月,2:日,3:时,4:分,5:秒
static uint16_t temp_year;
static uint8_t  temp_mon, temp_day, temp_hour, temp_min, temp_sec;
static uint8_t  wait_release = 0; 

static uint8_t GetMaxDay(uint16_t year, uint8_t month) {
    if (month == 2) {
        if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
            return 29;
        else
            return 28;
    } else if (month == 4 || month == 6 || month == 9 || month == 11) {
        return 30;
    }
    return 31;
}

static void Setting_Display(void) {
    char str[17];
    const char *field_names[] = {"Year", "Month", "Day", "Hour", "Min", "Sec"};

    OLED_Clear();
    OLED_ShowString(1, 1, "Set Time:");
    sprintf(str, "%04d-%02d-%02d", temp_year, temp_mon, temp_day);
    OLED_ShowString(2, 1, str);
    sprintf(str, "%02d:%02d:%02d", temp_hour, temp_min, temp_sec);
    OLED_ShowString(3, 1, str);
    sprintf(str, "Field: %-5s", field_names[field_index]);
    OLED_ShowString(4, 1, str);
}

static void Save_Time(void) {
    RTC_SetTime(temp_year, temp_mon, temp_day, temp_hour, temp_min, temp_sec);
}

void TimeSetting_Init(void) {
    setting_mode = 0;
    field_index = 0;
    wait_release = 0;
}

uint8_t TimeSetting_Process(void) {
    if (wait_release) {
        if (!Key_IsPressed(KEY1) && !Key_IsPressed(KEY2) && !Key_IsPressed(KEY3)) {
            wait_release = 0;
            Key_ClearAllEvents(); 
        } else {
            return setting_mode ? 1 : 0;
        }
    }

    if (!setting_mode) {
        uint8_t ev3 = Key_GetEvent(KEY3);  
        if (ev3 == KEY_EVENT_LONG_PRESS) {
            uint16_t y; uint8_t m, d, h, mi, s;
            RTC_GetTime(&y, &m, &d, &h, &mi, &s);
            temp_year = y; temp_mon = m; temp_day = d;
            temp_hour = h; temp_min = mi; temp_sec = s;
            field_index = 0;
            setting_mode = 1;
            wait_release = 1;
            Setting_Display();
            return 1; 
        }
        return 0; 
    }

    uint8_t ev1 = Key_GetEvent(KEY1);
    uint8_t ev2 = Key_GetEvent(KEY2);
    uint8_t ev3 = Key_GetEvent(KEY3);

    if (ev3 == KEY_EVENT_DOUBLE_CLICK) {
        Save_Time();
        setting_mode = 0;
        return 0;
    }

    if (ev3 == KEY_EVENT_LONG_PRESS) {
        setting_mode = 0;
        wait_release = 1;
        return 0;
    }

    if (ev3 == KEY_EVENT_SINGLE_CLICK) {
        field_index = (field_index + 1) % 6;
        Setting_Display();
        return 1;
    }

    if (ev1 == KEY_EVENT_SINGLE_CLICK) {
        switch (field_index) {
            case 0: temp_year  = (temp_year  >= 2099) ? 2000 : temp_year + 1; break;
            case 1: temp_mon   = (temp_mon   >= 12)   ? 1    : temp_mon + 1;  break;
            case 2: {
                uint8_t maxd = GetMaxDay(temp_year, temp_mon);
                temp_day   = (temp_day   >= maxd) ? 1 : temp_day + 1;
                break;
            }
            case 3: temp_hour  = (temp_hour  >= 23) ? 0 : temp_hour + 1; break;
            case 4: temp_min   = (temp_min   >= 59) ? 0 : temp_min + 1;  break;
            case 5: temp_sec   = (temp_sec   >= 59) ? 0 : temp_sec + 1;  break;
        }
        Setting_Display();
        return 1;
    }

    if (ev2 == KEY_EVENT_SINGLE_CLICK) {
        switch (field_index) {
            case 0: temp_year  = (temp_year  <= 2000) ? 2099 : temp_year - 1; break;
            case 1: temp_mon   = (temp_mon   <= 1)    ? 12   : temp_mon - 1;  break;
            case 2: {
                temp_day = (temp_day <= 1) ? GetMaxDay(temp_year, temp_mon) : temp_day - 1;
                break;
            }
            case 3: temp_hour  = (temp_hour  == 0) ? 23 : temp_hour - 1; break;
            case 4: temp_min   = (temp_min   == 0) ? 59 : temp_min - 1;  break;
            case 5: temp_sec   = (temp_sec   == 0) ? 59 : temp_sec - 1;  break;
        }
        Setting_Display();
        return 1;
    }

    return 1;  
}
