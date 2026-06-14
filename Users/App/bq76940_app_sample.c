#include "bq76940_app.h"
#include "bms_log.h"
#include "bq76940_app_sample.h"

#include "stdio.h"
#include "string.h"

/* 先按 4mΩ 试算 */
#define BQ76940_RSENSE_UOHM   4000U

/* 近零死区，避免 +1/-1 这种抖动误判
 * 当前先按 50mA 作为经验死区
 */
#define BQ76940_CURRENT_ZERO_DEADBAND_mA   50


static int8_t BQ76940_AppJudgeCurrentDir(int32_t current_mA)
{
    if (current_mA >= BQ76940_CURRENT_ZERO_DEADBAND_mA)
    {
        return 1;
    }
    else if (current_mA <= -BQ76940_CURRENT_ZERO_DEADBAND_mA)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

uint8_t BQ76940_AppSampleReadHw(const BQ76940_AdcCalib_t *calib,
                                BQ76940_AppSampleData_t *sample)
{
    uint8_t ret;

    if ((calib == 0) || (sample == 0))
    {
        return 1U;
    }

    /*
     * 1. 读取 9 节映射电压。
     *
     * 注意：
     *   该函数内部会通过 I2C 访问 BQ76940。
     *   因此调用本函数前，上层任务应已经拿到 I2C 总线互斥锁。
     */
    ret = BQ76940_ReadAllMappedCellVoltages9_mV(calib,
                                                sample->cell_raw,
                                                sample->cell_mV);
    if (ret != 0U)
    {
        return 11U;
    }

    /*
     * 2. 触发一次 CC 1-shot 电流采样。
     */
    ret = BQ76940_CC_StartOneShot();
    if (ret != 0U)
    {
        BMS_LOG_ERROR("[SMP] CC start:%d\r\n", ret);
        return 14U;
    }

    /*
     * 3. 等待 CC_READY。
     *
     * 当前版本保持原来的逻辑：
     *   等待期间仍然认为属于 BQ76940 电流采样事务的一部分。
     */
    ret = BQ76940_CC_WaitReady(600U);
    if (ret != 0U)
    {
        BMS_LOG_ERROR("[SMP] CC wait:%d\r\n", ret);
        return 15U;
    }

    /*
     * 4. 读取 CC 原始值。
     */
    ret = BQ76940_CC_ReadRaw(&sample->cc_raw);
    if (ret != 0U)
    {
        BMS_LOG_ERROR("[SMP] CC read:%d\r\n", ret);
        return 16U;
    }

    /*
     * 5. 读取 TS1 原始 ADC。
     */
    ret = BQ76940_ReadTS1Raw(&sample->ts1_raw_adc);
    if (ret != 0U)
    {
        BMS_LOG_ERROR("[SMP] TS1 read:%d\r\n", ret);
        return 18U;
    }

    /*
     * 6. 读取 BQ76940 硬件故障状态 SYS_STAT。
     */
    ret = BQ76940_ProtectReadFaultStatus(&sample->sys_stat);
    if (ret != 0U)
    {
        BMS_LOG_ERROR("[SMP] SYS read:%d\r\n", ret);
        return 23U;
    }

    return 0U;
}



uint8_t BQ76940_AppSampleProcess(BQ76940_AppSampleData_t *sample)
{
    uint8_t ret;

    if (sample == 0)
    {
        return 1U;
    }

    /*
     * 1. 计算 Pack 总压。
     * 这一步只依赖已经读取到的 cell_mV，不访问 I2C。
     */
    sample->pack_total_mV = BQ76940_CalcPackVoltage9_mV(sample->cell_mV);

    /*
     * 2. 统计最高单体、最低单体、压差。
     */
    ret = BQ76940_AnalyzeCellVoltages9(sample->cell_mV,
                                       &sample->cell_stats);
    if (ret != 0U)
    {
        return 12U;
    }

    /*
     * 3. CC 原始值换算为 Pack 电流。
     */
    ret = BQ76940_CC_ConvertToCurrent_mA(sample->cc_raw.raw_s16,
                                         BQ76940_RSENSE_UOHM,
                                         &sample->pack_current_mA);
    if (ret != 0U)
    {
        BMS_LOG_ERROR("[SMP] CC conv:%d\r\n", ret);
        return 17U;
    }

    /*
     * 4. 判断电流方向：充电 / 放电 / 近零。
     */
    sample->pack_current_dir =
        BQ76940_AppJudgeCurrentDir(sample->pack_current_mA);

    /*
     * 5. TS1 原始 ADC 换算为温度。
     */
    ret = BQ76940_ConvertTS1Temp_dC(sample->ts1_raw_adc,
                                    &sample->ts1_temp_dC);
    if (ret != 0U)
    {
        BMS_LOG_ERROR("[SMP] TS1 conv:%d\r\n", ret);
        return 19U;
    }

    /*
     * 6. 从 SYS_STAT 中提取当前激活的硬件故障位。
     */
    ret = BQ76940_ProtectGetActiveFaultMask(sample->sys_stat,
                                            &sample->fault_mask_active);
    if (ret != 0U)
    {
        BMS_LOG_ERROR("[SMP] SYS mask:%d\r\n", ret);
        return 24U;
    }

    return 0U;
}


uint8_t BQ76940_AppSampleCommit(BQ76940_AppCtx_t *ctx,
                                const BQ76940_AppSampleData_t *sample)
{
    if ((ctx == 0) || (sample == 0))
    {
        return 1U;
    }

    /*
     * 将局部采样快照提交到全局 app。
     *
     * 注意：
     *   该函数会修改 BQ76940_AppCtx_t，
     *   因此在 FreeRTOS 任务中调用时，应持有 g_bms_ctx_mutex。
     */
    memcpy(ctx->cell_raw,
           sample->cell_raw,
           sizeof(ctx->cell_raw));

    memcpy(ctx->cell_mV,
           sample->cell_mV,
           sizeof(ctx->cell_mV));

    ctx->pack_total_mV = sample->pack_total_mV;
    ctx->cell_stats    = sample->cell_stats;

    ctx->cc_raw           = sample->cc_raw;
    ctx->pack_current_mA  = sample->pack_current_mA;
    ctx->pack_current_dir = sample->pack_current_dir;

    ctx->ts1_raw_adc = sample->ts1_raw_adc;
    ctx->ts1_temp_dC = sample->ts1_temp_dC;

    ctx->sys_stat          = sample->sys_stat;
    ctx->fault_mask_active = sample->fault_mask_active;

    return 0U;
}


uint8_t BQ76940_AppSampleUpdate(BQ76940_AppCtx_t *ctx)
{
    uint8_t ret;
    BQ76940_AppSampleData_t sample;

    if (ctx == 0)
    {
        return 1U;
    }

    /*
     * 兼容旧接口：
     *   1. 读取硬件数据
     *   2. 处理采样数据
     *   3. 提交到 app
     *
     * 注意：
     *   该函数本身不负责加锁。
     *   在 FreeRTOS 任务中，推荐直接调用
     *   ReadHw / Process / Commit 三段式接口。
     */
    ret = BQ76940_AppSampleReadHw(&ctx->calib, &sample);
    if (ret != 0U)
    {
        return ret;
    }

    ret = BQ76940_AppSampleProcess(&sample);
    if (ret != 0U)
    {
        return ret;
    }

    ret = BQ76940_AppSampleCommit(ctx, &sample);
    if (ret != 0U)
    {
        return ret;
    }

    return 0U;
}


