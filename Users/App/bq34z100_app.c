#include "bq34z100_app.h"
#include "bq34z100_drv.h"
#include "stdio.h"

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
    ctx->last_error = 0U;
}

uint8_t BQ34Z100_AppRunCycle(BQ34Z100_AppCtx_t *ctx)
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

    ctx->data_valid = 1U;
    ctx->last_error = 0U;

    return 0U;
}

void BQ34Z100_AppPrint(const BQ34Z100_AppCtx_t *ctx)
{
    if (ctx == 0)
    {
        return;
    }

    printf("------------------------------------------------------\r\n");
    printf("[BQ34Z100-G1]\r\n");
    printf("DATA_VALID     = %d\r\n", ctx->data_valid);
    printf("LAST_ERROR     = %d\r\n", ctx->last_error);

    printf("SOC            = %d %%\r\n", ctx->soc_percent);
    printf("MAX_ERROR      = %d %%\r\n", ctx->max_error_percent);

    printf("RM             = %d mAh\r\n", ctx->remaining_capacity_mAh);
    printf("FCC            = %d mAh\r\n", ctx->full_charge_capacity_mAh);

    printf("VOLTAGE        = %d mV\r\n", ctx->voltage_mV);

    printf("AVG_CURRENT    = %d mA\r\n", ctx->average_current_mA);
    printf("CURRENT        = %d mA\r\n", ctx->current_mA);

    printf("TEMP           = %d.%d C\r\n",
           ctx->temperature_dC / 10,
           (ctx->temperature_dC >= 0) ?
           (ctx->temperature_dC % 10) :
           (-(ctx->temperature_dC % 10)));

    printf("FLAGS          = 0x%04X\r\n", ctx->flags);
    printf("FLAGS_B        = 0x%04X\r\n", ctx->flags_b);

    printf("CYCLE_COUNT    = %d\r\n", ctx->cycle_count);
    printf("SOH            = %d %%\r\n", ctx->soh_percent);
    printf("------------------------------------------------------\r\n");
}
