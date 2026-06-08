#ifndef __BQ34Z100_APP_H
#define __BQ34Z100_APP_H

#include "stdint.h"

typedef struct
{
    uint8_t  soc_percent;
    uint8_t  max_error_percent;

    uint16_t remaining_capacity_mAh;
    uint16_t full_charge_capacity_mAh;

    uint16_t voltage_mV;

    int16_t  average_current_mA;
    int16_t  current_mA;

    int16_t  temperature_dC;

    uint16_t flags;
    uint16_t flags_b;

    uint16_t cycle_count;
    uint8_t  soh_percent;

    uint8_t  data_valid;
    uint8_t  last_error;
} BQ34Z100_AppCtx_t;

void BQ34Z100_AppInit(BQ34Z100_AppCtx_t *ctx);
uint8_t BQ34Z100_AppRunCycle(BQ34Z100_AppCtx_t *ctx);
void BQ34Z100_AppPrint(const BQ34Z100_AppCtx_t *ctx);

#endif
