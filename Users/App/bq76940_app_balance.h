#ifndef __BQ76940_APP_BALANCE_H
#define __BQ76940_APP_BALANCE_H

#include "bq76940_drv.h"

typedef struct
{
    uint16_t diff_enter_mV;       /* 压差进入阈值 */
    uint16_t diff_exit_mV;        /* 压差退出阈值 */
    uint16_t min_cell_mV;         /* 允许均衡的最低最高单体电压门限 */
    int32_t  max_abs_current_mA;  /* 允许均衡的最大电流绝对值 */
} BQ76940_BalanceConfig_t;

/*
 * 自动均衡动作类型
 *
 * NONE:
 *   本轮不需要操作 CELLBAL。
 *
 * START:
 *   本轮需要开启某个单体的均衡。
 *
 * STOP:
 *   本轮需要关闭所有自动均衡。
 */
#define BQ76940_BAL_ACTION_NONE      0U
#define BQ76940_BAL_ACTION_START     1U
#define BQ76940_BAL_ACTION_STOP      2U

/*
 * 自动均衡停止原因
 */
#define BQ76940_BAL_REASON_NONE          0U
#define BQ76940_BAL_REASON_NOT_ALLOWED   1U
#define BQ76940_BAL_REASON_DIFF_EXIT     2U

/*
 * 自动均衡请求结构体
 *
 * 作用：
 *   把“均衡判断”和“CELLBAL 硬件写入”拆开。
 *
 * 使用流程：
 *   1. Decide  阶段：根据 app 状态生成 req
 *   2. ApplyHw 阶段：根据 req 写 BQ76940 CELLBAL
 *   3. Commit  阶段：将执行结果提交回 app
 */
typedef struct
{
    uint8_t action;        /* NONE / START / STOP */
    uint8_t target_label;  /* START 时的目标电芯编号 */
    uint8_t reason;        /* STOP 原因 */

    BQ76940_CellBalRegs_t wr;  /* 准备写入的 CELLBAL */
    BQ76940_CellBalRegs_t rd;  /* 写入后读回的 CELLBAL */
} BQ76940_BalanceRequest_t;



struct BQ76940_AppCtx;

void BQ76940_AppBalanceRequestClear(BQ76940_BalanceRequest_t *req);

uint8_t BQ76940_AppBalanceDecide(const struct BQ76940_AppCtx *ctx,
                                  BQ76940_BalanceRequest_t *req);

uint8_t BQ76940_AppBalanceApplyHw(BQ76940_BalanceRequest_t *req);

uint8_t BQ76940_AppBalanceCommit(struct BQ76940_AppCtx *ctx,
                                  const BQ76940_BalanceRequest_t *req);

uint8_t BQ76940_AppBalanceUpdate(struct BQ76940_AppCtx *ctx);

#endif
