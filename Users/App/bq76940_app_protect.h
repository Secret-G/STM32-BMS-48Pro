#ifndef __BQ76940_APP_PROTECT_H
#define __BQ76940_APP_PROTECT_H

#include <stdint.h>

#define BQ76940_PROTECT_DBG_ENABLE          0U
#define BQ76940_PROTECT_EVENT_PRINT_ENABLE  1U

/*
 * OCD/SCD 保护动作类型
 */
#define BQ76940_OCDSCD_ACTION_NONE        0U
#define BQ76940_OCDSCD_ACTION_DSG_OFF     1U
#define BQ76940_OCDSCD_ACTION_DSG_ON      2U

/*
 * OCD/SCD 保护请求
 *
 * 作用：
 *   将 OCD/SCD 保护拆成三段：
 *   1. Decide：根据 ctx 当前状态生成请求
 *   2. ApplyHw：根据请求写 BQ76940 DSG 位
 *   3. Commit：将执行结果提交回 ctx
 */
typedef struct
{
    uint8_t action;

    uint8_t sys_stat_snapshot;
    uint8_t hw_fault_now;

    uint8_t ocd_now;
    uint8_t scd_now;

    uint8_t recover_request;
} BQ76940_OcdScdRequest_t;


/*
 * OT 过温保护动作类型
 */
#define BQ76940_OT_ACTION_NONE        0U
#define BQ76940_OT_ACTION_FET_OFF     1U
#define BQ76940_OT_ACTION_FET_ON      2U

/*
 * OT 过温保护请求
 *
 * 作用：
 *   将 OT 过温保护拆成三段：
 *   1. Decide：根据 app 当前状态判断是否需要动作
 *   2. ApplyHw：根据请求写 BQ76940 CHG / DSG
 *   3. Commit：将动作结果提交回 app
 */
typedef struct
{
    uint8_t action;

    uint8_t ot_now;
    uint8_t ov_now;
    uint8_t uv_now;

    uint8_t ot_cutoff_active_snapshot;
} BQ76940_OtProtectRequest_t;

/*
 * UT 低温保护动作类型
 */
#define BQ76940_UT_ACTION_NONE        0U
#define BQ76940_UT_ACTION_CHG_OFF     1U
#define BQ76940_UT_ACTION_CHG_ON      2U

/*
 * UT 低温保护请求
 *
 * 作用：
 *   将 UT 低温保护拆成三段：
 *   1. Decide：根据 app 当前状态判断是否需要动作
 *   2. ApplyHw：根据请求写 BQ76940 CHG 位
 *   3. Commit：将动作结果提交回 app
 */
typedef struct
{
    uint8_t action;

    uint8_t ut_now;
    uint8_t ov_now;
    uint8_t ot_now;

    uint8_t ot_cutoff_active_snapshot;
    uint8_t ut_chg_block_active_snapshot;
} BQ76940_UtProtectRequest_t;



struct BQ76940_AppCtx;

/*
 * 保护基础更新：
 *   1. UV / OV / DIFF 软件告警
 *   2. OT 过温告警
 *   3. UT 低温告警
 *   4. OT 联动保护
 *   5. UT 联动保护
 *
 * 注意：
 *   不包含 OCD / SCD 处理。
 *   OCD / SCD 在 FreeRTOS ProtectTask 中走三段式：
 *   Decide -> ApplyHw -> Commit。
 */
uint8_t BQ76940_AppProtectUpdateBase(struct BQ76940_AppCtx *ctx);

/*ocd csd相关*/
void BQ76940_AppOcdScdRequestClear(BQ76940_OcdScdRequest_t *req);

uint8_t BQ76940_AppOcdScdDecide(const struct BQ76940_AppCtx *ctx,
                                BQ76940_OcdScdRequest_t *req);

uint8_t BQ76940_AppOcdScdApplyHw(const BQ76940_OcdScdRequest_t *req);

uint8_t BQ76940_AppOcdScdCommit(struct BQ76940_AppCtx *ctx,
                                const BQ76940_OcdScdRequest_t *req);


//过温保护的相关函数
void BQ76940_AppOtProtectRequestClear(BQ76940_OtProtectRequest_t *req);

uint8_t BQ76940_AppOtProtectDecide(const struct BQ76940_AppCtx *ctx,
                                    BQ76940_OtProtectRequest_t *req);

uint8_t BQ76940_AppOtProtectApplyHw(const BQ76940_OtProtectRequest_t *req);

uint8_t BQ76940_AppOtProtectCommit(struct BQ76940_AppCtx *ctx,
                                    const BQ76940_OtProtectRequest_t *req);

//低温保护的相关函数
void BQ76940_AppUtProtectRequestClear(BQ76940_UtProtectRequest_t *req);

uint8_t BQ76940_AppUtProtectDecide(const struct BQ76940_AppCtx *ctx,
                                    BQ76940_UtProtectRequest_t *req);

uint8_t BQ76940_AppUtProtectApplyHw(const BQ76940_UtProtectRequest_t *req);

uint8_t BQ76940_AppUtProtectCommit(struct BQ76940_AppCtx *ctx,
                                    const BQ76940_UtProtectRequest_t *req);


uint8_t BQ76940_AppProtectUpdate(struct BQ76940_AppCtx *ctx);
/*
 * 只更新保护相关告警状态：
 *   UV / OV / DIFF
 *   OT / UT
 *
 * 注意：
 *   本函数不执行 CHG / DSG / CELLBAL 等硬件动作。
 *   不访问 I2C。
 */
uint8_t BQ76940_AppProtectUpdateAlarms(struct BQ76940_AppCtx *ctx);


#endif
