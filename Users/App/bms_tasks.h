#ifndef BMS_TASKS_H
#define BMS_TASKS_H

#include "FreeRTOS.h"
#include "bq76940_app.h"

BaseType_t BMS_TasksCreate(BQ76940_AppCtx_t *app);

#endif
