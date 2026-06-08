#ifndef __SOFT_I2C1_H
#define __SOFT_I2C1_H

#include "sys.h"

/* 基础接口 */
void SoftI2C1_Init(void);
void SoftI2C1_Start(void);
void SoftI2C1_Stop(void);
void SoftI2C1_SendByte(uint8_t byte);
uint8_t SoftI2C1_ReadByte(uint8_t ack);
uint8_t SoftI2C1_WaitAck(void);
void SoftI2C1_Ack(void);
void SoftI2C1_NAck(void);

uint8_t SoftI2C1_ReadRegs(uint8_t dev_addr,
                          uint8_t reg_addr,
                          uint8_t *buf,
                          uint8_t len);

/* 寄存器读写接口
 * dev_addr: 7位设备地址
 * reg_addr: 寄存器地址
 */
uint8_t SoftI2C1_WriteReg(uint8_t dev_addr, uint8_t reg_addr, uint8_t data);
uint8_t SoftI2C1_ReadReg(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data);

#endif

