#include "stm32f10x.h"                  // Device header

#define SCL_PIN    GPIO_Pin_6
#define SDA_PIN    GPIO_Pin_8
#define I2C_PORT   GPIOB

#define SCL_H()    GPIO_SetBits(I2C_PORT, SCL_PIN)
#define SCL_L()    GPIO_ResetBits(I2C_PORT, SCL_PIN)
#define SDA_H()    GPIO_SetBits(I2C_PORT, SDA_PIN)
#define SDA_L()    GPIO_ResetBits(I2C_PORT, SDA_PIN)
#define SDA_READ() GPIO_ReadInputDataBit(I2C_PORT, SDA_PIN)

static void delay_us(uint32_t us) {
    uint32_t i = us * 36; 
    while (i--) {
        __NOP();
    }
}

void MyI2C_DelayMs(uint32_t ms) {
    while (ms--) {
        delay_us(1000);
    }
}

void MyI2C_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitStructure.GPIO_Pin = SCL_PIN | SDA_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(I2C_PORT, &GPIO_InitStructure);
    SCL_H();
    SDA_H();
    MyI2C_DelayMs(1);
}

void MyI2C_Start(void) {
    SDA_H();
    SCL_H();
    delay_us(5);
    SDA_L();
    delay_us(5);
    SCL_L();
}

void MyI2C_Stop(void) {
    SDA_L();
    SCL_H();
    delay_us(5);
    SDA_H();
    delay_us(5);
}

uint8_t MyI2C_SendByte(uint8_t data) {
    for (uint8_t i = 0; i < 8; i++) {
        if (data & 0x80) SDA_H();
        else SDA_L();
        data <<= 1;
        delay_us(2);
        SCL_H();
        delay_us(2);
        SCL_L();
    }
    SDA_H();
    SCL_H();
    delay_us(2);
    uint8_t ack = SDA_READ();
    SCL_L();
    return ack;
}

uint8_t MyI2C_ReadByte(uint8_t ack) {
    uint8_t value = 0;
    SDA_H();
    for (uint8_t i = 0; i < 8; i++) {
        value <<= 1;
        SCL_H();
        delay_us(2);
        if (SDA_READ()) value |= 1;
        SCL_L();
        delay_us(2);
    }
    if (ack) SDA_L();
    else SDA_H();
    SCL_H();
    delay_us(2);
    SCL_L();
    SDA_H();
    return value;
}
