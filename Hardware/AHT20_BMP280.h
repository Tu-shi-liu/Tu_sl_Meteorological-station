#ifndef __SOFT_I2C_SENSOR_H
#define __SOFT_I2C_SENSOR_H

uint8_t AHT20_Init(void);
uint8_t AHT20_Read(float *temperature, float *humidity);
uint8_t BMP280_Init(void);
uint8_t BMP280_Read(float *pressure_hPa, float *temperature);

#endif
