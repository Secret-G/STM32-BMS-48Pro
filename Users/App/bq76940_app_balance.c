#include "bq76940_app.h"
#include "bq76940_app_balance.h"

#include "stdio.h"

static uint8_t BQ76940_AppIsBalanceAllowed(const BQ76940_AppCtx_t *ctx)
{
    int32_t abs_current_mA;

    if (ctx == 0)
    {
        return 0;
    }

    abs_current_mA = (ctx->pack_current_mA >= 0) ? ctx->pack_current_mA : (-ctx->pack_current_mA);

    /* 保护与告警条件下禁止均衡 */
    if (ctx->alarm_state.uv_flag != 0U) return 0;
    if (ctx->alarm_state.ov_flag != 0U) return 0;
    if (ctx->alarm_state.ot_flag != 0U) return 0;
    if (ctx->alarm_state.ut_flag != 0U) return 0;

    if (ctx->ot_cutoff_active != 0U) return 0;
    if (ctx->ut_chg_block_active != 0U) return 0;
    if (ctx->hw_dsg_block_active != 0U) return 0;
    if (ctx->hw_ocd_active != 0U) return 0;
    if (ctx->hw_scd_active != 0U) return 0;

    /* 电流太大时先不均衡 */
    if (abs_current_mA > ctx->bal_cfg.max_abs_current_mA) return 0;

    /* 最高单体电压太低时先不均衡 */
    if (ctx->cell_stats.max_mV < ctx->bal_cfg.min_cell_mV) return 0;

    return 1;
}

void BQ76940_AppBalanceRequestClear(BQ76940_BalanceRequest_t *req)
{
    if (req == 0)
    {
        return;
    }

    req->action       = BQ76940_BAL_ACTION_NONE;
    req->target_label = 0U;
    req->reason       = BQ76940_BAL_REASON_NONE;

    BQ76940_ClearCellBalRegs(&req->wr);
    BQ76940_ClearCellBalRegs(&req->rd);
}

uint8_t BQ76940_AppBalanceDecide(const BQ76940_AppCtx_t *ctx,
                                  BQ76940_BalanceRequest_t *req)
{
    uint8_t allow_balance;
    uint8_t ret;

    if ((ctx == 0) || (req == 0))
    {
        return 1U;
    }

    BQ76940_AppBalanceRequestClear(req);

    /*
     * Decide 阶段只做判断：
     *   - 读取当前 app 状态
     *   - 判断是否允许均衡
     *   - 判断是否需要 START / STOP
     *   - 生成准备写入的 CELLBAL mask
     *
     * 注意：
     *   这里不访问 I2C，不写 BQ76940。
     */
    allow_balance = BQ76940_AppIsBalanceAllowed(ctx);

    /*
     * 情况 1：
     * 当前不允许均衡，但之前处于均衡状态。
     * 需要关闭所有 CELLBAL。
     */
    if (allow_balance == 0U)
    {
        if (ctx->bal_active != 0U)
        {
            req->action = BQ76940_BAL_ACTION_STOP;
            req->reason = BQ76940_BAL_REASON_NOT_ALLOWED;

            BQ76940_ClearCellBalRegs(&req->wr);
        }

        return 0U;
    }

    /*
     * 情况 2：
     * 当前允许均衡，且还没有处于均衡状态。
     * 如果压差达到进入阈值，则对最高单体开启均衡。
     */
    if (ctx->bal_active == 0U)
    {
        if (ctx->cell_stats.diff_mV >= ctx->bal_cfg.diff_enter_mV)
        {
            req->action       = BQ76940_BAL_ACTION_START;
            req->target_label = ctx->cell_stats.max_cell_label;

            ret = BQ76940_BuildSingleCellBalMask(req->target_label,
                                                 &req->wr);
            if (ret != BQ76940_OK)
            {
                return 2U;
            }
        }

        return 0U;
    }

    /*
     * 情况 3：
     * 当前已经处于均衡状态。
     * 如果压差降到退出阈值，则关闭均衡。
     */
    if (ctx->cell_stats.diff_mV <= ctx->bal_cfg.diff_exit_mV)
    {
        req->action = BQ76940_BAL_ACTION_STOP;
        req->reason = BQ76940_BAL_REASON_DIFF_EXIT;

        BQ76940_ClearCellBalRegs(&req->wr);

        return 0U;
    }

    /*
     * 情况 4：
     * 当前已经在均衡，且没有达到退出条件。
     * 第一版策略保持原目标，不频繁切换均衡通道。
     */
    return 0U;
}

uint8_t BQ76940_AppBalanceApplyHw(BQ76940_BalanceRequest_t *req)
{
    uint8_t ret;

    if (req == 0)
    {
        return 1U;
    }

    /*
     * 没有动作时，不访问 I2C。
     */
    if (req->action == BQ76940_BAL_ACTION_NONE)
    {
        return 0U;
    }

    /*
     * ApplyHw 阶段只负责硬件动作：
     *   1. 写 CELLBAL1/2/3
     *   2. 读回 CELLBAL1/2/3
     *
     * 注意：
     *   该函数会访问 BQ76940 I2C。
     *   在 FreeRTOS 任务中调用前，外层必须持有 i2c mutex。
     */
    ret = BQ76940_WriteCellBalRegs(&req->wr);
    if (ret != BQ76940_OK)
    {
        return 2U;
    }

    ret = BQ76940_ReadCellBalRegs(&req->rd);
    if (ret != BQ76940_OK)
    {
        return 3U;
    }

    return 0U;
}

uint8_t BQ76940_AppBalanceCommit(BQ76940_AppCtx_t *ctx,
                                  const BQ76940_BalanceRequest_t *req)
{
    if ((ctx == 0) || (req == 0))
    {
        return 1U;
    }

    /*
     * 没有动作时，不修改 app。
     */
    if (req->action == BQ76940_BAL_ACTION_NONE)
    {
        return 0U;
    }

    /*
     * 保存本轮自动均衡写入值和读回值。
     */
    ctx->bal_auto_wr = req->wr;
    ctx->bal_auto_rd = req->rd;

    if (req->action == BQ76940_BAL_ACTION_START)
    {
        ctx->bal_active       = 1U;
        ctx->bal_target_label = req->target_label;

        /*
         * 精简打印，避免 Keil 32KB 超限。
         */
        printf("[BAL] START VC%d\r\n", req->target_label);
    }
    else if (req->action == BQ76940_BAL_ACTION_STOP)
    {
        ctx->bal_active       = 0U;
        ctx->bal_target_label = 0U;

        printf("[BAL] STOP R=%d\r\n", req->reason);
    }

    return 0U;
}




uint8_t BQ76940_AppBalanceUpdate(BQ76940_AppCtx_t *ctx)
{
    uint8_t ret;
    BQ76940_BalanceRequest_t req;

    if (ctx == 0)
    {
        return 1U;
    }

    /*
     * 兼容旧接口：
     *   1. Decide  判断是否需要均衡动作
     *   2. ApplyHw 执行 CELLBAL 写入 / 读回
     *   3. Commit  更新 app 均衡状态
     *
     * 注意：
     *   本函数本身不负责加锁。
     *   FreeRTOS 任务中推荐直接使用 Decide / ApplyHw / Commit 三段式接口。
     */
    ret = BQ76940_AppBalanceDecide(ctx, &req);
    if (ret != 0U)
    {
        return 10U;
    }

    ret = BQ76940_AppBalanceApplyHw(&req);
    if (ret != 0U)
    {
        return 20U;
    }

    ret = BQ76940_AppBalanceCommit(ctx, &req);
    if (ret != 0U)
    {
        return 30U;
    }

    return 0U;
}


