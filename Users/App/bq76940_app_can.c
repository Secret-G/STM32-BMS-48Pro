#include "bq76940_app.h"
#include "bq76940_app_can.h"

#include "can_drv.h"

static void BQ76940_AppPackU16LE(uint8_t *buf, uint8_t offset, uint16_t value)
{
    buf[offset] = (uint8_t)(value & 0xFFU);
    buf[offset + 1U] = (uint8_t)((value >> 8U) & 0xFFU);
}

static void BQ76940_AppPackU32LE(uint8_t *buf, uint8_t offset, uint32_t value)
{
    buf[offset] = (uint8_t)(value & 0xFFU);
    buf[offset + 1U] = (uint8_t)((value >> 8U) & 0xFFU);
    buf[offset + 2U] = (uint8_t)((value >> 16U) & 0xFFU);
    buf[offset + 3U] = (uint8_t)((value >> 24U) & 0xFFU);
}

static void BQ76940_AppPackI32LE(uint8_t *buf, uint8_t offset, int32_t value)
{
    BQ76940_AppPackU32LE(buf, offset, (uint32_t)value);
}

static uint8_t BQ76940_AppBuildProtectFlags(const BQ76940_AppCtx_t *ctx)
{
    uint8_t flags = 0U;

    if (ctx->ot_cutoff_active != 0U) flags |= 0x01U;
    if (ctx->ut_chg_block_active != 0U) flags |= 0x02U;
    if (ctx->hw_dsg_block_active != 0U) flags |= 0x04U;
    if (ctx->hw_ocd_active != 0U) flags |= 0x08U;
    if (ctx->hw_scd_active != 0U) flags |= 0x10U;
    if (ctx->bal_active != 0U) flags |= 0x20U;

    return flags;
}

static uint8_t BQ76940_AppBuildAlarmFlags(const BQ76940_AppCtx_t *ctx)
{
    uint8_t flags = 0U;

    if (ctx->alarm_state.uv_flag != 0U) flags |= 0x01U;
    if (ctx->alarm_state.ov_flag != 0U) flags |= 0x02U;
    if (ctx->alarm_state.diff_flag != 0U) flags |= 0x04U;
    if (ctx->alarm_state.ot_flag != 0U) flags |= 0x08U;
    if (ctx->alarm_state.ut_flag != 0U) flags |= 0x10U;

    return flags;
}

static uint8_t BQ76940_AppBuildHwFaultCanFlags(const BQ76940_AppCtx_t *ctx)
{
    uint8_t flags = 0U;

    if (ctx == 0)
    {
        return 0U;
    }

    if (ctx->hw_ocd_active != 0U)
    {
        flags |= BMS_CAN_HW_FLAG_OCD_ACTIVE;
    }

    if (ctx->hw_scd_active != 0U)
    {
        flags |= BMS_CAN_HW_FLAG_SCD_ACTIVE;
    }

    if (ctx->hw_dsg_block_active != 0U)
    {
        flags |= BMS_CAN_HW_FLAG_DSG_BLOCK;
    }

    return flags;
}

/*
 * 将 BQ76940 应用层状态打包为 CAN 状态帧并发送。
 *
 * 当前 CAN 协议：
 * 0x301: Pack 总压 + Pack 电流
 * 0x302: Cell1 ~ Cell4 电压
 * 0x303: Cell5 ~ Cell8 电压
 * 0x304: Cell9 电压 + TS1 温度 + 告警/保护/均衡状态
 *
 * 注意：
 * 本函数只负责业务数据打包和调用 CAN_DrvSendStd()，
 * CAN 底层初始化、邮箱发送等细节由 can_drv.c 负责。
 */

void BQ76940_AppSendCanTelemetry(const BQ76940_AppCtx_t *ctx)
{
    uint8_t data[8];
    uint8_t ret;

    if ((ctx == 0) || (CAN_DrvIsReady() == 0U))
    {
        return;
    }

    BQ76940_AppPackU32LE(data, 0, ctx->pack_total_mV);
    BQ76940_AppPackI32LE(data, 4, ctx->pack_current_mA);
    ret = CAN_DrvSendStd(CAN_ID_BMS_PACK_STATUS, data, 8);
    if (ret != 0U) return;

    BQ76940_AppPackU16LE(data, 0, ctx->cell_mV[0]);
    BQ76940_AppPackU16LE(data, 2, ctx->cell_mV[1]);
    BQ76940_AppPackU16LE(data, 4, ctx->cell_mV[2]);
    BQ76940_AppPackU16LE(data, 6, ctx->cell_mV[3]);
    ret = CAN_DrvSendStd(CAN_ID_BMS_CELL_1_4, data, 8);
    if (ret != 0U) return;

    BQ76940_AppPackU16LE(data, 0, ctx->cell_mV[4]);
    BQ76940_AppPackU16LE(data, 2, ctx->cell_mV[5]);
    BQ76940_AppPackU16LE(data, 4, ctx->cell_mV[6]);
    BQ76940_AppPackU16LE(data, 6, ctx->cell_mV[7]);
    ret = CAN_DrvSendStd(CAN_ID_BMS_CELL_5_8, data, 8);
    if (ret != 0U) return;

    BQ76940_AppPackU16LE(data, 0, ctx->cell_mV[8]);
    BQ76940_AppPackU16LE(data, 2, (uint16_t)ctx->ts1_temp_dC);
    data[4] = BQ76940_AppBuildAlarmFlags(ctx);
    data[5] = BQ76940_AppBuildProtectFlags(ctx);
    data[6] = ctx->bal_target_label;
    data[7] = (uint8_t)ctx->pack_current_dir;
    (void)CAN_DrvSendStd(CAN_ID_BMS_CELL_9_STATUS, data, 8);
}

void BQ76940_AppSendBringUpFaultCan(const BQ76940_AppCtx_t *ctx,
                                    uint8_t main_ret,
                                    uint8_t safe_off_result)
{
    uint8_t data[8];
    uint8_t fault_flags = 0U;

    if ((ctx == 0) || (CAN_DrvIsReady() == 0U))
    {
        return;
    }

    if (ctx->diag_state.bringup_fault_active != 0U)
    {
        fault_flags |= 0x01U;
    }

    if (safe_off_result != 0U)
    {
        fault_flags |= 0x02U;
    }

    /*
     * 0x305: BMS Fault Diagnostic
     * Type = 0x01 BringUp Fault
     *
     * Byte0: fault_type = BRINGUP
     * Byte1: main_ret
     * Byte2: bringup_last_stage
     * Byte3: bringup_last_error
     * Byte4: bringup_attempt_count
     * Byte5: safe_off_result
     * Byte6: fault_flags
     * Byte7: reserved
     */
    data[0] = BMS_CAN_FAULT_TYPE_BRINGUP;
    data[1] = main_ret;
    data[2] = ctx->diag_state.bringup_last_stage;
    data[3] = ctx->diag_state.bringup_last_error;
    data[4] = ctx->diag_state.bringup_attempt_count;
    data[5] = safe_off_result;
    data[6] = fault_flags;
    data[7] = 0U;

    (void)CAN_DrvSendStd(CAN_ID_BMS_FAULT_STATUS, data, 8U);
}

void BQ76940_AppSendFaultDiagCan(const BQ76940_AppCtx_t *ctx)
{
    uint8_t data[8];

    if ((ctx == 0) || (CAN_DrvIsReady() == 0U))
    {
        return;
    }

    /*
     * 优先级：
     * Runtime Fault > HwFault > None
     */
    if (ctx->runtime_diag.fault_active != 0U)
    {
        /*
         * 0x305: BMS Fault Diagnostic
         * Type = 0x02 Runtime Fault
         *
         * Byte0: fault_type = RUNTIME
         * Byte1: runtime fault code
         * Byte2: runtime fault stage
         * Byte3: safe_off_result
         * Byte4: runtime fault active
         * Byte5: retry_count
         * Byte6: total_sample_fail_count low
         * Byte7: total_sample_fail_count high
         */
        data[0] = BMS_CAN_FAULT_TYPE_RUNTIME;
        data[1] = ctx->runtime_diag.last_fault_code;
        data[2] = ctx->runtime_diag.last_fault_stage;
        data[3] = ctx->runtime_diag.safe_off_result;
        data[4] = ctx->runtime_diag.fault_active;
        data[5] = ctx->runtime_diag.safe_off_retry_count;
        data[6] = (uint8_t)(ctx->runtime_diag.total_sample_fail_count & 0xFFU);
        data[7] = (uint8_t)((ctx->runtime_diag.total_sample_fail_count >> 8U) & 0xFFU);

        (void)CAN_DrvSendStd(CAN_ID_BMS_FAULT_STATUS, data, 8U);
        return;
    }

    if (ctx->hw_fault_count != 0U)
    {
        /*
         * 0x305: BMS Fault Diagnostic
         * Type = 0x03 HwFault
         *
         * Byte0: fault_type = HW_FAULT
         * Byte1: hw_fault_last_code
         * Byte2: hw_fault_sys_stat_latched
         * Byte3: hw flags
         * Byte4: hw_fault_last_apply_ret
         * Byte5: reserved
         * Byte6: hw_fault_count low
         * Byte7: hw_fault_count high
         */
        data[0] = BMS_CAN_FAULT_TYPE_HW_FAULT;
        data[1] = ctx->hw_fault_last_code;
        data[2] = ctx->hw_fault_sys_stat_latched;
        data[3] = BQ76940_AppBuildHwFaultCanFlags(ctx);
        data[4] = ctx->hw_fault_last_apply_ret;
        data[5] = 0U;
        data[6] = (uint8_t)(ctx->hw_fault_count & 0xFFU);
        data[7] = (uint8_t)((ctx->hw_fault_count >> 8U) & 0xFFU);

        (void)CAN_DrvSendStd(CAN_ID_BMS_FAULT_STATUS, data, 8U);
        return;
    }

    /*
     * 没有异常时也发一帧 0x305 = all zero。
     * 好处：PCAN-View 能确认故障诊断帧在线。
     */
    data[0] = BMS_CAN_FAULT_TYPE_NONE;
    data[1] = 0U;
    data[2] = 0U;
    data[3] = 0U;
    data[4] = 0U;
    data[5] = 0U;
    data[6] = 0U;
    data[7] = 0U;

    (void)CAN_DrvSendStd(CAN_ID_BMS_FAULT_STATUS, data, 8U);
}
