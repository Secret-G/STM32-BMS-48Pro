#include "bq76940_app.h"
#include "bq76940_app_runtime_diag.h"

#include "stdio.h"

void BQ76940_AppRuntimeDiagInit(BQ76940_RuntimeDiag_t *diag)
{
    if (diag == 0)
    {
        return;
    }

    diag->fault_active = 0U;
    diag->safe_off_requested = 0U;
    diag->safe_off_done = 0U;
    diag->safe_off_result = 0U;

    diag->sample_fail_count = 0U;
    diag->sample_success_count = 0U;

    diag->last_fault_code = BQ76940_RT_FAULT_NONE;
    diag->last_fault_stage = BQ76940_RT_STAGE_NONE;
    diag->last_ret = 0U;

    diag->total_sample_fail_count = 0U;
}

uint8_t BQ76940_AppRuntimeDiagIsFaultActive(const BQ76940_AppCtx_t *ctx)
{
    if (ctx == 0)
    {
        return 1U;
    }

    return ctx->runtime_diag.fault_active;
}


void BQ76940_AppRuntimeDiagRecordSampleOk(BQ76940_AppCtx_t *ctx,
                                          uint8_t *recovered)
{
    BQ76940_RuntimeDiag_t *diag;

    if (recovered != 0)
    {
        *recovered = 0U;
    }

    if (ctx == 0)
    {
        return;
    }

    diag = &ctx->runtime_diag;

    diag->sample_fail_count = 0U;

    if (diag->sample_success_count < 255U)
    {
        diag->sample_success_count++;
    }

#if (BQ76940_RT_FAULT_AUTO_RECOVER != 0U)
    if ((diag->fault_active != 0U) &&
        (diag->sample_success_count >= BQ76940_RT_SAMPLE_RECOVER_LIMIT))
    {
        diag->fault_active = 0U;
        diag->safe_off_requested = 0U;
        diag->safe_off_done = 0U;
        diag->safe_off_result = 0U;

        diag->last_fault_code = BQ76940_RT_FAULT_NONE;
        diag->last_fault_stage = BQ76940_RT_STAGE_NONE;
        diag->last_ret = 0U;

        if (recovered != 0)
        {
            *recovered = 1U;
        }

        printf("[RTF] RECOVER after %d ok samples\r\n",
               diag->sample_success_count);

        diag->sample_success_count = 0U;
    }
#endif
}


void BQ76940_AppRuntimeDiagRecordSampleFail(BQ76940_AppCtx_t *ctx,
                                            uint8_t fault_code,
                                            uint8_t fault_stage,
                                            uint8_t ret,
                                            uint8_t *enter_fault)
{
    BQ76940_RuntimeDiag_t *diag;

    if (enter_fault != 0)
    {
        *enter_fault = 0U;
    }

    if (ctx == 0)
    {
        return;
    }

    diag = &ctx->runtime_diag;

    if (diag->sample_fail_count < 255U)
    {
        diag->sample_fail_count++;
    }

    diag->sample_success_count = 0U;

    if (diag->total_sample_fail_count < 65535U)
    {
        diag->total_sample_fail_count++;
    }

    diag->last_fault_code = fault_code;
    diag->last_fault_stage = fault_stage;
    diag->last_ret = ret;

    printf("[RTF] SAMPLE FAIL code=%d stage=%d ret=%d cnt=%d\r\n",
           fault_code,
           fault_stage,
           ret,
           diag->sample_fail_count);

    /*
     * 第一次达到阈值时，锁存 runtime fault 并请求 Safe-Off。
     * 后续继续失败，不重复请求 Safe-Off。
     */
    if ((diag->sample_fail_count >= BQ76940_RT_SAMPLE_FAIL_LIMIT) &&
        (diag->fault_active == 0U))
    {
        diag->fault_active = 1U;
        diag->safe_off_requested = 1U;
        diag->safe_off_done = 0U;
        diag->safe_off_result = 0U;

        if (enter_fault != 0)
        {
            *enter_fault = 1U;
        }

        printf("[RTF] ENTER code=%d stage=%d ret=%d fail_cnt=%d\r\n",
               fault_code,
               fault_stage,
               ret,
               diag->sample_fail_count);
    }
}

void BQ76940_AppRuntimeDiagTakeSafeOffRequest(BQ76940_AppCtx_t *ctx,
                                              uint8_t *need_safe_off)
{
    BQ76940_RuntimeDiag_t *diag;

    if (need_safe_off != 0)
    {
        *need_safe_off = 0U;
    }

    if (ctx == 0)
    {
        return;
    }

    diag = &ctx->runtime_diag;

    if ((diag->fault_active != 0U) &&
        (diag->safe_off_requested != 0U) &&
        (diag->safe_off_done == 0U))
    {
        diag->safe_off_requested = 0U;

        if (need_safe_off != 0)
        {
            *need_safe_off = 1U;
        }
    }
}

void BQ76940_AppRuntimeDiagCommitSafeOffResult(BQ76940_AppCtx_t *ctx,
                                               uint8_t safe_off_result)
{
    BQ76940_RuntimeDiag_t *diag;

    if (ctx == 0)
    {
        return;
    }

    diag = &ctx->runtime_diag;

    diag->safe_off_done = 1U;
    diag->safe_off_result = safe_off_result;

    if (safe_off_result == 0U)
    {
        printf("[RTF] SAFE-OFF done\r\n");
    }
    else
    {
        printf("[RTF] SAFE-OFF fail, result=0x%02X\r\n",
               safe_off_result);
    }
}
