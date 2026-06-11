#ifndef __BQ76940_APP_CAN_H
#define __BQ76940_APP_CAN_H

#include <stdint.h>

struct BQ76940_AppCtx;

void BQ76940_AppSendCanTelemetry(const struct BQ76940_AppCtx *ctx);

void BQ76940_AppSendBringUpFaultCan(const struct BQ76940_AppCtx *ctx,
                                    uint8_t main_ret,
                                    uint8_t safe_off_result);

#endif
