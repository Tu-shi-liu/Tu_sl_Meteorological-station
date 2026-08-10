#include "stm32f10x.h"                  // Device header
#include "MyI2C.h"
#include <string.h>

#define AHT20_ADDR 0x38

/* ----- AHT20 ----- */
uint8_t AHT20_Init(void) {
    uint8_t status;
    MyI2C_Start();
    if (MyI2C_SendByte(AHT20_ADDR << 1 | 0)) { MyI2C_Stop(); return 0; }
    MyI2C_SendByte(0x71);
    MyI2C_Stop();
    MyI2C_DelayMs(1);

    MyI2C_Start();
    if (MyI2C_SendByte(AHT20_ADDR << 1 | 1)) { MyI2C_Stop(); return 0; }
    status = MyI2C_ReadByte(0);
    MyI2C_Stop();

    if ((status & 0x18) != 0x18) {
        MyI2C_Start();
        if (MyI2C_SendByte(AHT20_ADDR << 1 | 0)) return 0;
        MyI2C_SendByte(0xBE); MyI2C_SendByte(0x08); MyI2C_SendByte(0x00);
        MyI2C_Stop();
        MyI2C_DelayMs(10);
        for (uint8_t i = 0; i < 20; i++) {
            MyI2C_DelayMs(5);
            MyI2C_Start();
            if (MyI2C_SendByte(AHT20_ADDR << 1 | 0)) continue;
            MyI2C_SendByte(0x71);
            MyI2C_Stop();
            MyI2C_DelayMs(1);
            MyI2C_Start();
            if (MyI2C_SendByte(AHT20_ADDR << 1 | 1)) continue;
            status = MyI2C_ReadByte(0);
            MyI2C_Stop();
            if (status & 0x08) return 1;
        }
        return 0;
    }
    return 1;
}

uint8_t AHT20_Read(float *temp, float *humi) {
    uint8_t buf[6];
    MyI2C_Start();
    if (MyI2C_SendByte(AHT20_ADDR << 1 | 0)) return 0;
    MyI2C_SendByte(0xAC); MyI2C_SendByte(0x33); MyI2C_SendByte(0x00);
    MyI2C_Stop();
    MyI2C_DelayMs(80);

    MyI2C_Start();
    if (MyI2C_SendByte(AHT20_ADDR << 1 | 1)) return 0;
    for (int i = 0; i < 5; i++) buf[i] = MyI2C_ReadByte(1);
    buf[5] = MyI2C_ReadByte(0);
    MyI2C_Stop();

    if (buf[0] & 0x80) return 0;

    uint32_t raw = ((uint32_t)buf[1] << 12) | ((uint32_t)buf[2] << 4) | (buf[3] >> 4);
    *humi = raw * 100.0f / 1048576.0f;
    raw = (((uint32_t)buf[3] & 0x0F) << 16) | ((uint32_t)buf[4] << 8) | buf[5];
    *temp = raw * 200.0f / 1048576.0f - 50.0f;
    return 1;
}

/* ----- BMP280 ----- */
static uint8_t bmp_addr = 0x76;

typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;
} bmp280_calib_t;

static bmp280_calib_t bmp_cal;
static int32_t t_fine; 

static uint8_t BMP280_Probe(void) {
    uint8_t id;
    bmp_addr = 0x76;
    MyI2C_Start();
    if (MyI2C_SendByte(bmp_addr << 1 | 0)) goto try77;
    MyI2C_SendByte(0xD0);
    MyI2C_Start();
    if (MyI2C_SendByte(bmp_addr << 1 | 1)) goto try77;
    id = MyI2C_ReadByte(0); MyI2C_Stop();
    if (id == 0x58) return 1;
try77:
    bmp_addr = 0x77;
    MyI2C_Start();
    if (MyI2C_SendByte(bmp_addr << 1 | 0)) return 0;
    MyI2C_SendByte(0xD0);
    MyI2C_Start();
    if (MyI2C_SendByte(bmp_addr << 1 | 1)) return 0;
    id = MyI2C_ReadByte(0); MyI2C_Stop();
    return (id == 0x58);
}

static uint8_t BMP280_ReadCalib(bmp280_calib_t *cal) {
    uint8_t buf[24];
    MyI2C_Start();
    if (MyI2C_SendByte(bmp_addr << 1 | 0)) { MyI2C_Stop(); return 0; }
    MyI2C_SendByte(0x88);
    MyI2C_Start();
    if (MyI2C_SendByte(bmp_addr << 1 | 1)) { MyI2C_Stop(); return 0; }
    for (int i = 0; i < 23; i++) buf[i] = MyI2C_ReadByte(1);
    buf[23] = MyI2C_ReadByte(0);
    MyI2C_Stop();

    cal->dig_T1 = (uint16_t)(buf[0] | (buf[1] << 8));
    cal->dig_T2 = (int16_t)(buf[2] | (buf[3] << 8));
    cal->dig_T3 = (int16_t)(buf[4] | (buf[5] << 8));
    cal->dig_P1 = (uint16_t)(buf[6] | (buf[7] << 8));
    cal->dig_P2 = (int16_t)(buf[8] | (buf[9] << 8));
    cal->dig_P3 = (int16_t)(buf[10] | (buf[11] << 8));
    cal->dig_P4 = (int16_t)(buf[12] | (buf[13] << 8));
    cal->dig_P5 = (int16_t)(buf[14] | (buf[15] << 8));
    cal->dig_P6 = (int16_t)(buf[16] | (buf[17] << 8));
    cal->dig_P7 = (int16_t)(buf[18] | (buf[19] << 8));
    cal->dig_P8 = (int16_t)(buf[20] | (buf[21] << 8));
    cal->dig_P9 = (int16_t)(buf[22] | (buf[23] << 8));

    // 基本校验：典型值 36000 左右，防御最底层的硬件故障
    if (cal->dig_P1 < 1000 || cal->dig_P1 > 60000) return 0;
    return 1;
}

uint8_t BMP280_Init(void) {
    if (!BMP280_Probe()) return 0;

    bmp280_calib_t cal1, cal2;
    uint8_t success = 0;
    for (uint8_t i = 0; i < 5; i++) {
        if (!BMP280_ReadCalib(&cal1)) continue;
        MyI2C_DelayMs(1);
        if (!BMP280_ReadCalib(&cal2)) continue;
        if (memcmp(&cal1, &cal2, sizeof(bmp280_calib_t)) == 0) {
            bmp_cal = cal1;
            success = 1;
            break;
        }
        MyI2C_DelayMs(5);
    }
    if (!success) return 0;

    MyI2C_Start();
    MyI2C_SendByte(bmp_addr << 1 | 0);
    MyI2C_SendByte(0xF4);
    MyI2C_SendByte(0x27);
    MyI2C_Stop();

    MyI2C_Start();
    MyI2C_SendByte(bmp_addr << 1 | 0);
    MyI2C_SendByte(0xF5);
    MyI2C_SendByte(0xA4);
    MyI2C_Stop();

    MyI2C_DelayMs(20);
    return 1;
}

static int32_t BMP280_Compensate_T(int32_t adc_T) {
    int32_t var1, var2;
    var1 = ((((adc_T >> 3) - ((int32_t)bmp_cal.dig_T1 << 1))) * ((int32_t)bmp_cal.dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)bmp_cal.dig_T1)) * ((adc_T >> 4) - ((int32_t)bmp_cal.dig_T1))) >> 12) * ((int32_t)bmp_cal.dig_T3)) >> 14;
    t_fine = var1 + var2;
    return (t_fine * 5 + 128) >> 8; 
}

static uint32_t BMP280_Compensate_P(int32_t adc_P) {
    int64_t var1, var2, p;
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)bmp_cal.dig_P6;
    var2 = var2 + ((var1 * (int64_t)bmp_cal.dig_P5) << 17);
    var2 = var2 + (((int64_t)bmp_cal.dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)bmp_cal.dig_P3) >> 8) + ((var1 * (int64_t)bmp_cal.dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)bmp_cal.dig_P1) >> 33;
    if (var1 == 0) return 0;
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)bmp_cal.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)bmp_cal.dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)bmp_cal.dig_P7) << 4);
    return (uint32_t)p;
}

uint8_t BMP280_Read(float *pressure_hPa, float *temperature) {
    uint8_t data[6];
    int32_t adc_T, adc_P;

    MyI2C_Start();
    if (MyI2C_SendByte(bmp_addr << 1 | 0)) return 0;
    MyI2C_SendByte(0xF7);
    MyI2C_Start();
    if (MyI2C_SendByte(bmp_addr << 1 | 1)) return 0;
    for (int i = 0; i < 5; i++) data[i] = MyI2C_ReadByte(1);
    data[5] = MyI2C_ReadByte(0);
    MyI2C_Stop();

    adc_P = ((int32_t)data[0] << 12) | ((int32_t)data[1] << 4) | (data[2] >> 4);
    adc_T = ((int32_t)data[3] << 12) | ((int32_t)data[4] << 4) | (data[5] >> 4);

    *temperature = BMP280_Compensate_T(adc_T) / 100.0f;
    uint32_t p = BMP280_Compensate_P(adc_P);
    if (p == 0) return 0;
    *pressure_hPa = (float)p / 25600.0f; 
    return 1;
}
