#include "bq76940_app.h"
#include "bq76940_app_protect.h"

#include "stdio.h"

static uint8_t BQ76940_AppHandleTempProtect(BQ76940_AppCtx_t *ctx)
{
    uint8_t ret;
    BQ76940_OtProtectRequest_t req;

    if (ctx == 0)
    {
        return 1U;
    }

    /*
     * 兼容旧接口：
     *   1. Decide  只判断
     *   2. ApplyHw 只写硬件
     *   3. Commit  只更新 ctx
     *
     * 注意：
     *   本函数本身不负责加锁。
     *   后续在 FreeRTOS ProtectTask 中，应直接调用三段式接口。
     */
    ret = BQ76940_AppOtProtectDecide(ctx, &req);
    if (ret != 0U)
    {
        return 10U;
    }

    ret = BQ76940_AppOtProtectApplyHw(&req);
    if (ret != 0U)
    {
        return 20U;
    }

    ret = BQ76940_AppOtProtectCommit(ctx, &req);
    if (ret != 0U)
    {
        return 30U;
    }

    return 0U;
}

void BQ76940_AppOtProtectRequestClear(BQ76940_OtProtectRequest_t *req)
{
    if (req == 0)
    {
        return;
    }

    req->action = BQ76940_OT_ACTION_NONE;

    req->ot_now = 0U;
    req->ov_now = 0U;
    req->uv_now = 0U;

    req->ot_cutoff_active_snapshot = 0U;
}

uint8_t BQ76940_AppOtProtectDecide(const BQ76940_AppCtx_t *ctx,
                                    BQ76940_OtProtectRequest_t *req)
{
    if ((ctx == 0) || (req == 0))
    {
        return 1U;
    }

    BQ76940_AppOtProtectRequestClear(req);

    /*
     * 保存当前保护相关状态快照。
     *
     * 注意：
     *   Decide 阶段只读取 ctx，不访问 BQ76940 I2C，
     *   也不修改 ctx。
     */
    req->ot_now = ctx->alarm_state.ot_flag;
    req->ov_now = ctx->alarm_state.ov_flag;
    req->uv_now = ctx->alarm_state.uv_flag;

    req->ot_cutoff_active_snapshot = ctx->ot_cutoff_active;

    /*
     * 情况 1：
     * OT 过温触发，并且当前还没有进入 OT 截止状态。
     * 需要关闭 CHG / DSG。
     */
    if ((req->ot_now != 0U) &&
        (req->ot_cutoff_active_snapshot == 0U))
    {
        req->action = BQ76940_OT_ACTION_FET_OFF;
        return 0U;
    }

    /*
     * 情况 2：
     * OT 已恢复，并且之前处于 OT 截止状态。
     *
     * 保持你原来的逻辑：
     *   只有当前没有 OV / UV 时，才允许恢复 CHG / DSG。
     */
    if ((req->ot_now == 0U) &&
        (req->ot_cutoff_active_snapshot != 0U))
    {
        if ((req->ov_now == 0U) &&
            (req->uv_now == 0U))
        {
            req->action = BQ76940_OT_ACTION_FET_ON;
            return 0U;
        }
    }

    /*
     * 情况 3：
     * 本轮不需要硬件动作。
     */
    req->action = BQ76940_OT_ACTION_NONE;

    return 0U;
}

uint8_t BQ76940_AppOtProtectApplyHw(const BQ76940_OtProtectRequest_t *req)
{
    uint8_t ret;

    if (req == 0)
    {
        return 1U;
    }

    /*
     * 无动作时，不访问 I2C。
     */
    if (req->action == BQ76940_OT_ACTION_NONE)
    {
        return 0U;
    }

    /*
     * OT active：
     *   CHG = OFF
     *   DSG = OFF
     */
    if (req->action == BQ76940_OT_ACTION_FET_OFF)
    {
        ret = BQ76940_SetFETState(0U, 0U);
        if (ret != 0U)
        {
            return 2U;
        }

        return 0U;
    }

    /*
     * OT recover：
     *   CHG = ON
     *   DSG = ON
     *
     * 注意：
     *   是否允许恢复已经在 Decide 阶段判断。
     */
    if (req->action == BQ76940_OT_ACTION_FET_ON)
    {
        ret = BQ76940_SetFETState(1U, 1U);
        if (ret != 0U)
        {
            return 3U;
        }

        return 0U;
    }

    return 4U;
}


uint8_t BQ76940_AppOtProtectCommit(BQ76940_AppCtx_t *ctx,
                                    const BQ76940_OtProtectRequest_t *req)
{
    if ((ctx == 0) || (req == 0))
    {
        return 1U;
    }

    /*
     * 无动作时，不修改 ctx。
     */
    if (req->action == BQ76940_OT_ACTION_NONE)
    {
        return 0U;
    }

    /*
     * OT 触发后，提交截止状态。
     */
    if (req->action == BQ76940_OT_ACTION_FET_OFF)
    {
        ctx->ot_cutoff_active = 1U;

#if (BQ76940_PROTECT_EVENT_PRINT_ENABLE != 0U)
        printf("[PROT] OT -> CHG OFF, DSG OFF\r\n");
#endif

        return 0U;
    }

    /*
     * OT 恢复后，清除截止状态。
     */
    if (req->action == BQ76940_OT_ACTION_FET_ON)
    {
        ctx->ot_cutoff_active = 0U;

#if (BQ76940_PROTECT_EVENT_PRINT_ENABLE != 0U)
        printf("[PROT] OT recover -> CHG ON, DSG ON\r\n");
#endif

        return 0U;
    }

    return 2U;
}


uint8_t BQ76940_AppProtectUpdateAlarms(BQ76940_AppCtx_t *ctx)
{
    uint8_t ret;

    if (ctx == 0)
    {
        return 1U;
    }

    /*
     * 1. 更新单体电压相关软件告警：
     *    UV / OV / DIFF
     */
    ret = BQ76940_UpdateAlarmState9(ctx->cell_mV,
                                    &ctx->cell_stats,
                                    &ctx->alarm_th,
                                    &ctx->alarm_state);
    if (ret != 0U)
    {
        return 13U;
    }

    /*
     * 2. 更新 TS1 过温告警
     */
    ret = BQ76940_UpdateTempAlarmTs1(ctx->ts1_temp_dC,
                                     &ctx->alarm_th,
                                     &ctx->alarm_state);
    if (ret != 0U)
    {
        printf("[TEMP ALARM] update fail, ret = %d\r\n", ret);
        return 20U;
    }

    /*
     * 3. 更新 TS1 低温告警
     */
    ret = BQ76940_UpdateLowTempAlarmTs1(ctx->ts1_temp_dC,
                                        &ctx->alarm_th,
                                        &ctx->alarm_state);
    if (ret != 0U)
    {
        printf("[LOW TEMP ALARM] update fail, ret = %d\r\n", ret);
        return 22U;
    }

    return 0U;
}

static uint8_t BQ76940_AppHandleLowTempProtect(BQ76940_AppCtx_t *ctx)
{
    uint8_t ret;
    BQ76940_UtProtectRequest_t req;

    if (ctx == 0)
    {
        return 1U;
    }

    /*
     * 兼容旧接口：
     *   1. Decide  只判断
     *   2. ApplyHw 只写硬件
     *   3. Commit  只更新 ctx
     *
     * 注意：
     *   本函数本身不负责加锁。
     *   后续在 FreeRTOS ProtectTask 中，应直接调用三段式接口。
     */
    ret = BQ76940_AppUtProtectDecide(ctx, &req);
    if (ret != 0U)
    {
        return 10U;
    }

    ret = BQ76940_AppUtProtectApplyHw(&req);
    if (ret != 0U)
    {
        return 20U;
    }

    ret = BQ76940_AppUtProtectCommit(ctx, &req);
    if (ret != 0U)
    {
        return 30U;
    }

    return 0U;
}


void BQ76940_AppUtProtectRequestClear(BQ76940_UtProtectRequest_t *req)
{
    if (req == 0)
    {
        return;
    }

    req->action = BQ76940_UT_ACTION_NONE;

    req->ut_now = 0U;
    req->ov_now = 0U;
    req->ot_now = 0U;

    req->ot_cutoff_active_snapshot  = 0U;
    req->ut_chg_block_active_snapshot = 0U;
}

uint8_t BQ76940_AppUtProtectDecide(const BQ76940_AppCtx_t *ctx,
                                    BQ76940_UtProtectRequest_t *req)
{
    if ((ctx == 0) || (req == 0))
    {
        return 1U;
    }

    BQ76940_AppUtProtectRequestClear(req);

    /*
     * 保存当前保护相关状态快照。
     *
     * Decide 阶段只读取 ctx：
     *   - 不访问 BQ76940 I2C
     *   - 不修改 ctx
     */
    req->ut_now = ctx->alarm_state.ut_flag;
    req->ov_now = ctx->alarm_state.ov_flag;
    req->ot_now = ctx->alarm_state.ot_flag;

    req->ot_cutoff_active_snapshot   = ctx->ot_cutoff_active;
    req->ut_chg_block_active_snapshot = ctx->ut_chg_block_active;

    /*
     * 情况 1：
     * UT 低温触发，并且当前还没有进入 CHG 阻断状态。
     *
     * 低温保护策略：
     *   只关闭 CHG，不关闭 DSG。
     */
    if ((req->ut_now != 0U) &&
        (req->ut_chg_block_active_snapshot == 0U))
    {
        req->action = BQ76940_UT_ACTION_CHG_OFF;
        return 0U;
    }

    /*
     * 情况 2：
     * UT 已恢复，并且之前处于 CHG 阻断状态。
     *
     * 保持你原来的恢复条件：
     *   - 当前没有 OV
     *   - 当前没有 OT
     *   - 当前没有 OT 截止状态
     */
    if ((req->ut_now == 0U) &&
        (req->ut_chg_block_active_snapshot != 0U))
    {
        if ((req->ov_now == 0U) &&
            (req->ot_now == 0U) &&
            (req->ot_cutoff_active_snapshot == 0U))
        {
            req->action = BQ76940_UT_ACTION_CHG_ON;
            return 0U;
        }
    }

    /*
     * 情况 3：
     * 本轮不需要硬件动作。
     */
    req->action = BQ76940_UT_ACTION_NONE;

    return 0U;
}

uint8_t BQ76940_AppUtProtectApplyHw(const BQ76940_UtProtectRequest_t *req)
{
    uint8_t ret;

    if (req == 0)
    {
        return 1U;
    }

    /*
     * 无动作时，不访问 I2C。
     */
    if (req->action == BQ76940_UT_ACTION_NONE)
    {
        return 0U;
    }

    /*
     * UT active：
     *   CHG = OFF
     *   DSG = unchanged
     */
    if (req->action == BQ76940_UT_ACTION_CHG_OFF)
    {
        ret = BQ76940_SetCHGState(0U);
        if (ret != 0U)
        {
            return 2U;
        }

#if (BQ76940_PROTECT_DBG_ENABLE != 0U)
        /*
         * 读回 SYS_CTRL2 只用于调试确认。
         * 默认关闭，避免增加 I2C 访问和 Flash 字符串。
         */
        ret = BQ76940_AppPrintSysCtrl2Readback("UT ACTIVE READBACK");
        if (ret != 0U)
        {
            return 4U;
        }
#endif

        return 0U;
    }

    /*
     * UT recover：
     *   CHG = ON
     *   DSG = unchanged
     *
     * 是否允许恢复已经在 Decide 阶段判断。
     */
    if (req->action == BQ76940_UT_ACTION_CHG_ON)
    {
        ret = BQ76940_SetCHGState(1U);
        if (ret != 0U)
        {
            return 3U;
        }

#if (BQ76940_PROTECT_DBG_ENABLE != 0U)
        /*
         * 恢复后读回 SYS_CTRL2，仅用于调试确认。
         */
        ret = BQ76940_AppPrintSysCtrl2Readback("UT RECOVER READBACK");
        if (ret != 0U)
        {
            return 5U;
        }
#endif

        return 0U;
    }

    return 6U;
}

uint8_t BQ76940_AppUtProtectCommit(BQ76940_AppCtx_t *ctx,
                                    const BQ76940_UtProtectRequest_t *req)
{
    if ((ctx == 0) || (req == 0))
    {
        return 1U;
    }

    /*
     * 无动作时，不修改 ctx。
     */
    if (req->action == BQ76940_UT_ACTION_NONE)
    {
        return 0U;
    }

    /*
     * UT 触发后，提交 CHG 阻断状态。
     */
    if (req->action == BQ76940_UT_ACTION_CHG_OFF)
    {
        ctx->ut_chg_block_active = 1U;

#if (BQ76940_PROTECT_EVENT_PRINT_ENABLE != 0U)
        printf("[PROT] UT -> CHG OFF\r\n");
#endif

        return 0U;
    }

    /*
     * UT 恢复后，清除 CHG 阻断状态。
     */
    if (req->action == BQ76940_UT_ACTION_CHG_ON)
    {
        ctx->ut_chg_block_active = 0U;

#if (BQ76940_PROTECT_EVENT_PRINT_ENABLE != 0U)
        printf("[PROT] UT recover -> CHG ON\r\n");
#endif

        return 0U;
    }

    return 2U;
}


void BQ76940_AppOcdScdRequestClear(BQ76940_OcdScdRequest_t *req)
{
    if (req == 0)
    {
        return;
    }

    req->action            = BQ76940_OCDSCD_ACTION_NONE;
    req->sys_stat_snapshot = 0U;
    req->hw_fault_now      = 0U;
    req->ocd_now           = 0U;
    req->scd_now           = 0U;
    req->recover_request   = 0U;
}

uint8_t BQ76940_AppOcdScdDecide(const BQ76940_AppCtx_t *ctx,
                                BQ76940_OcdScdRequest_t *req)
{
    if ((ctx == 0) || (req == 0))
    {
        return 1U;
    }

    BQ76940_AppOcdScdRequestClear(req);

    /*
     * 保存当前 SYS_STAT 快照。
     *
     * 注意：
     *   ctx->sys_stat 已经在 SampleTask 采样阶段读取并提交到 app。
     *   这里不直接读 BQ76940。
     */
    req->sys_stat_snapshot = ctx->sys_stat;

    req->hw_fault_now = (uint8_t)(ctx->sys_stat &
                                  (BQ76940_SYS_STAT_OCD |
                                   BQ76940_SYS_STAT_SCD));

    if ((ctx->sys_stat & BQ76940_SYS_STAT_OCD) != 0U)
    {
        req->ocd_now = 1U;
    }

    if ((ctx->sys_stat & BQ76940_SYS_STAT_SCD) != 0U)
    {
        req->scd_now = 1U;
    }

    /*
     * 情况 1：
     * 当前硬件检测到 OCD / SCD，并且 DSG 还没有被阻断。
     * 需要执行 DSG_OFF。
     */
    if ((req->hw_fault_now != 0U) &&
        (ctx->hw_dsg_block_active == 0U))
    {
        req->action = BQ76940_OCDSCD_ACTION_DSG_OFF;
        return 0U;
    }

    /*
     * 情况 2：
     * 手动恢复：
     *   - 已经人工发起恢复请求
     *   - 当前 DSG 处于阻断状态
     *   - 当前硬件 OCD / SCD 位已经清除
     *   - 当前没有 UV / OT 阻塞
     */
    if ((ctx->hw_fault_recover_once_enable != 0U) &&
        (ctx->hw_dsg_block_active != 0U) &&
        (req->hw_fault_now == 0U))
    {
        if ((ctx->alarm_state.uv_flag == 0U) &&
            (ctx->alarm_state.ot_flag == 0U))
        {
            req->action = BQ76940_OCDSCD_ACTION_DSG_ON;
            req->recover_request = 1U;
            return 0U;
        }
    }

    /*
     * 情况 3：
     * 无需硬件动作。
     * 但 Commit 阶段仍然可以根据 ocd_now / scd_now 锁存状态。
     */
    req->action = BQ76940_OCDSCD_ACTION_NONE;

    return 0U;
}

uint8_t BQ76940_AppOcdScdApplyHw(const BQ76940_OcdScdRequest_t *req)
{
    uint8_t ret;

    if (req == 0)
    {
        return 1U;
    }

    /*
     * 无动作时，不访问 I2C。
     */
    if (req->action == BQ76940_OCDSCD_ACTION_NONE)
    {
        return 0U;
    }

    if (req->action == BQ76940_OCDSCD_ACTION_DSG_OFF)
    {
        ret = BQ76940_SetDSGState(0U);
        if (ret != 0U)
        {
            return 2U;
        }

#if (BQ76940_PROTECT_DBG_ENABLE != 0U)
        /*
         * 读回 SYS_CTRL2 只用于调试确认。
         * 默认关闭，避免增加 I2C 访问和 Flash 字符串。
         */
        ret = BQ76940_AppPrintSysCtrl2Readback("OCD/SCD ACTIVE READBACK");
        if (ret != 0U)
        {
            return 3U;
        }
#endif

        return 0U;
    }

    if (req->action == BQ76940_OCDSCD_ACTION_DSG_ON)
    {
        ret = BQ76940_SetDSGState(1U);
        if (ret != 0U)
        {
            return 4U;
        }

#if (BQ76940_PROTECT_DBG_ENABLE != 0U)
        ret = BQ76940_AppPrintSysCtrl2Readback("OCD/SCD RECOVER READBACK");
        if (ret != 0U)
        {
            return 5U;
        }
#endif

        return 0U;
    }

    return 6U;
}

uint8_t BQ76940_AppOcdScdCommit(BQ76940_AppCtx_t *ctx,
                                const BQ76940_OcdScdRequest_t *req)
{
    if ((ctx == 0) || (req == 0))
    {
        return 1U;
    }

    /*
     * 锁存当前是否发生过 OCD / SCD。
     * 即使本轮没有硬件动作，也要记录当前故障状态。
     */
    if (req->ocd_now != 0U)
    {
        ctx->hw_ocd_active = 1U;
    }

    if (req->scd_now != 0U)
    {
        ctx->hw_scd_active = 1U;
    }

#if (BQ76940_PROTECT_DBG_ENABLE != 0U)
    printf("[OCD/SCD DBG] SYS=%02X OCD=%u SCD=%u DSG_BLK=%u\r\n",
           req->sys_stat_snapshot,
           ctx->hw_ocd_active,
           ctx->hw_scd_active,
           ctx->hw_dsg_block_active);
#endif

    /*
     * DSG_OFF 执行成功后，提交 DSG 阻断状态。
     */
    if (req->action == BQ76940_OCDSCD_ACTION_DSG_OFF)
    {
        ctx->hw_dsg_block_active = 1U;

#if (BQ76940_PROTECT_EVENT_PRINT_ENABLE != 0U)
        if (req->scd_now != 0U)
        {
            printf("[HW] SCD -> DSG OFF\r\n");
        }
        else if (req->ocd_now != 0U)
        {
            printf("[HW] OCD -> DSG OFF\r\n");
        }
        else
        {
            printf("[HW] OCD/SCD -> DSG OFF\r\n");
        }
#endif
    }
    /*
     * DSG_ON 执行成功后，清除锁存状态和恢复请求。
     */
    else if (req->action == BQ76940_OCDSCD_ACTION_DSG_ON)
    {
        ctx->hw_dsg_block_active          = 0U;
        ctx->hw_ocd_active                = 0U;
        ctx->hw_scd_active                = 0U;
        ctx->hw_fault_recover_once_enable = 0U;

#if (BQ76940_PROTECT_EVENT_PRINT_ENABLE != 0U)
        printf("[HW] OCD/SCD recover -> DSG ON\r\n");
#endif
    }

    return 0U;
}




static uint8_t BQ76940_AppHandleOcdScdProtect(BQ76940_AppCtx_t *ctx)
{
    uint8_t ret;
    BQ76940_OcdScdRequest_t req;

    if (ctx == 0)
    {
        return 1U;
    }

    /*
     * 兼容旧接口：
     *   1. Decide  只判断
     *   2. ApplyHw 只写硬件
     *   3. Commit  只更新 ctx
     *
     * 注意：
     *   本函数不负责加锁。
     *   后续在 FreeRTOS ProtectTask 中，应直接调用三段式接口。
     */
    ret = BQ76940_AppOcdScdDecide(ctx, &req);
    if (ret != 0U)
    {
        return 10U;
    }

    ret = BQ76940_AppOcdScdApplyHw(&req);
    if (ret != 0U)
    {
        return 20U;
    }

    ret = BQ76940_AppOcdScdCommit(ctx, &req);
    if (ret != 0U)
    {
        return 30U;
    }

    return 0U;
}

uint8_t BQ76940_AppProtectUpdateBase(BQ76940_AppCtx_t *ctx)
{
    if (ctx == 0)
    {
        return 1U;
    }

    /*
     * Base 阶段只做告警更新：
     *   - UV / OV / DIFF
     *   - OT
     *   - UT
     *
     * 不执行 CHG / DSG / CELLBAL 等硬件动作。
     * 不访问 I2C。
     */
    return BQ76940_AppProtectUpdateAlarms(ctx);
}


uint8_t BQ76940_AppProtectUpdate(BQ76940_AppCtx_t *ctx)
{
    uint8_t ret;

    if (ctx == 0)
    {
        return 1U;
    }

    /*
     * 1. 更新单体电压相关软件告警：
     * UV / OV / DIFF
     */
    ret = BQ76940_UpdateAlarmState9(ctx->cell_mV,
                                    &ctx->cell_stats,
                                    &ctx->alarm_th,
                                    &ctx->alarm_state);
    if (ret != 0U)
    {
        return 13U;
    }

    /*
     * 2. 更新 TS1 过温告警
     */
    ret = BQ76940_UpdateTempAlarmTs1(ctx->ts1_temp_dC,
                                     &ctx->alarm_th,
                                     &ctx->alarm_state);
    if (ret != 0U)
    {
        printf("[TEMP ALARM] update fail, ret = %d\r\n", ret);
        return 20U;
    }

    /*
     * 3. 更新 TS1 低温告警
     */
    ret = BQ76940_UpdateLowTempAlarmTs1(ctx->ts1_temp_dC,
                                        &ctx->alarm_th,
                                        &ctx->alarm_state);
    if (ret != 0U)
    {
        printf("[LOW TEMP ALARM] update fail, ret = %d\r\n", ret);
        return 22U;
    }

    /*
     * 4. 处理过温联动控制：
     * OT 生效时，充放电都禁止。
     */
    ret = BQ76940_AppHandleTempProtect(ctx);
    if (ret != 0U)
    {
        printf("[TEMP PROTECT] handle fail, ret = %d\r\n", ret);
        return 21U;
    }

    /*
     * 5. 处理低温联动控制：
     * UT 生效时，只禁止充电。
     */
    ret = BQ76940_AppHandleLowTempProtect(ctx);
    if (ret != 0U)
    {
        printf("[LOW TEMP PROTECT] handle fail, ret = %d\r\n", ret);
        return 25U;
    }

    /*
     * 6. 处理 OCD / SCD 放电侧硬件保护状态
     */
    ret = BQ76940_AppHandleOcdScdProtect(ctx);
    if (ret != 0U)
    {
        printf("[OCD/SCD PROTECT] handle fail, ret = %d\r\n", ret);
        return 27U;
    }

    return 0U;
}

