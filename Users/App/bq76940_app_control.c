#include "bq76940_app.h"
#include "bms_log.h"
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
     * ��֯ BQ76200 ִ�в����롣
     *
     * ot_cutoff_active:
     *     ���±�����Ч����ŵ綼��ֹ
     *
     * ut_chg_block_active:
     *     ���½��䣬ֻ��ֹ���
     *
     * hw_dsg_block_active:
     *     OCD/SCD �ȷŵ����ϣ�ֻ��ֹ�ŵ�
     */
		exec_input.ot_cutoff_active      = ctx->ot_cutoff_active;
		exec_input.ut_chg_block_active   = ctx->ut_chg_block_active;
		exec_input.hw_dsg_block_active   = ctx->hw_dsg_block_active;
		exec_input.runtime_fault_active  = ctx->runtime_diag.fault_active;

    /*
     * ���� BQ76200 ִ�в�״̬��
     */
    ret = BQ76200_ExecUpdate(&ctx->bq76200_exec, &exec_input);
    if (ret != 0U)
    {
        BMS_LOG_ERROR("[CTRL] exec:%d\r\n", ret);
        return 31U;
    }

    return 0U;
}

