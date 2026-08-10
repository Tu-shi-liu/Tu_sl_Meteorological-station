#include "stm32f10x.h"                  // Device header

void BKP_Init(void) {
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
}

void BKP_Write(uint8_t reg, uint16_t data) {
    BKP_WriteBackupRegister(reg, data);
}

uint16_t BKP_Read(uint8_t reg) {
    return BKP_ReadBackupRegister(reg);
}
