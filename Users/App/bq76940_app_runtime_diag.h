#ifndef __BQ76940_APP_RUNTIME_DIAG_H
#define __BQ76940_APP_RUNTIME_DIAG_H

#include <stdint.h>

/*
 * RuntimeDiag V1 参数
 */
#define BQ76940_RT_SAMPLE_FAIL_LIMIT        3U
#define BQ76940_RT_SAMPLE_RECOVER_LIMIT     5U
#define BQ76940_RT_FAULT_AUTO_RECOVER       1U

/*
 * 运行时故障码
 */
#define BQ76940_RT_FAULT_NONE               0x00U
#define BQ76940_RT_FAULT_I2C_LOCK_TIMEOUT   0x01U
#define BQ76940_RT_FAULT_SAMPLE_READ        0x02U
#define BQ76940_RT_FAULT_SAMPLE_PROCESS     0x03U
#define BQ76940_RT_FAULT_SAMPLE_COMMIT      0x04U
#define BQ76940_RT_FAULT_CTX_LOCK           0x05U
#define BQ76940_RT_FAULT_SAFE_OFF_FAIL      0x06U

/*
 * 运行时故障阶段码
 */
#define BQ76940_RT_STAGE_NONE               0x00U
#define BQ76940_RT_STAGE_CALIB_SNAPSHOT     0x01U
#define BQ76940_RT_STAGE_I2C_LOCK           0x02U
#define BQ76940_RT_STAGE_SAMPLE_READ_HW     0x03U
#define BQ76940_RT_STAGE_SAMPLE_PROCESS     0x04U
#define BQ76940_RT_STAGE_SAMPLE_COMMIT      0x05U
#define BQ76940_RT_STAGE_SAFE_OFF           0x06U

/*
 * 运行时诊断状态
 */
typedef struct
{
    uint8_t fault_active;          /* 运行时故障是否锁存 */
    uint8_t safe_off_requested;    /* 是否请求 RuntimeTask 执行 Safe-Off */
    uint8_t safe_off_done;         /* Safe-Off 是否已执行 */
    uint8_t safe_off_result;       /* Safe-Off 结果 */

    uint8_t sample_fail_count;     /* 连续采样失败次数 */
    uint8_t sample_success_count;  /* 连续采样成功次数 */

    uint8_t last_fault_code;       /* 最近故障码 */
    uint8_t last_fault_stage;      /* 最近故障阶段 */
    uint8_t last_ret;              /* 最近底层返回值 */

    uint16_t total_sample_fail_count; /* 总采样失败次数 */
} BQ76940_RuntimeDiag_t;


struct BQ76940_AppCtx;

void BQ76940_AppRuntimeDiagInit(BQ76940_RuntimeDiag_t *diag);

uint8_t BQ76940_AppRuntimeDiagIsFaultActive(const struct BQ76940_AppCtx *ctx);

void BQ76940_AppRuntimeDiagRecordSampleOk(struct BQ76940_AppCtx *ctx,
                                          uint8_t *recovered);

void BQ76940_AppRuntimeDiagRecordSampleFail(struct BQ76940_AppCtx *ctx,
                                            uint8_t fault_code,
                                            uint8_t fault_stage,
                                            uint8_t ret,
                                            uint8_t *enter_fault);

void BQ76940_AppRuntimeDiagTakeSafeOffRequest(struct BQ76940_AppCtx *ctx,
                                              uint8_t *need_safe_off);

void BQ76940_AppRuntimeDiagCommitSafeOffResult(struct BQ76940_AppCtx *ctx,
                                               uint8_t safe_off_result);

#endif

