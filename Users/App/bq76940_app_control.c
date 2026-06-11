#include "bq76940_app.h"
#include "bq76940_app_control.h"

#include "stdio.h"

uint8_t BQ76940_AppControlUpdate(BQ76940_AppCtx_t *ctx)
{
    uint8_t ret;
    BQ76200_ExecInput_t exec_input;

    if (ctx == 0)
    {
        return 1U;
    }

    /*
     * 组织 BQ76200 执行层输入。
     *
     * ot_cutoff_active:
     *     过温保护生效，充放电都禁止
     *
     * ut_chg_block_active:
     *     低温禁充，只禁止充电
     *
     * hw_dsg_block_active:
     *     OCD/SCD 等放电侧故障，只禁止放电
     */
		exec_input.ot_cutoff_active      = ctx->ot_cutoff_active;
		exec_input.ut_chg_block_active   = ctx->ut_chg_block_active;
		exec_input.hw_dsg_block_active   = ctx->hw_dsg_block_active;
		exec_input.runtime_fault_active  = ctx->runtime_diag.fault_active;

    /*
     * 更新 BQ76200 执行层状态机
     */
    ret = BQ76200_ExecUpdate(&ctx->bq76200_exec, &exec_input);
    if (ret != 0U)
    {
        printf("[BQ76200 EXEC] update fail, ret = %d\r\n", ret);
        return 31U;
    }

    return 0U;
}

