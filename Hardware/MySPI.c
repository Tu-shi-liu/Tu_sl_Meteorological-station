#include "stm32f10x.h"

/* 片选引脚定义 */
#define CS_GPIO_PORT   GPIOA
#define CS_GPIO_PIN    GPIO_Pin_4

void MySPI_CS_High(void);
void MySPI_CS_Low(void);

/**
 * @brief  初始化 SPI1 外设及片选引脚
 * @note   SPI1 配置：全双工主机，8 位数据，模式 0，软件 NSS，预分频 4，MSB 先行
 */
void MySPI_Init(void)
{
    /* 使能 GPIOA 和 SPI1 时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);

    /* 配置 SPI1 通信引脚（PA5=SCK, PA6=MISO, PA7=MOSI）为复用推挽输出 */
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* 配置片选引脚（PA4）为通用推挽输出，初始为高电平（不选中） */
    GPIO_InitStructure.GPIO_Pin = CS_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    MySPI_CS_High();

    /* 配置 SPI1 参数 */
    SPI_InitTypeDef SPI_InitStructure;
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;          /* 时钟空闲低电平 */
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;        /* 第一个时钟沿采样 */
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;           /* 软件控制片选 */
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4; /* 18MHz (72/4) */
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI1, &SPI_InitStructure);

    /* 使能 SPI1 */
    SPI_Cmd(SPI1, ENABLE);
}

/**
 * @brief  SPI1 发送并接收一个字节
 * @param  data: 要发送的字节
 * @retval 接收到的字节
 */
uint8_t MySPI_SendByte(uint8_t data)
{
    /* 等待发送缓冲区空 */
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(SPI1, data);

    /* 等待接收缓冲区非空 */
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
    return SPI_I2S_ReceiveData(SPI1);
}

/**
 * @brief  片选拉低（开始通信）
 */
void MySPI_CS_Low(void)
{
    GPIO_ResetBits(CS_GPIO_PORT, CS_GPIO_PIN);
}

/**
 * @brief  片选拉高（结束通信）
 */
void MySPI_CS_High(void)
{
    GPIO_SetBits(CS_GPIO_PORT, CS_GPIO_PIN);
}
