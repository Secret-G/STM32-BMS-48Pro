#ifndef __CAN_DRV_H
#define __CAN_DRV_H

#include "sys.h"
#include <stdint.h>

/*
 * BMS CAN 标准帧 ID
 * 第一版只做状态上报
 */
#define CAN_ID_BMS_PACK_STATUS    0x301U
#define CAN_ID_BMS_CELL_1_4       0x302U
#define CAN_ID_BMS_CELL_5_8       0x303U
#define CAN_ID_BMS_CELL_9_STATUS  0x304U

/*
 * 新增故障帧
 */
#define CAN_ID_BMS_FAULT_STATUS    0x305U

/*
 * CAN 驱动初始化
 * 返回 0 表示成功，非 0 表示失败
 */
uint8_t CAN_DrvInit(void);

/*
 * CAN 驱动是否已经初始化成功
 */
uint8_t CAN_DrvIsReady(void);

/*
 * 发送标准帧
 * std_id: 11 位标准帧 ID，范围 0x000 ~ 0x7FF
 * data: 数据区
 * dlc: 数据长度，0~8
 */
uint8_t CAN_DrvSendStd(uint16_t std_id, const uint8_t *data, uint8_t dlc);

#endif

