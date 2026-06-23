#include "bq76940_app.h"
#include "bms_log.h"
#include "bq76940_app_balance.h"

#include "stdio.h"

static const uint8_t g_bq76940_bal_cell_label[BQ76940_CELL_COUNT_9] =
    {
        1U, 2U, 5U, 6U, 7U, 10U, 11U, 12U, 15U};

static uint8_t BQ76940_AppBalanceIsAdjacentIndex(uint8_t a, uint8_t b)
{
    if (a > b)
    {
        return ((uint8_t)(a - b) == 1U) ? 1U : 0U;
    }

    return ((uint8_t)(b - a) == 1U) ? 1U : 0U;
}

static uint8_t BQ76940_AppBuildMultiCellBalMask(const BQ76940_AppCtx_t *ctx,
                                                BQ76940_BalanceRequest_t *req)
{
    uint8_t selected_idx[BQ76940_BAL_MAX_TARGET_COUNT];
    uint8_t selected_count = 0U;
    uint8_t used[BQ76940_CELL_COUNT_9];
    uint8_t i;
    uint8_t j;
    uint8_t best_idx;
    uint16_t best_mV;
    uint8_t found;
    uint8_t adjacent;
    uint16_t candidate_threshold_mV;

    BQ76940_CellBalRegs_t one_mask;

    if ((ctx == 0) || (req == 0))
    {
        return 1U;
    }

    for (i = 0U; i < BQ76940_CELL_COUNT_9; i++)
    {
        used[i] = 0U;
    }

    BQ76940_ClearCellBalRegs(&req->wr);

    /*
     * 候选条件：
     * 当前电芯电压 >= 最低电芯 + diff_enter_mV
     *
     * 也就是：明显高于最低电芯的电芯，才允许参与均衡。
     */
    candidate_threshold_mV = (uint16_t)(ctx->cell_stats.min_mV + ctx->bal_cfg.diff_enter_mV);

    while (selected_count < BQ76940_BAL_MAX_TARGET_COUNT)
    {
        found = 0U;
        best_idx = 0U;
        best_mV = 0U;

        /*
         * 每一轮从未使用的候选里找最高电芯。
         */
        for (i = 0U; i < BQ76940_CELL_COUNT_9; i++)
        {
            if (used[i] != 0U)
            {
                continue;
            }

            if (ctx->cell_mV[i] < candidate_threshold_mV)
            {
                continue;
            }

            if (ctx->cell_mV[i] < ctx->bal_cfg.min_cell_mV)
            {
                continue;
            }

            /*
             * 不允许和已经选中的电芯相邻。
             */
            adjacent = 0U;
            for (j = 0U; j < selected_count; j++)
            {
                if (BQ76940_AppBalanceIsAdjacentIndex(i, selected_idx[j]) != 0U)
                {
                    adjacent = 1U;
                    break;
                }
            }

            if (adjacent != 0U)
            {
                continue;
            }

            if ((found == 0U) || (ctx->cell_mV[i] > best_mV))
            {
                found = 1U;
                best_idx = i;
                best_mV = ctx->cell_mV[i];
            }
        }

        if (found == 0U)
        {
            break;
        }

        /*
         * 选中 best_idx，对应一个真实 BQ76940 cell label。
         */
        selected_idx[selected_count] = best_idx;
        selected_count++;

        used[best_idx] = 1U;

        /*
         * 复用已有单节 mask 构造函数，
         * 然后 OR 到本轮多节 wr mask 里。
         */
        BQ76940_ClearCellBalRegs(&one_mask);

        if (BQ76940_BuildSingleCellBalMask(g_bq76940_bal_cell_label[best_idx],
                                           &one_mask) != BQ76940_OK)
        {
            return 2U;
        }

        req->wr.cellbal1 |= one_mask.cellbal1;
        req->wr.cellbal2 |= one_mask.cellbal2;
        req->wr.cellbal3 |= one_mask.cellbal3;
    }

    req->target_count = selected_count;

    if (selected_count != 0U)
    {
        /*
         * target_label 仍然保存最高目标，方便日志和 CAN 兼容旧字段。
         */
        req->target_label = g_bq76940_bal_cell_label[selected_idx[0]];
    }

    return 0U;
}

static uint8_t BQ76940_AppIsBalanceAllowed(const BQ76940_AppCtx_t *ctx)
{
    int32_t abs_current_mA;

    if (ctx == 0)
    {
        return 0;
    }

    abs_current_mA = (ctx->pack_current_mA >= 0) ? ctx->pack_current_mA : (-ctx->pack_current_mA);

    /* 保护与告警条件下禁止均衡 */
    if (ctx->alarm_state.uv_flag != 0U)
        return 0;
    if (ctx->alarm_state.ov_flag != 0U)
        return 0;
    if (ctx->alarm_state.ot_flag != 0U)
        return 0;
    if (ctx->alarm_state.ut_flag != 0U)
        return 0;

    if (ctx->ot_cutoff_active != 0U)
        return 0;
    if (ctx->ut_chg_block_active != 0U)
        return 0;
    if (ctx->hw_dsg_block_active != 0U)
        return 0;
    if (ctx->hw_ocd_active != 0U)
        return 0;
    if (ctx->hw_scd_active != 0U)
        return 0;

    if (ctx->runtime_diag.fault_active != 0U)
        return 0;

    /* 电流太大时先不均衡 */
    if (abs_current_mA > ctx->bal_cfg.max_abs_current_mA)
        return 0;

    /* 最高单体电压太低时先不均衡 */
    if (ctx->cell_stats.max_mV < ctx->bal_cfg.min_cell_mV)
        return 0;

    return 1;
}

void BQ76940_AppBalanceRequestClear(BQ76940_BalanceRequest_t *req)
{
    if (req == 0)
    {
        return;
    }

    req->action = BQ76940_BAL_ACTION_NONE;
    req->target_label = 0U;
    req->target_count = 0U;
    req->reason = BQ76940_BAL_REASON_NONE;

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
            ret = BQ76940_AppBuildMultiCellBalMask(ctx, req);
            if (ret != 0U)
            {
                return 2U;
            }

            if (req->target_count != 0U)
            {
                req->action = BQ76940_BAL_ACTION_START;
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

    if ((req->rd.cellbal1 != req->wr.cellbal1) ||
        (req->rd.cellbal2 != req->wr.cellbal2) ||
        (req->rd.cellbal3 != req->wr.cellbal3))
    {
        return 4U;
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
        ctx->bal_active = 1U;
        ctx->bal_target_label = req->target_label;
			  ctx->bal_target_count = req->target_count;

    }
    else if (req->action == BQ76940_BAL_ACTION_STOP)
    {
        ctx->bal_active = 0U;
        ctx->bal_target_label = 0U;
        ctx->bal_target_count = 0U;

        BMS_LOG_BALANCE("[BAL] off:%d\r\n", req->reason);
    }
    return 0U;
}
