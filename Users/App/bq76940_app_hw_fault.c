#include "bq76940_app.h"
#include "bms_log.h"
#include "bq76940_app_hw_fault.h"


void BQ76940_AppOcdScdRequestClear( BQ76940_OcdScdRequest_t *req)
{
    if (req == 0)
    {
        return;
    }

    req->action            = BQ76940_OCDSCD_ACTION_NONE;
    req->sys_stat_snapshot = 0U;
    req->hw_fault_now      = 0U;
		req->fault_code 			 = BQ76940_HW_FAULT_CODE_NONE;
		req->apply_ret 				 = 0U;
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
		
		//错误码判断
		if ((req->ocd_now != 0U) && (req->scd_now != 0U))
		{
				req->fault_code = BQ76940_HW_FAULT_CODE_OCD_SCD;
		}
		else if (req->ocd_now != 0U)
		{
				req->fault_code = BQ76940_HW_FAULT_CODE_OCD;
		}
		else if (req->scd_now != 0U)
		{
				req->fault_code = BQ76940_HW_FAULT_CODE_SCD;
		}
		else
		{
				req->fault_code = BQ76940_HW_FAULT_CODE_NONE;
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

uint8_t BQ76940_AppOcdScdCommit(BQ76940_AppCtx_t *ctx,const BQ76940_OcdScdRequest_t *req)
{
    if ((ctx == 0) || (req == 0))
    {
        return 1U;
    }
		
		ctx->hw_fault_sys_stat_latched |= (uint8_t)(req->sys_stat_snapshot & BQ76940_SYS_STAT_HW_LATCH_MASK);

		ctx->hw_fault_last_apply_ret = req->apply_ret;
		ctx->hw_fault_last_code      = req->fault_code;

		if (ctx->hw_fault_count < 65535U)
		{
				ctx->hw_fault_count++;
		}

        /*
     * 锁存本次硬件故障触发时的 SYS_STAT 快照。
     * 注意：
     *   ctx->sys_stat 后续可能被 SampleTask 读取真实 SYS_STAT 覆盖成 0，
     *   所以这里单独保存曾经触发过的硬件故障位。
     */
    ctx->hw_fault_sys_stat_latched |=
        (uint8_t)(req->sys_stat_snapshot & BQ76940_SYS_STAT_HW_LATCH_MASK);

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
    BMS_LOG_TEST_HW_FAULT("[HW] SYS:%02X O:%u S:%u B:%u\r\n",
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

    if ((req->scd_now != 0U) && (req->ocd_now != 0U))
    {
        BMS_LOG_TEST_HW_FAULT("[HW] OCD/SCD block\r\n");
    }
    else if (req->scd_now != 0U)
    {
        BMS_LOG_TEST_HW_FAULT("[HW] SCD block\r\n");
    }
    else if (req->ocd_now != 0U)
    {
        BMS_LOG_TEST_HW_FAULT("[HW] OCD block\r\n");
    }
    else
    {
        BMS_LOG_TEST_HW_FAULT("[HW] no latch\r\n");
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
        BMS_LOG_HW_FAULT("[HW] recover DSG on\r\n");
#endif
    }

    return 0U;
}



uint8_t BQ76940_AppHandleOcdScdProtect(BQ76940_AppCtx_t *ctx)
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

