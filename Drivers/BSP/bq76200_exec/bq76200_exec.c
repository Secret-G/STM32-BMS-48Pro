#include "bq76200_exec.h"
#include "bq76200_exec_port.h"
#include <stdio.h>

/* 内部函数：根据状态真正刷新 GPIO */
static void BQ76200_ExecApplyState(BQ76200_ExecState_t state);


/* ==============================
 * 初始化执行层
 * ============================== */
uint8_t BQ76200_ExecInit(BQ76200_ExecCtx_t *ctx)
{
    if (ctx == 0)
    {
        return 1;
    }

    /* 先初始化执行口：PB10~PB13 */
    BQ76200_PortInit();

    ctx->state = BQ76200_EXEC_STATE_OFF;
    ctx->last_state = BQ76200_EXEC_STATE_OFF;

    /*
     * 上电默认进入安全关闭状态：
     * CHG_EN  = 0
     * DSG_EN  = 0
     * CP_EN   = 0
     * PCHG_EN = 0
     */
    BQ76200_ExecApplyState(BQ76200_EXEC_STATE_OFF);

    return 0;
}


/* ==============================
 * 执行层状态更新
 * ============================== */
uint8_t BQ76200_ExecUpdate(BQ76200_ExecCtx_t *ctx,
                           const BQ76200_ExecInput_t *input)
{
    BQ76200_ExecState_t next_state;

    if ((ctx == 0) || (input == 0))
    {
        return 1;
    }

    /*
     * 状态优先级：
     *
     * 1. OT 过温最高优先级：
     *    充电、放电都禁止
     *
     * 2. UT 和 OCD/SCD 同时存在：
     *    UT 禁充，OCD/SCD 禁放，所以充放都禁止
     *
     * 3. UT 单独存在：
     *    只禁止充电，允许放电
     *
     * 4. OCD/SCD 单独存在：
     *    只禁止放电，允许充电
     *
     * 5. 无故障：
     *    正常打开充放电
     *
     * 说明：
     * PRECHARGE 当前第一版仅保留状态定义，
     * 暂未在这里引入实际状态切换逻辑。
     */
    if (input->ot_cutoff_active != 0U)
    {
        next_state = BQ76200_EXEC_STATE_CHG_DSG_BLOCK;
    }
    else if ((input->ut_chg_block_active != 0U) &&
             (input->hw_dsg_block_active != 0U))
    {
        next_state = BQ76200_EXEC_STATE_CHG_DSG_BLOCK;
    }
    else if (input->ut_chg_block_active != 0U)
    {
        next_state = BQ76200_EXEC_STATE_CHG_BLOCK;
    }
    else if (input->hw_dsg_block_active != 0U)
    {
        next_state = BQ76200_EXEC_STATE_DSG_BLOCK;
    }
    else
    {
        next_state = BQ76200_EXEC_STATE_NORMAL_ON;
    }

    /* 只有发生状态变化时，才更新 last_state */
    if (ctx->state != next_state)
    {
        ctx->last_state = ctx->state;
        ctx->state = next_state;
    }

    /*
     * 第一版每轮都刷新 GPIO。
     * 这样调试最直观，后面再优化成状态变化才刷新也可以。
     */
    BQ76200_ExecApplyState(ctx->state);

    return 0;
}


/* ==============================
 * 获取当前状态
 * ============================== */
BQ76200_ExecState_t BQ76200_ExecGetState(const BQ76200_ExecCtx_t *ctx)
{
    if (ctx == 0)
    {
        return BQ76200_EXEC_STATE_OFF;
    }

    return ctx->state;
}


/* ==============================
 * 状态转字符串
 * ============================== */
const char *BQ76200_ExecStateToString(BQ76200_ExecState_t state)
{
    switch (state)
    {
        case BQ76200_EXEC_STATE_OFF:
            return "OFF";

        case BQ76200_EXEC_STATE_PRECHARGE:
            return "PRECHARGE";

        case BQ76200_EXEC_STATE_NORMAL_ON:
            return "NORMAL_ON";

        case BQ76200_EXEC_STATE_CHG_BLOCK:
            return "CHG_BLOCK";

        case BQ76200_EXEC_STATE_DSG_BLOCK:
            return "DSG_BLOCK";

        case BQ76200_EXEC_STATE_CHG_DSG_BLOCK:
            return "CHG_DSG_BLOCK";

        default:
            return "UNKNOWN";
    }
}


/* ==============================
 * 打印执行层状态
 * ============================== */
void BQ76200_ExecPrintState(const BQ76200_ExecCtx_t *ctx)
{
    if (ctx == 0)
    {
        return;
    }

    printf("[BQ76200 EXEC]\r\n");
    printf("STATE        = %s\r\n", BQ76200_ExecStateToString(ctx->state));
    printf("LAST_STATE   = %s\r\n", BQ76200_ExecStateToString(ctx->last_state));

    /* GPIO 读回 */
    printf("PB10_CHG_EN  = %d\r\n", BQ76200_CHG_EN_ReadBack());
    printf("PB11_DSG_EN  = %d\r\n", BQ76200_DSG_EN_ReadBack());
    printf("PB12_CP_EN   = %d\r\n", BQ76200_CP_EN_ReadBack());
    printf("PB13_PCHG_EN = %d\r\n", BQ76200_PCHG_EN_ReadBack());
}


/* ==============================
 * 根据状态刷新 GPIO 输出
 * ============================== */
static void BQ76200_ExecApplyState(BQ76200_ExecState_t state)
{
    switch (state)
    {
        case BQ76200_EXEC_STATE_OFF:
            BQ76200_CHG_EN_Write(0);
            BQ76200_DSG_EN_Write(0);
            BQ76200_CP_EN_Write(0);
            BQ76200_PCHG_EN_Write(0);
            break;

        case BQ76200_EXEC_STATE_PRECHARGE:
            BQ76200_CP_EN_Write(1);
            BQ76200_PCHG_EN_Write(1);
            BQ76200_CHG_EN_Write(0);
            BQ76200_DSG_EN_Write(0);
            break;

        case BQ76200_EXEC_STATE_NORMAL_ON:
            BQ76200_CP_EN_Write(1);
            BQ76200_PCHG_EN_Write(0);
            BQ76200_CHG_EN_Write(1);
            BQ76200_DSG_EN_Write(1);
            break;

        case BQ76200_EXEC_STATE_CHG_BLOCK:
            BQ76200_CP_EN_Write(1);
            BQ76200_PCHG_EN_Write(0);
            BQ76200_CHG_EN_Write(0);
            BQ76200_DSG_EN_Write(1);
            break;

        case BQ76200_EXEC_STATE_DSG_BLOCK:
            BQ76200_CP_EN_Write(1);
            BQ76200_PCHG_EN_Write(0);
            BQ76200_CHG_EN_Write(1);
            BQ76200_DSG_EN_Write(0);
            break;

        case BQ76200_EXEC_STATE_CHG_DSG_BLOCK:
            BQ76200_CP_EN_Write(1);
            BQ76200_PCHG_EN_Write(0);
            BQ76200_CHG_EN_Write(0);
            BQ76200_DSG_EN_Write(0);
            break;

        default:
            BQ76200_CHG_EN_Write(0);
            BQ76200_DSG_EN_Write(0);
            BQ76200_CP_EN_Write(0);
            BQ76200_PCHG_EN_Write(0);
            break;
    }
}
