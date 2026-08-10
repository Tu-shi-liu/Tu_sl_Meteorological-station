#ifndef __W25Q64_H
#define __W25Q64_H

void W25Q64_Init(void);
uint32_t W25Q64_ReadID(void);
void W25Q64_WriteRecord(LogRecord *rec);
uint8_t W25Q64_ReadRecord(uint32_t index, LogRecord *rec);
uint32_t W25Q64_GetRecordCount(void);
uint32_t W25Q64_GetRemainingCapacity(void);
void W25Q64_ChipErase(void);

#endif
