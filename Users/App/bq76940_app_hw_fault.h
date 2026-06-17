#ifndef __BQ76940_APP_HW_FAULT_H
#define __BQ76940_APP_HW_FAULT_H

#include <stdint.h>




/*
 * OCD/SCD 保护动作类型
 */
#define BQ76940_OCDSCD_ACTION_NONE        0U
#define BQ76940_OCDSCD_ACTION_DSG_OFF     1U
#define BQ76940_OCDSCD_ACTION_DSG_ON      2U


/*
 * 故障码
 */
#define BQ76940_HW_FAULT_CODE_NONE       0U
#define BQ76940_HW_FAULT_CODE_OCD        1U
#define BQ76940_HW_FAULT_CODE_SCD        2U
#define BQ76940_HW_FAULT_CODE_OCD_SCD    3U

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
    uint8_t fault_code;
    uint8_t apply_ret;

    uint8_t ocd_now;
    uint8_t scd_now;

    uint8_t recover_request;
} BQ76940_OcdScdRequest_t;



struct BQ76940_AppCtx;

/*ocd csd相关*/
void BQ76940_AppOcdScdRequestClear(BQ76940_OcdScdRequest_t *req);

uint8_t BQ76940_AppOcdScdDecide(const struct BQ76940_AppCtx *ctx,
                                BQ76940_OcdScdRequest_t *req);

uint8_t BQ76940_AppOcdScdApplyHw(const BQ76940_OcdScdRequest_t *req);

uint8_t BQ76940_AppOcdScdCommit(struct BQ76940_AppCtx *ctx,
                                const BQ76940_OcdScdRequest_t *req);
uint8_t BQ76940_AppHandleOcdScdProtect(struct BQ76940_AppCtx *ctx);

#endif
