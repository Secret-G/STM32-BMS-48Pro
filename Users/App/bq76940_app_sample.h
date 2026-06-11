#ifndef __BQ76940_APP_SAMPLE_H
#define __BQ76940_APP_SAMPLE_H

#include "bq76940_drv.h"

/*
 * BQ76940 采样快照数据
 *
 * 作用：
 *   用于 FreeRTOS 任务中实现“先读硬件，再提交全局状态”的结构。
 *
 * 设计目的：
 *   1. I2C 锁只保护硬件读取过程
 *   2. ctx 锁只保护 app 全局结构体更新
 *   3. 避免拿着全局锁长时间访问 I2C
 */
typedef struct
{
    uint16_t cell_raw[BQ76940_CELL_COUNT_9];
    uint16_t cell_mV[BQ76940_CELL_COUNT_9];

    uint32_t pack_total_mV;
    BQ76940_CellStats9_t cell_stats;

    BQ76940_CCRaw_t cc_raw;
    int32_t pack_current_mA;
    int8_t pack_current_dir;

    uint16_t ts1_raw_adc;
    int16_t ts1_temp_dC;

    uint8_t sys_stat;
    uint8_t fault_mask_active;
} BQ76940_AppSampleData_t;




struct BQ76940_AppCtx;

/*
 * 只读取 BQ76940 硬件数据。
 * 该函数内部会访问 I2C，总线锁应由上层任务持有。
 */
uint8_t BQ76940_AppSampleReadHw(const BQ76940_AdcCalib_t *calib,
                                BQ76940_AppSampleData_t *sample);

/*
 * 对采样快照进行计算处理。
 * 该函数不访问 I2C，也不修改全局 app。
 */
uint8_t BQ76940_AppSampleProcess(BQ76940_AppSampleData_t *sample);

/*
 * 将采样快照提交到 app 全局上下文。
 * 该函数应在持有 ctx mutex 时调用。
 */
uint8_t BQ76940_AppSampleCommit(struct BQ76940_AppCtx *ctx,
                                const BQ76940_AppSampleData_t *sample);


uint8_t BQ76940_AppSampleUpdate(struct BQ76940_AppCtx *ctx);

#endif
