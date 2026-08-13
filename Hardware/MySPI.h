#ifndef __MYSPI_H
#define __MYSPI_H

void MySPI_Init(void);              /* 初始化 SPI1 及 CS 引脚 */
uint8_t MySPI_SendByte(uint8_t data);/* 发送并接收一个字节 */
void MySPI_CS_Low(void);            /* 片选拉低 */
void MySPI_CS_High(void);           /* 片选拉高 */

#endif
