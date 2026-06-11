#ifndef __BQ76200_EXEC_H
#define __BQ76200_EXEC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ==============================
 * BQ76200 执行层状态
 * ============================== */
typedef enum
{
    BQ76200_EXEC_STATE_OFF = 0,        /* 全部关闭 */
    BQ76200_EXEC_STATE_PRECHARGE,      /* 预充状态 */
    BQ76200_EXEC_STATE_NORMAL_ON,      /* 正常充放电开启 */
    BQ76200_EXEC_STATE_CHG_BLOCK,      /* 禁止充电，允许放电 */
    BQ76200_EXEC_STATE_DSG_BLOCK,      /* 允许充电，禁止放电 */
    BQ76200_EXEC_STATE_CHG_DSG_BLOCK   /* 充放电都禁止 */
} BQ76200_ExecState_t;


/* ==============================
 * BQ76200 执行层输入
 * 来自 bq76940_app
 * ============================== */
typedef struct
{
    uint8_t ot_cutoff_active;       /* 过温：1=充放都关 */
    uint8_t ut_chg_block_active;    /* 低温：1=只禁止充电 */
    uint8_t hw_dsg_block_active;    /* OCD/SCD：1=只禁止放电 */
    uint8_t runtime_fault_active;   /* 运行时故障：1=全部关闭 */
} BQ76200_ExecInput_t;


/* ==============================
 * BQ76200 执行层上下文
 * ============================== */
typedef struct
{
    BQ76200_ExecState_t state;
    BQ76200_ExecState_t last_state;
} BQ76200_ExecCtx_t;


/* 初始化执行层 */
uint8_t BQ76200_ExecInit(BQ76200_ExecCtx_t *ctx);

/* 根据输入更新执行层状态 */
uint8_t BQ76200_ExecUpdate(BQ76200_ExecCtx_t *ctx,
                           const BQ76200_ExecInput_t *input);

/* 获取当前状态 */
BQ76200_ExecState_t BQ76200_ExecGetState(const BQ76200_ExecCtx_t *ctx);

/* 状态转字符串，方便 printf */
const char *BQ76200_ExecStateToString(BQ76200_ExecState_t state);

/* 打印当前执行层状态 */
void BQ76200_ExecPrintState(const BQ76200_ExecCtx_t *ctx);


/* 强制进入安全关闭状态：CHG/DSG/CP/PCHG 全部关闭 */
void BQ76200_ExecForceOff(BQ76200_ExecCtx_t *ctx);


#ifdef __cplusplus
}
#endif

#endif

