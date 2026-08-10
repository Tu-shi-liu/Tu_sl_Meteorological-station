#include "stm32f10x.h"                  // Device header
#include "BPK.h"
#include "W25Q64_Cmd.h"
#include "Weather_Common.h"
#include <string.h>

#define W25Q64_PAGE_SIZE          256
#define W25Q64_SECTOR_SIZE        4096
#define W25Q64_BLOCK_SIZE         65536
#define W25Q64_RECORD_SIZE        16
#define W25Q64_RECORDS_PER_BLOCK  (W25Q64_BLOCK_SIZE / W25Q64_RECORD_SIZE)  // 4096

/* 数据块：0 ~ 127 共 128 块，全部用于存储 */
#define W25Q64_DATA_BLOCKS        128
#define W25Q64_MAX_RECORDS        (W25Q64_DATA_BLOCKS * W25Q64_RECORDS_PER_BLOCK)  // 128*4096

/* 备份寄存器分配 */
#define BKP_MAGIC         BKP_DR1   /* 有效标志 0xA5A5 */
#define BKP_NEXT_BLOCK    BKP_DR2   /* 下一个写入块索引 (0~127) */
#define BKP_NEXT_REC      BKP_DR3   /* 下一个块内记录索引 (0~4095) */
#define BKP_FIRST_BLOCK   BKP_DR4   /* 最早记录块索引 */
#define BKP_FIRST_REC     BKP_DR5   /* 最早记录块内索引 */
#define BKP_VALID_LOW     BKP_DR6   /* 有效记录数低16位 */
#define BKP_VALID_HIGH    BKP_DR7   /* 有效记录数高16位 */

#define MGMT_MAGIC  0xA5A5

#define CS_HIGH()   GPIO_SetBits(GPIOA, GPIO_Pin_4)
#define CS_LOW()    GPIO_ResetBits(GPIOA, GPIO_Pin_4)

static uint32_t next_block, next_rec;
static uint32_t first_block, first_rec;
static uint32_t valid_count;

static void LoadIndex(void) {
    next_block  = BKP_Read(BKP_NEXT_BLOCK);
    next_rec    = BKP_Read(BKP_NEXT_REC);
    first_block = BKP_Read(BKP_FIRST_BLOCK);
    first_rec   = BKP_Read(BKP_FIRST_REC);
    uint32_t low  = BKP_Read(BKP_VALID_LOW);
    uint32_t high = BKP_Read(BKP_VALID_HIGH);
    valid_count = (high << 16) | low;
}

static void SaveIndex(void) {
    BKP_Write(BKP_NEXT_BLOCK, (uint16_t)next_block);
    BKP_Write(BKP_NEXT_REC,   (uint16_t)next_rec);
    BKP_Write(BKP_FIRST_BLOCK,(uint16_t)first_block);
    BKP_Write(BKP_FIRST_REC,  (uint16_t)first_rec);
    BKP_Write(BKP_VALID_LOW,  (uint16_t)(valid_count & 0xFFFF));
    BKP_Write(BKP_VALID_HIGH, (uint16_t)(valid_count >> 16));
}

static void SPI1_Init(void) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    CS_HIGH();

    SPI_InitTypeDef SPI_InitStructure;
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI1, &SPI_InitStructure);
    SPI_Cmd(SPI1, ENABLE);
}

static uint8_t SPI1_SendByte(uint8_t data) {
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(SPI1, data);
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
    return SPI_I2S_ReceiveData(SPI1);
}

static void W25Q64_WriteEnable(void) {
    CS_LOW();
    SPI1_SendByte(W25Q64_CMD_WRITE_ENABLE);
    CS_HIGH();
}

static void W25Q64_WaitBusy(void) {
    CS_LOW();
    SPI1_SendByte(W25Q64_CMD_READ_STATUS1);
    while (SPI1_SendByte(0xFF) & 0x01);
    CS_HIGH();
}

void W25Q64_EraseBlock(uint32_t block_index) {
    uint32_t addr = block_index * W25Q64_BLOCK_SIZE;
    W25Q64_WriteEnable();
    CS_LOW();
    SPI1_SendByte(W25Q64_CMD_BLOCK_ERASE_64K);
    SPI1_SendByte((addr >> 16) & 0xFF);
    SPI1_SendByte((addr >> 8) & 0xFF);
    SPI1_SendByte(addr & 0xFF);
    CS_HIGH();
    W25Q64_WaitBusy();
}

void W25Q64_Init(void) {
    SPI1_Init();
    BKP_Init();

    if (BKP_Read(BKP_MAGIC) != MGMT_MAGIC) {
        next_block  = 0;
        next_rec    = 0;
        first_block = 0;
        first_rec   = 0;
        valid_count = 0;
        SaveIndex();
        BKP_Write(BKP_MAGIC, MGMT_MAGIC);
        W25Q64_EraseBlock(0);
    } else {
        LoadIndex();
    }
}

uint32_t W25Q64_ReadID(void) {
    uint32_t id = 0;
    CS_LOW();
    SPI1_SendByte(W25Q64_CMD_READ_JEDEC_ID);
    id |= SPI1_SendByte(0xFF) << 16;
    id |= SPI1_SendByte(0xFF) << 8;
    id |= SPI1_SendByte(0xFF);
    CS_HIGH();
    return id;
}

void W25Q64_WriteRecord(LogRecord *rec) {
    uint32_t addr = next_block * W25Q64_BLOCK_SIZE + next_rec * W25Q64_RECORD_SIZE;

    W25Q64_WriteEnable();
    CS_LOW();
    SPI1_SendByte(W25Q64_CMD_PAGE_PROGRAM);
    SPI1_SendByte((addr >> 16) & 0xFF);
    SPI1_SendByte((addr >> 8) & 0xFF);
    SPI1_SendByte(addr & 0xFF);
    for (uint8_t i = 0; i < W25Q64_RECORD_SIZE; i++) {
        SPI1_SendByte(((uint8_t*)rec)[i]);
    }
    CS_HIGH();
    W25Q64_WaitBusy();

    next_rec++;
    if (next_rec >= W25Q64_RECORDS_PER_BLOCK) {
        next_block = (next_block + 1) % W25Q64_DATA_BLOCKS;
        next_rec = 0;
        W25Q64_EraseBlock(next_block);
    }

    if (valid_count < W25Q64_MAX_RECORDS) {
        valid_count++;
    } else {
        first_rec++;
        if (first_rec >= W25Q64_RECORDS_PER_BLOCK) {
            first_block = (first_block + 1) % W25Q64_DATA_BLOCKS;
            first_rec = 0;
        }
    }

    SaveIndex(); 
}

uint8_t W25Q64_ReadRecord(uint32_t index, LogRecord *rec) {
    if (index >= valid_count) return 0;

    uint32_t total_offset = first_rec + index;
    uint32_t target_block = (first_block + total_offset / W25Q64_RECORDS_PER_BLOCK) % W25Q64_DATA_BLOCKS;
    uint32_t target_rec   = total_offset % W25Q64_RECORDS_PER_BLOCK;
    uint32_t addr = target_block * W25Q64_BLOCK_SIZE + target_rec * W25Q64_RECORD_SIZE;

    CS_LOW();
    SPI1_SendByte(W25Q64_CMD_READ_DATA);
    SPI1_SendByte((addr >> 16) & 0xFF);
    SPI1_SendByte((addr >> 8) & 0xFF);
    SPI1_SendByte(addr & 0xFF);
    for (uint8_t i = 0; i < W25Q64_RECORD_SIZE; i++) {
        ((uint8_t*)rec)[i] = SPI1_SendByte(0xFF);
    }
    CS_HIGH();
    return 1;
}

uint32_t W25Q64_GetRecordCount(void) {
    return valid_count;
}

uint32_t W25Q64_GetRemainingCapacity(void) {
    if (valid_count >= W25Q64_MAX_RECORDS) return 0;
    return W25Q64_MAX_RECORDS - valid_count;
}

void W25Q64_ChipErase(void) {
    W25Q64_WriteEnable();          
    CS_LOW();
    SPI1_SendByte(W25Q64_CMD_CHIP_ERASE); // 0xC7
    CS_HIGH();
    W25Q64_WaitBusy(); 
}
