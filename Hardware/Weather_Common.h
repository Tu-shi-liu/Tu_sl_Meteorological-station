#ifndef __WEATHER_COMMON_H
#define __WEATHER_COMMON_H

typedef struct {
    uint32_t timestamp;   // My timestamp
    int16_t  temperature; // x10
    uint16_t humidity;    // x10
    uint32_t pressure;    // Pa
} LogRecord;

//	报警阈值设置栏
#define TEMP_HIGH  320   // 32.0 C
#define TEMP_LOW   -100  // -10.0 C
#define HUMI_HIGH  800   // 80.0%
#define HUMI_LOW   200   // 20.0%
#define PRES_HIGH  108000 // 1080 hPa -> Pa
#define PRES_LOW   90000  // 900 hPa -> Pa
//	工作模式定义
#define MODE_CONTINUOUS 0x01
#define MODE_LOWPOWER   0x02

#endif
