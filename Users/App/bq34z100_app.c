#include "bq34z100_app.h"
#include "bq34z100_drv.h"
#include "stdio.h"
#include "bms_log.h"

void BQ34Z100_AppInit(BQ34Z100_AppCtx_t *ctx)
{
    if (ctx == 0)
    {
        return;
    }

    ctx->soc_percent = 0U;
    ctx->max_error_percent = 0U;

    ctx->remaining_capacity_mAh = 0U;
    ctx->full_charge_capacity_mAh = 0U;

    ctx->voltage_mV = 0U;

    ctx->average_current_mA = 0;
    ctx->current_mA = 0;

    ctx->temperature_dC = 0;

    ctx->flags = 0U;
    ctx->flags_b = 0U;

    ctx->cycle_count = 0U;
    ctx->soh_percent = 0U;

    ctx->data_valid = 0U;
    ctx->last_error = BQ34Z100_APP_ERR_NOT_READY;
}

static uint8_t BQ34Z100_AppReadSample(BQ34Z100_AppCtx_t *ctx)
{
    uint8_t ret;

    if (ctx == 0)
    {
        return 1U;
    }

    ret = BQ34Z100_ReadSOC(&ctx->soc_percent);
    if (ret != BQ34Z100_OK)
    {
        ctx->data_valid = 0U;
        ctx->last_error = 10U;
        return 10U;
    }

    ret = BQ34Z100_ReadMaxError(&ctx->max_error_percent);
    if (ret != BQ34Z100_OK)
    {
        ctx->data_valid = 0U;
        ctx->last_error = 11U;
        return 11U;
    }

    ret = BQ34Z100_ReadRemainingCapacity_mAh(&ctx->remaining_capacity_mAh);
    if (ret != BQ34Z100_OK)
    {
        ctx->data_valid = 0U;
        ctx->last_error = 12U;
        return 12U;
    }

    ret = BQ34Z100_ReadFullChargeCapacity_mAh(&ctx->full_charge_capacity_mAh);
    if (ret != BQ34Z100_OK)
    {
        ctx->data_valid = 0U;
        ctx->last_error = 13U;
        return 13U;
    }

    ret = BQ34Z100_ReadVoltage_mV(&ctx->voltage_mV);
    if (ret != BQ34Z100_OK)
    {
        ctx->data_valid = 0U;
        ctx->last_error = 14U;
        return 14U;
    }

    ret = BQ34Z100_ReadAverageCurrent_mA(&ctx->average_current_mA);
    if (ret != BQ34Z100_OK)
    {
        ctx->data_valid = 0U;
        ctx->last_error = 15U;
        return 15U;
    }

    ret = BQ34Z100_ReadCurrent_mA(&ctx->current_mA);
    if (ret != BQ34Z100_OK)
    {
        ctx->data_valid = 0U;
        ctx->last_error = 16U;
        return 16U;
    }

    ret = BQ34Z100_ReadTemperature_dC(&ctx->temperature_dC);
    if (ret != BQ34Z100_OK)
    {
        ctx->data_valid = 0U;
        ctx->last_error = 17U;
        return 17U;
    }

    ret = BQ34Z100_ReadFlags(&ctx->flags);
    if (ret != BQ34Z100_OK)
    {
        ctx->data_valid = 0U;
        ctx->last_error = 18U;
        return 18U;
    }

    ret = BQ34Z100_ReadFlagsB(&ctx->flags_b);
    if (ret != BQ34Z100_OK)
    {
        ctx->data_valid = 0U;
        ctx->last_error = 19U;
        return 19U;
    }

    ret = BQ34Z100_ReadCycleCount(&ctx->cycle_count);
    if (ret != BQ34Z100_OK)
    {
        ctx->data_valid = 0U;
        ctx->last_error = 20U;
        return 20U;
    }

    ret = BQ34Z100_ReadSOH(&ctx->soh_percent);
    if (ret != BQ34Z100_OK)
    {
        ctx->data_valid = 0U;
        ctx->last_error = 21U;
        return 21U;
    }

    if ((ctx->soc_percent > 100U) || (ctx->soh_percent > 100U) ||
        (ctx->max_error_percent > 100U))
    {
        ctx->data_valid = 0U;
        ctx->last_error = BQ34Z100_APP_ERR_RANGE;
        return ctx->last_error;
    }

    ctx->data_valid = 1U;
    ctx->last_error = 0U;

    return 0U;
}

/* Publish a complete sample only; failed reads retain the last numeric values. */
uint8_t BQ34Z100_AppRunCycle(BQ34Z100_AppCtx_t *ctx)
{
    BQ34Z100_AppCtx_t sample;
    uint8_t ret;

    if (ctx == 0) return 1U;
    BQ34Z100_AppInit(&sample);
    ret = BQ34Z100_AppReadSample(&sample);
    if (ret == 0U)
    {
        *ctx = sample;
    }
    else
    {
        ctx->data_valid = 0U;
        ctx->last_error = ret;
    }
    return ret;
}

/* CAN 0x308 has a self-contained validity flag: no cross-frame assembly needed. */
uint8_t BQ34Z100_AppBuildCanPayload(const BQ34Z100_AppCtx_t *ctx, uint8_t data[8])
{
    uint8_t i;
    if ((ctx == 0) || (data == 0)) return 1U;
    for (i = 0U; i < 8U; i++) data[i] = 0U;
    if (ctx->data_valid != 0U)
    {
        data[0] = ctx->soc_percent;
        data[1] = ctx->soh_percent;
        data[2] = (uint8_t)ctx->remaining_capacity_mAh;
        data[3] = (uint8_t)(ctx->remaining_capacity_mAh >> 8);
        data[4] = (uint8_t)ctx->full_charge_capacity_mAh;
        data[5] = (uint8_t)(ctx->full_charge_capacity_mAh >> 8);
        data[6] = 1U;
    }
    data[7] = ctx->last_error;
    return 0U;
}

void BQ34Z100_AppPrint(const BQ34Z100_AppCtx_t *ctx)
{
    if (ctx == 0)
    {
        return;
    }

    BMS_LOG_PERIODIC("------------------------------------------------------\r\n");
    BMS_LOG_PERIODIC("[BQ34Z100-G1]\r\n");
    BMS_LOG_PERIODIC("DATA_VALID     = %d\r\n", ctx->data_valid);
    BMS_LOG_PERIODIC("LAST_ERROR     = %d\r\n", ctx->last_error);

    BMS_LOG_PERIODIC("SOC            = %d %%\r\n", ctx->soc_percent);
    BMS_LOG_PERIODIC("MAX_ERROR      = %d %%\r\n", ctx->max_error_percent);

    BMS_LOG_PERIODIC("RM             = %d mAh\r\n", ctx->remaining_capacity_mAh);
    BMS_LOG_PERIODIC("FCC            = %d mAh\r\n", ctx->full_charge_capacity_mAh);

    BMS_LOG_PERIODIC("VOLTAGE        = %d mV\r\n", ctx->voltage_mV);

    BMS_LOG_PERIODIC("AVG_CURRENT    = %d mA\r\n", ctx->average_current_mA);
    BMS_LOG_PERIODIC("CURRENT        = %d mA\r\n", ctx->current_mA);

    BMS_LOG_PERIODIC("TEMP           = %d.%d C\r\n",
           ctx->temperature_dC / 10,
           (ctx->temperature_dC >= 0) ?
           (ctx->temperature_dC % 10) :
           (-(ctx->temperature_dC % 10)));

    BMS_LOG_PERIODIC("FLAGS          = 0x%04X\r\n", ctx->flags);
    BMS_LOG_PERIODIC("FLAGS_B        = 0x%04X\r\n", ctx->flags_b);

    BMS_LOG_PERIODIC("CYCLE_COUNT    = %d\r\n", ctx->cycle_count);
    BMS_LOG_PERIODIC("SOH            = %d %%\r\n", ctx->soh_percent);
    BMS_LOG_PERIODIC("------------------------------------------------------\r\n");
}
