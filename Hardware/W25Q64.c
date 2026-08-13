#include "stm32f10x.h"                  // Device header
#include "BPK.h" 
#include "MySPI.h"
#include "W25Q64_Cmd.h" 
#include "Weather_Common.h"
#include <string.h>

/* 存储布局定义 */
#define W25Q64_PAGE_SIZE          256
#define W25Q64_SECTOR_SIZE        4096
#define W25Q64_BLOCK_SIZE         65536
#define W25Q64_RECORD_SIZE        16
#define W25Q64_RECORDS_PER_BLOCK  (W25Q64_BLOCK_SIZE / W25Q64_RECORD_SIZE)  // 4096

/* 数据块：0 ~ 127 共 128 块，全部用于存储 */
#define W25Q64_DATA_BLOCKS        128
#define W25Q64_MAX_RECORDS        (W25Q64_DATA_BLOCKS * W25Q64_RECORDS_PER_BLOCK)  // 524288

/* 备份寄存器分配 */
#define BKP_MAGIC         BKP_DR1   /* 有效标志 0xA5A5 */
#define BKP_NEXT_BLOCK    BKP_DR2   /* 下一个写入块索引 (0~127) */
#define BKP_NEXT_REC      BKP_DR3   /* 下一个块内记录索引 (0~4095) */
#define BKP_FIRST_BLOCK   BKP_DR4   /* 最早记录块索引 */
#define BKP_FIRST_REC     BKP_DR5   /* 最早记录块内索引 */
#define BKP_VALID_LOW     BKP_DR6   /* 有效记录数低16位 */
#define BKP_VALID_HIGH    BKP_DR7   /* 有效记录数高16位 */

#define MGMT_MAGIC  0xA5A5

void W25Q64_EraseBlock(uint32_t block_index);

/* 模块内部静态变量：记录当前索引和有效数量 */
static uint32_t next_block, next_rec;
static uint32_t first_block, first_rec;
static uint32_t valid_count;

/* 内部函数：备份寄存器读写 */
static void LoadIndex(void)
{
    next_block  = BKP_Read(BKP_NEXT_BLOCK);
    next_rec    = BKP_Read(BKP_NEXT_REC);
    first_block = BKP_Read(BKP_FIRST_BLOCK);
    first_rec   = BKP_Read(BKP_FIRST_REC);
    uint32_t low  = BKP_Read(BKP_VALID_LOW);
    uint32_t high = BKP_Read(BKP_VALID_HIGH);
    valid_count = (high << 16) | low;
}

static void SaveIndex(void)
{
    BKP_Write(BKP_NEXT_BLOCK, (uint16_t)next_block);
    BKP_Write(BKP_NEXT_REC,   (uint16_t)next_rec);
    BKP_Write(BKP_FIRST_BLOCK,(uint16_t)first_block);
    BKP_Write(BKP_FIRST_REC,  (uint16_t)first_rec);
    BKP_Write(BKP_VALID_LOW,  (uint16_t)(valid_count & 0xFFFF));
    BKP_Write(BKP_VALID_HIGH, (uint16_t)(valid_count >> 16));
}

/* 内部函数：发送写使能命令 */
static void W25Q64_WriteEnable(void)
{
    MySPI_CS_Low();
    MySPI_SendByte(W25Q64_CMD_WRITE_ENABLE);
    MySPI_CS_High();
}

/* 内部函数：等待芯片忙结束 */
static void W25Q64_WaitBusy(void)
{
    MySPI_CS_Low();
    MySPI_SendByte(W25Q64_CMD_READ_STATUS1);
    while (MySPI_SendByte(0xFF) & 0x01);
    MySPI_CS_High();
}

/**
 * @brief  初始化 W25Q64 及存储索引
 * @note   首次上电时若备份寄存器无效，则格式化第 0 块并初始化索引
 */
void W25Q64_Init(void)
{
    MySPI_Init();
    BKP_Init();   /* 若使用备份寄存器，需先初始化 */

    if (BKP_Read(BKP_MAGIC) != MGMT_MAGIC) {
        /* 首次运行：索引清零，格式化第 0 块，写入魔术数 */
        next_block  = 0;
        next_rec    = 0;
        first_block = 0;
        first_rec   = 0;
        valid_count = 0;
        SaveIndex();
        BKP_Write(BKP_MAGIC, MGMT_MAGIC);
        W25Q64_EraseBlock(0);
    } else {
        /* 非首次运行：从备份寄存器恢复索引 */
        LoadIndex();
    }
}

/**
 * @brief  读取 JEDEC 制造商 ID
 * @retval 24 位 ID
 */
uint32_t W25Q64_ReadID(void)
{
    uint32_t id = 0;
    MySPI_CS_Low();
    MySPI_SendByte(W25Q64_CMD_READ_JEDEC_ID);
    id |= MySPI_SendByte(0xFF) << 16;
    id |= MySPI_SendByte(0xFF) << 8;
    id |= MySPI_SendByte(0xFF);
    MySPI_CS_High();
    return id;
}

/**
 * @brief  擦除一个 64KB 块
 * @param  block_index: 块索引（0~127）
 */
void W25Q64_EraseBlock(uint32_t block_index)
{
    uint32_t addr = block_index * W25Q64_BLOCK_SIZE;

    W25Q64_WriteEnable();
    MySPI_CS_Low();
    MySPI_SendByte(W25Q64_CMD_BLOCK_ERASE_64K);
    MySPI_SendByte((addr >> 16) & 0xFF);
    MySPI_SendByte((addr >> 8) & 0xFF);
    MySPI_SendByte(addr & 0xFF);
    MySPI_CS_High();
    W25Q64_WaitBusy();
}

/**
 * @brief  写入一条记录到环形缓冲区
 * @param  rec: 指向记录数据的指针（必须为 16 字节）
 */
void W25Q64_WriteRecord(LogRecord *rec)
{
    uint32_t addr = next_block * W25Q64_BLOCK_SIZE + next_rec * W25Q64_RECORD_SIZE;

    /* 页编程写入 16 字节记录 */
    W25Q64_WriteEnable();
    MySPI_CS_Low();
    MySPI_SendByte(W25Q64_CMD_PAGE_PROGRAM);
    MySPI_SendByte((addr >> 16) & 0xFF);
    MySPI_SendByte((addr >> 8) & 0xFF);
    MySPI_SendByte(addr & 0xFF);
    for (uint8_t i = 0; i < W25Q64_RECORD_SIZE; i++) {
        MySPI_SendByte(((uint8_t*)rec)[i]);
    }
    MySPI_CS_High();
    W25Q64_WaitBusy();

    /* 更新下一次写入位置 */
    next_rec++;
    if (next_rec >= W25Q64_RECORDS_PER_BLOCK) {
        next_block = (next_block + 1) % W25Q64_DATA_BLOCKS;
        next_rec = 0;
        /* 擦除即将被覆盖的块 */
        W25Q64_EraseBlock(next_block);
    }

    /* 更新有效记录数及最早记录位置 */
    if (valid_count < W25Q64_MAX_RECORDS) {
        valid_count++;
    } else {
        first_rec++;
        if (first_rec >= W25Q64_RECORDS_PER_BLOCK) {
            first_block = (first_block + 1) % W25Q64_DATA_BLOCKS;
            first_rec = 0;
        }
    }

    /* 保存索引到备份寄存器 */
    SaveIndex();
}

/**
 * @brief  按索引读取一条记录（索引 0 为最早记录）
 * @param  index: 记录索引（0 ~ valid_count-1）
 * @param  rec:   输出记录数据缓冲区
 * @retval 1 成功，0 失败（索引越界）
 */
uint8_t W25Q64_ReadRecord(uint32_t index, LogRecord *rec)
{
    if (index >= valid_count) return 0;

    /* 根据环形缓冲区计算物理地址 */
    uint32_t total_offset = first_rec + index;
    uint32_t target_block = (first_block + total_offset / W25Q64_RECORDS_PER_BLOCK) % W25Q64_DATA_BLOCKS;
    uint32_t target_rec   = total_offset % W25Q64_RECORDS_PER_BLOCK;
    uint32_t addr = target_block * W25Q64_BLOCK_SIZE + target_rec * W25Q64_RECORD_SIZE;

    MySPI_CS_Low();
    MySPI_SendByte(W25Q64_CMD_READ_DATA);
    MySPI_SendByte((addr >> 16) & 0xFF);
    MySPI_SendByte((addr >> 8) & 0xFF);
    MySPI_SendByte(addr & 0xFF);
    for (uint8_t i = 0; i < W25Q64_RECORD_SIZE; i++) {
        ((uint8_t*)rec)[i] = MySPI_SendByte(0xFF);
    }
    MySPI_CS_High();
    return 1;
}

/**
 * @brief  获取当前有效记录数
 */
uint32_t W25Q64_GetRecordCount(void)
{
    return valid_count;
}

/**
 * @brief  获取剩余可写入记录数
 */
uint32_t W25Q64_GetRemainingCapacity(void)
{
    if (valid_count >= W25Q64_MAX_RECORDS) return 0;
    return W25Q64_MAX_RECORDS - valid_count;
}

/**
 * @brief  全片擦除（慎用）
 */
void W25Q64_ChipErase(void)
{
    W25Q64_WriteEnable();
    MySPI_CS_Low();
    MySPI_SendByte(W25Q64_CMD_CHIP_ERASE); // 0xC7
    MySPI_CS_High();
    W25Q64_WaitBusy();
}
