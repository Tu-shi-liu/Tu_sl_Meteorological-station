#ifndef __BKP_H
#define __BKP_H

void BKP_Init(void);
void BKP_Write(uint8_t reg, uint16_t data);
uint16_t BKP_Read(uint8_t reg);

#endif
