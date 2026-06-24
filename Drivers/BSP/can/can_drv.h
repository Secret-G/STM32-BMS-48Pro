#ifndef __CAN_DRV_H
#define __CAN_DRV_H

#include "sys.h"
#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"

/*
 * BMS CAN 标准帧 ID
 *
 * 0x401: PC  -> BMS 命令请求
 * 0x307: BMS -> PC  命令 ACK
 *
 * 0x301~0x306: BMS 状态上报
 */
#define CAN_ID_BMS_CMD_REQ        0x401U
#define CAN_ID_BMS_CMD_ACK        0x307U

#define CAN_ID_BMS_PACK_STATUS    0x301U
#define CAN_ID_BMS_CELL_1_4       0x302U
#define CAN_ID_BMS_CELL_5_8       0x303U
#define CAN_ID_BMS_CELL_9_STATUS  0x304U
#define CAN_ID_BMS_FAULT_STATUS   0x305U
#define CAN_ID_BMS_BALANCE_STATUS 0x306U


/*
 * CAN RX 队列长度
 */
#define CAN_DRV_RX_QUEUE_LEN      8U



/*
 * CAN 标准接收帧
 */
typedef struct
{
    uint16_t std_id;
    uint8_t  dlc;
    uint8_t  data[8];
} CAN_DrvRxFrame_t;

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
 */
uint8_t CAN_DrvSendStd(uint16_t std_id, const uint8_t *data, uint8_t dlc);

/*
 * 注册 CAN RX 队列
 * 队列由上层任务创建，驱动层只负责往队列里投递接收帧
 */
void CAN_DrvSetRxQueue(QueueHandle_t queue);

/*
 * CAN RX0 中断入口封装
 *
 * 如果 stm32f1xx_it.c 里已经有 USB_LP_CAN1_RX0_IRQHandler，
 * 就在那个中断函数里调用 CAN_DrvRxIrqHandler()。
 */
void CAN_DrvRxIrqHandler(void);


/*
 * 获取 CAN RX 队列满导致的丢帧计数
 */
uint16_t CAN_DrvGetRxDropCount(void);


#endif

