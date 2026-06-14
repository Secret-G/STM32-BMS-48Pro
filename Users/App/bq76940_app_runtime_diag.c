#include "bq76940_app.h"
#include "bms_log.h"
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

    diag->safe_off_retry_count = 0U;
    diag->safe_off_failed = 0U;

    diag->sample_fail_count = 0U;
    diag->sample_success_count = 0U;

    diag->last_fault_code = BQ76940_RT_FAULT_NONE;
    diag->last_fault_stage = BQ76940_RT_STAGE_NONE;
    diag->last_ret = 0U;

    diag->total_sample_fail_count = 0U;
    diag->total_safe_off_fail_count = 0U;
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

    /*
     * 采样成功只能说明“本轮采样事实成功”。
     * 第一版不在这里自动恢复 runtime fault。
     */
    diag->sample_fail_count = 0U;

    if (diag->sample_success_count < 255U)
    {
        diag->sample_success_count++;
    }

#if (BQ76940_RT_FAULT_AUTO_RECOVER != 0U)
    /*
     * 调试阶段如需自动恢复，可打开宏。
     * 正式第一版建议关闭，防止故障刚恢复几次就重新导通。
     */
    if ((diag->fault_active != 0U) &&
        (diag->sample_success_count >= BQ76940_RT_SAMPLE_RECOVER_LIMIT) &&
        (diag->safe_off_done != 0U))
    {
        diag->fault_active = 0U;
        diag->safe_off_requested = 0U;
        diag->safe_off_done = 0U;
        diag->safe_off_result = 0U;
        diag->safe_off_retry_count = 0U;
        diag->safe_off_failed = 0U;

        diag->last_fault_code = BQ76940_RT_FAULT_NONE;
        diag->last_fault_stage = BQ76940_RT_STAGE_NONE;
        diag->last_ret = 0U;

        if (recovered != 0)
        {
            *recovered = 1U;
        }

        BMS_LOG_RUNTIME("[RT] recover:%d\r\n",
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

    BMS_LOG_TEST_HW_FAULT("[RT] fail:%d/%d/%d n:%d\r\n",
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

        /*
         * 第一次进入 runtime fault 时，重置 Safe-Off 重试状态。
         */
        diag->safe_off_retry_count = 0U;
        diag->safe_off_failed = 0U;

        if (enter_fault != 0)
        {
            *enter_fault = 1U;
        }

        BMS_LOG_RUNTIME("[RT] enter:%d/%d/%d n:%d\r\n",
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
                                               uint8_t safe_off_result,
                                               uint8_t *retry_allowed)
{
    BQ76940_RuntimeDiag_t *diag;

    if (retry_allowed != 0)
    {
        *retry_allowed = 0U;
    }

    if (ctx == 0)
    {
        return;
    }

    diag = &ctx->runtime_diag;

    diag->safe_off_result = safe_off_result;

    /*
     * 情况 1：Safe-Off 成功
     *
     * 成功后进入“故障保持”效果：
     * - fault_active 仍然保持 1
     * - safe_off_done = 1
     * - 不自动恢复
     */
    if (safe_off_result == 0U)
    {
        diag->safe_off_done = 1U;
        diag->safe_off_requested = 0U;
        diag->safe_off_failed = 0U;

        BMS_LOG_RUNTIME("[RT] safe-off ok\r\n");
        return;
    }

    /*
     * 情况 2：Safe-Off 失败
     *
     * 注意：
     * - 故障不能清除
     * - 记录失败次数
     * - 如果没超过重试次数，允许 RuntimeTask 再试一次
     */
    diag->safe_off_done = 0U;

    if (diag->safe_off_retry_count < 255U)
    {
        diag->safe_off_retry_count++;
    }

    if (diag->total_safe_off_fail_count < 65535U)
    {
        diag->total_safe_off_fail_count++;
    }

    diag->last_fault_code = BQ76940_RT_FAULT_SAFE_OFF_FAIL;
    diag->last_fault_stage = BQ76940_RT_STAGE_SAFE_OFF;
    diag->last_ret = safe_off_result;

    BMS_LOG_ERROR("[RT] off fail:%02X n:%d\r\n",
           safe_off_result,
           diag->safe_off_retry_count);

    /*
     * 快速重试还没超过上限，则允许 RuntimeTask 继续重试。
     */
    if (diag->safe_off_retry_count < BQ76940_RT_SAFE_OFF_RETRY_LIMIT)
    {
        diag->safe_off_requested = 1U;

        if (retry_allowed != 0)
        {
            *retry_allowed = 1U;
        }

        return;
    }

    /*
     * 超过快速重试上限：
     * - 标记 Safe-Off 失败
     * - 保持 fault_active = 1
     * - 后续可以降低频率重试，或者等待人工处理
     */
    diag->safe_off_failed = 1U;
    diag->safe_off_requested = 0U;

    BMS_LOG_ERROR("[RT] off hold\r\n");
}
