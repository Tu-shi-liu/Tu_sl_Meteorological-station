#ifndef __MYI2C_H
#define __MYI2C_H

void MyI2C_Init(void);
void MyI2C_Start(void);
void MyI2C_Stop(void);
uint8_t MyI2C_SendByte(uint8_t data);
uint8_t MyI2C_ReadByte(uint8_t ack);
void MyI2C_DelayMs(uint32_t ms);

#endif
