#ifndef __BQ76940_APP_CAN_H
#define __BQ76940_APP_CAN_H

#include <stdint.h>
#include "can_drv.h"

#define BMS_CAN_FAULT_TYPE_NONE 0U
#define BMS_CAN_FAULT_TYPE_BRINGUP 1U
#define BMS_CAN_FAULT_TYPE_RUNTIME 2U
#define BMS_CAN_FAULT_TYPE_HW_FAULT 3U
#define BMS_CAN_FAULT_TYPE_RTOS_INIT 4U

#define BMS_CAN_HW_FLAG_OCD_ACTIVE 0x01U
#define BMS_CAN_HW_FLAG_SCD_ACTIVE 0x02U
#define BMS_CAN_HW_FLAG_DSG_BLOCK 0x04U

#define BMS_CAN_CMD_REQ_ALL_STATUS 0x01U
#define BMS_CAN_CMD_REQ_FAULT_DIAG 0x02U
#define BMS_CAN_CMD_REQ_BALANCE_STATUS 0x03U

#define BMS_CAN_ACK_OK 0x00U
#define BMS_CAN_ACK_UNKNOWN_CMD 0x01U
#define BMS_CAN_ACK_INVALID_DLC 0x02U
#define BMS_CAN_ACK_REJECTED 0x03U
#define BMS_CAN_ACK_EXEC_FAIL 0x04U

struct BQ76940_AppCtx;

void BQ76940_AppSendCanTelemetry(const struct BQ76940_AppCtx *ctx);

/*
 * 0x305 统一异常诊断帧：
 * - Runtime fault
 * - HwFault
 * 周期任务中调用。
 */
void BQ76940_AppSendFaultDiagCan(const struct BQ76940_AppCtx *ctx);

/*
 * 0x305 启动异常诊断帧：
 * main bring-up 失败时调用。
 */
void BQ76940_AppSendBringUpFaultCan(const struct BQ76940_AppCtx *ctx,
                                    uint8_t main_ret,
                                    uint8_t safe_off_result);

void BQ76940_AppSendRtosInitFaultCan(const struct BQ76940_AppCtx *ctx,
                                     uint8_t err_code,
                                     uint8_t safe_off_result);

void BQ76940_AppSendBalanceStatusCan(const struct BQ76940_AppCtx *ctx);

uint8_t BQ76940_AppHandleCanCommand(const struct BQ76940_AppCtx *ctx,
                                    const CAN_DrvRxFrame_t *rx);

#endif
