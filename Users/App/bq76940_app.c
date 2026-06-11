#include "bq76940_app.h"
#include "bq76940_print.h"

#include "bq76200_exec.h"
#include "can_drv.h"
#include "io_ctrl.h"
#include "delay.h"
#include "stdio.h"
#include "string.h"


/* 先按 4mΩ 试算 */
#define BQ76940_RSENSE_UOHM   4000U

/* 近零死区，避免 +1/-1 这种抖动误判
 * 当前先按 50mA 作为经验死区
 */
#define BQ76940_CURRENT_ZERO_DEADBAND_mA   50


/* BQ76940 上电 bring-up 最大尝试次数 */
#define BQ76940_APP_BRINGUP_RETRY_LIMIT     3U

/* BQ76940 bring-up 阶段码 */
#define BQ76940_BRINGUP_STAGE_WAKE          1U
#define BQ76940_BRINGUP_STAGE_BASIC         2U
#define BQ76940_BRINGUP_STAGE_HW_CONFIG     3U
#define BQ76940_BRINGUP_STAGE_STATUS        4U
#define BQ76940_BRINGUP_STAGE_OCDSCD        5U
#define BQ76940_BRINGUP_STAGE_SAMPLE        6U




#define BQ76940_PROTECT_DBG_ENABLE          0U
#define BQ76940_PROTECT_EVENT_PRINT_ENABLE  1U

static int8_t BQ76940_AppJudgeCurrentDir(int32_t current_mA)
{
    if (current_mA >= BQ76940_CURRENT_ZERO_DEADBAND_mA)
    {
        return 1;
    }
    else if (current_mA <= -BQ76940_CURRENT_ZERO_DEADBAND_mA)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

static void BQ76940_AppPackU16LE(uint8_t *buf, uint8_t offset, uint16_t value)
{
    buf[offset] = (uint8_t)(value & 0xFFU);
    buf[offset + 1U] = (uint8_t)((value >> 8U) & 0xFFU);
}

static void BQ76940_AppPackU32LE(uint8_t *buf, uint8_t offset, uint32_t value)
{
    buf[offset] = (uint8_t)(value & 0xFFU);
    buf[offset + 1U] = (uint8_t)((value >> 8U) & 0xFFU);
    buf[offset + 2U] = (uint8_t)((value >> 16U) & 0xFFU);
    buf[offset + 3U] = (uint8_t)((value >> 24U) & 0xFFU);
}

static void BQ76940_AppPackI32LE(uint8_t *buf, uint8_t offset, int32_t value)
{
    BQ76940_AppPackU32LE(buf, offset, (uint32_t)value);
}

static uint8_t BQ76940_AppBuildProtectFlags(const BQ76940_AppCtx_t *ctx)
{
    uint8_t flags = 0U;

    if (ctx->ot_cutoff_active != 0U) flags |= 0x01U;
    if (ctx->ut_chg_block_active != 0U) flags |= 0x02U;
    if (ctx->hw_dsg_block_active != 0U) flags |= 0x04U;
    if (ctx->hw_ocd_active != 0U) flags |= 0x08U;
    if (ctx->hw_scd_active != 0U) flags |= 0x10U;
    if (ctx->bal_active != 0U) flags |= 0x20U;

    return flags;
}

static uint8_t BQ76940_AppBuildAlarmFlags(const BQ76940_AppCtx_t *ctx)
{
    uint8_t flags = 0U;

    if (ctx->alarm_state.uv_flag != 0U) flags |= 0x01U;
    if (ctx->alarm_state.ov_flag != 0U) flags |= 0x02U;
    if (ctx->alarm_state.diff_flag != 0U) flags |= 0x04U;
    if (ctx->alarm_state.ot_flag != 0U) flags |= 0x08U;
    if (ctx->alarm_state.ut_flag != 0U) flags |= 0x10U;

    return flags;
}

/*
 * 将 BQ76940 应用层状态打包为 CAN 状态帧并发送。
 *
 * 当前 CAN 协议：
 * 0x301: Pack 总压 + Pack 电流
 * 0x302: Cell1 ~ Cell4 电压
 * 0x303: Cell5 ~ Cell8 电压
 * 0x304: Cell9 电压 + TS1 温度 + 告警/保护/均衡状态
 *
 * 注意：
 * 本函数只负责业务数据打包和调用 CAN_DrvSendStd()，
 * CAN 底层初始化、邮箱发送等细节由 can_drv.c 负责。
 */

void BQ76940_AppSendCanTelemetry(const BQ76940_AppCtx_t *ctx)
{
    uint8_t data[8];
    uint8_t ret;

    if ((ctx == 0) || (CAN_DrvIsReady() == 0U))
    {
        return;
    }

    BQ76940_AppPackU32LE(data, 0, ctx->pack_total_mV);
    BQ76940_AppPackI32LE(data, 4, ctx->pack_current_mA);
    ret = CAN_DrvSendStd(CAN_ID_BMS_PACK_STATUS, data, 8);
    if (ret != 0U) return;

    BQ76940_AppPackU16LE(data, 0, ctx->cell_mV[0]);
    BQ76940_AppPackU16LE(data, 2, ctx->cell_mV[1]);
    BQ76940_AppPackU16LE(data, 4, ctx->cell_mV[2]);
    BQ76940_AppPackU16LE(data, 6, ctx->cell_mV[3]);
    ret = CAN_DrvSendStd(CAN_ID_BMS_CELL_1_4, data, 8);
    if (ret != 0U) return;

    BQ76940_AppPackU16LE(data, 0, ctx->cell_mV[4]);
    BQ76940_AppPackU16LE(data, 2, ctx->cell_mV[5]);
    BQ76940_AppPackU16LE(data, 4, ctx->cell_mV[6]);
    BQ76940_AppPackU16LE(data, 6, ctx->cell_mV[7]);
    ret = CAN_DrvSendStd(CAN_ID_BMS_CELL_5_8, data, 8);
    if (ret != 0U) return;

    BQ76940_AppPackU16LE(data, 0, ctx->cell_mV[8]);
    BQ76940_AppPackU16LE(data, 2, (uint16_t)ctx->ts1_temp_dC);
    data[4] = BQ76940_AppBuildAlarmFlags(ctx);
    data[5] = BQ76940_AppBuildProtectFlags(ctx);
    data[6] = ctx->bal_target_label;
    data[7] = (uint8_t)ctx->pack_current_dir;
    (void)CAN_DrvSendStd(CAN_ID_BMS_CELL_9_STATUS, data, 8);
}


void BQ76940_AppSendBringUpFaultCan(const BQ76940_AppCtx_t *ctx,
                                    uint8_t main_ret,
                                    uint8_t safe_off_result)
{
    uint8_t data[8];
    uint8_t fault_flags = 0U;

    if ((ctx == 0) || (CAN_DrvIsReady() == 0U))
    {
        return;
    }

    if (ctx->diag_state.bringup_fault_active != 0U)
    {
        fault_flags |= 0x01U;
    }

    if (safe_off_result != 0U)
    {
        fault_flags |= 0x02U;
    }

    data[0] = 0x01U;                                  /* BQ76940 bring-up fault */
    data[1] = main_ret;                               /* main error code */
    data[2] = ctx->diag_state.bringup_last_stage;     /* failed stage */
    data[3] = ctx->diag_state.bringup_last_error;     /* low-level error */
    data[4] = ctx->diag_state.bringup_attempt_count;  /* retry count */
    data[5] = safe_off_result;                        /* safe-off result */
    data[6] = fault_flags;                            /* flags */
    data[7] = 0U;                                     /* reserved */

    (void)CAN_DrvSendStd(CAN_ID_BMS_FAULT_STATUS, data, 8U);
}


void BQ76940_AppInitDefaultConfig(BQ76940_AppCtx_t *ctx)
{
    if (ctx == 0)
    {
        return;
    }

    memset(ctx, 0, sizeof(BQ76940_AppCtx_t));

    /* 软件告警实验阈值 */
    ctx->alarm_th.uv_enter_mV   = 3200;
    ctx->alarm_th.uv_exit_mV    = 3300;
    ctx->alarm_th.ov_enter_mV   = 4180;
    ctx->alarm_th.ov_exit_mV    = 4130;
    ctx->alarm_th.diff_enter_mV = 150;
    ctx->alarm_th.diff_exit_mV  = 120;
		
		ctx->alarm_th.ot_enter_dC = 600;   /* 60.0°C 进入过温告警 */
		ctx->alarm_th.ot_exit_dC  = 550;   /* 55.0°C 退出过温告警 */
		
		ctx->alarm_th.ut_enter_dC = 200;   /* 20.0°C 进入低温告警 */
		ctx->alarm_th.ut_exit_dC  = 230;   /* 23.0°C 退出低温告警 */
		
		ctx->ot_cutoff_active = 0;
		
		ctx->ut_chg_block_active = 0;

    /* 硬件保护真实配置 */
    ctx->hw_cfg.ov_target_mV = 4200;
    ctx->hw_cfg.uv_target_mV = 3000;
    ctx->hw_cfg.protect3     = 0x50;

    /* 新增：电流相关初值 */
    ctx->pack_current_mA  = 0;
    ctx->pack_current_dir = 0;
		
		ctx->ts1_raw_adc = 0;
		ctx->ts1_temp_dC = 0;
		/* OCD / SCD 第一阶段：先用保守配置，重点验证装载链和状态观察链 */
		ctx->ocdscd_cfg.scd_delay_code  = BQ76940_SCD_DELAY_400US;
		ctx->ocdscd_cfg.scd_thresh_code = BQ76940_SCD_THRESH_200MV;

		ctx->ocdscd_cfg.ocd_delay_code  = BQ76940_OCD_DELAY_320MS;
		ctx->ocdscd_cfg.ocd_thresh_code = BQ76940_OCD_THRESH_100MV;
		
		
		ctx->fault_clear_once_enable = 0;
		ctx->fault_mask_active       = 0;
		ctx->sys_stat_before_clear   = 0;
		ctx->sys_stat_after_clear    = 0;
		
		
		ctx->hw_ocd_active              = 0;
		ctx->hw_scd_active              = 0;
		ctx->hw_dsg_block_active        = 0;
		ctx->hw_fault_recover_once_enable = 0;
		
		BQ76940_ClearCellBalRegs(&ctx->cellbal_wr);
		BQ76940_ClearCellBalRegs(&ctx->cellbal_rd);
		/* BQ76200 执行层初始化 */
    BQ76200_ExecInit(&ctx->bq76200_exec);

		ctx->bal_test_once_enable  = 1;
		ctx->bal_test_target_label = 12;
		
				/* 自动均衡第一版参数 */
		ctx->bal_cfg.diff_enter_mV      = 80;    /* 压差 >= 80mV 开始均衡 */
		ctx->bal_cfg.diff_exit_mV       = 30;    /* 压差 <= 30mV 退出均衡 */
		ctx->bal_cfg.min_cell_mV        = 3900;  /* 最高单体至少高于 3.9V 才考虑均衡 */
		ctx->bal_cfg.max_abs_current_mA = 200;   /* 电流绝对值小于 200mA 才允许均衡 */

		ctx->bal_active      = 0;
		ctx->bal_target_label = 0;

		BQ76940_ClearCellBalRegs(&ctx->bal_auto_wr);
		BQ76940_ClearCellBalRegs(&ctx->bal_auto_rd);
}


//static uint8_t BQ76940_AppPrintSysCtrl2Readback(const char *tag)
//{
//    uint8_t sys_ctrl2;

//    if (BQ76940_ReadReg(BQ76940_REG_SYS_CTRL2, &sys_ctrl2) != BQ76940_OK)
//    {
//        return 1;
//    }

//    printf("[%s]\r\n", tag);
//    printf("SYS_CTRL2 = 0x%02X\r\n", sys_ctrl2);
//    printf("DSG_ON    = %d\r\n", (sys_ctrl2 & BQ76940_SYS_CTRL2_DSG_ON) ? 1 : 0);
//    printf("CHG_ON    = %d\r\n", (sys_ctrl2 & BQ76940_SYS_CTRL2_CHG_ON) ? 1 : 0);

//    return 0;
//}

static void BQ76940_AppSetBringUpFault(BQ76940_AppCtx_t *ctx,
                                       uint8_t stage,
                                       uint8_t error)
{
    if (ctx == 0)
    {
        return;
    }

    ctx->diag_state.bringup_last_stage = stage;
    ctx->diag_state.bringup_last_error = error;
}



uint8_t BQ76940_AppForceSafeOff(BQ76940_AppCtx_t *ctx)
{
    uint8_t result = 0U;
    uint8_t ret;
    BQ76940_CellBalRegs_t zero_bal;

    if (ctx == 0)
    {
        return 1U;
    }

    BQ76200_ExecForceOff(&ctx->bq76200_exec);

    BQ76940_ClearCellBalRegs(&zero_bal);

    ret = BQ76940_WriteCellBalRegs(&zero_bal);
    if (ret == BQ76940_OK)
    {
        ctx->cellbal_wr = zero_bal;
        ctx->cellbal_rd = zero_bal;
        ctx->bal_auto_wr = zero_bal;
        ctx->bal_auto_rd = zero_bal;
        ctx->bal_active = 0U;
        ctx->bal_target_label = 0U;
    }
    else
    {
        result |= 0x01U;
    }

    ret = BQ76940_SetFETState(0U, 0U);
    if (ret != BQ76940_OK)
    {
        result |= 0x02U;
    }

    return result;
}


static uint8_t BQ76940_AppBringUpOnce(BQ76940_AppCtx_t *ctx)
{
    uint8_t ret;

    if (ctx == 0)
    {
        return 1U;
    }

    /* 1. 唤醒 BQ76940 */
    printf("MCU_WAKE_BQ start...\r\n");
    BQ_WAKE_Pulse();
    printf("MCU_WAKE_BQ done.\r\n");

    delay_ms(20);

    /* 2. 最小 bring-up：初始化寄存器、读取基础寄存器、读取 ADC 校准 */
    ret = BQ76940_ProtectBringUp(&ctx->regs, &ctx->calib);
    if (ret != 0U)
    {
        BQ76940_AppSetBringUpFault(ctx,
                                   BQ76940_BRINGUP_STAGE_BASIC,
                                   ret);
        return 11U;
    }

    printf("BQ76940_InitForBringUp ok.\r\n");
    printf("BQ76940_ReadBasicRegs ok.\r\n");
    BQ76940_PrintBasicRegs(&ctx->regs);

//    printf("\r\n[ADC Calib]\r\n");
//    printf("GAIN_uV_per_LSB = %d\r\n", ctx->calib.gain_uV_per_lsb);
//    printf("OFFSET_mV       = %d\r\n", ctx->calib.offset_mV);

    /* 3. 加载硬件 OV / UV 保护参数 */
    ret = BQ76940_ProtectLoadConfig(&ctx->hw_cfg,
                                    &ctx->calib,
                                    &ctx->regs);
    if (ret != 0U)
    {
        BQ76940_AppSetBringUpFault(ctx,
                                   BQ76940_BRINGUP_STAGE_HW_CONFIG,
                                   ret);
        return 12U;
    }

    BQ76940_PrintBasicRegs(&ctx->regs);

    /* 4. 读取当前硬件状态 */
    ret = BQ76940_ProtectReadStatus(&ctx->sys_stat, &ctx->sys_ctrl2);
    if (ret != 0U)
    {
        BQ76940_AppSetBringUpFault(ctx,
                                   BQ76940_BRINGUP_STAGE_STATUS,
                                   ret);
        return 13U;
    }

    /* 5. 加载 OCD / SCD 硬件保护参数 */
    ret = BQ76940_ProtectLoadOcdScd(&ctx->ocdscd_cfg, &ctx->regs);
    if (ret != 0U)
    {
        BQ76940_AppSetBringUpFault(ctx,
                                   BQ76940_BRINGUP_STAGE_OCDSCD,
                                   ret);
        return 14U;
    }

//    printf("\r\n[OCD/SCD CONFIG]\r\n");
//    printf("PROTECT1 = 0x%02X\r\n", ctx->regs.protect1);
//    printf("PROTECT2 = 0x%02X\r\n", ctx->regs.protect2);

    BQ76940_ProtectPrintStatus(ctx->sys_stat, ctx->sys_ctrl2);

    /*
     * 6. 采样链路自检
     * 目的：
     * - 验证 9 节电压读取链路
     * - 验证 CC 电流读取链路
     * - 验证 TS1 温度读取链路
     * - 验证 SYS_STAT 运行状态读取链路
     */
    ret = BQ76940_AppSampleUpdate(ctx);
    if (ret != 0U)
    {
        BQ76940_AppSetBringUpFault(ctx,
                                   BQ76940_BRINGUP_STAGE_SAMPLE,
                                   ret);
        return 15U;
    }

    printf("[BQ76940 SELFTEST] sample chain ok.\r\n");

    return 0U;
}


uint8_t BQ76940_AppBringUpAndSelfTest(BQ76940_AppCtx_t *ctx)
{
    uint8_t ret;
    uint8_t attempt;

    if (ctx == 0)
    {
        return 1U;
    }

    /* 1. 打印当前软件实验阈值 */
    BQ76940_PrintAlarmThresholds9(&ctx->alarm_th);

    printf("\r\n============================\r\n");
    printf("BQ76940 Bring-up Self Test\r\n");
    printf("============================\r\n");

		/*上电前先清楚所有故障*/
    ctx->diag_state.bringup_attempt_count = 0U;
    ctx->diag_state.bringup_fault_active  = 0U;
    ctx->diag_state.bringup_last_stage    = 0U;
    ctx->diag_state.bringup_last_error    = 0U;

    for (attempt = 1U;
         attempt <= BQ76940_APP_BRINGUP_RETRY_LIMIT;
         attempt++)
    {
        ctx->diag_state.bringup_attempt_count = attempt;

        printf("\r\n[BQ76940 BRINGUP] attempt %d/%d\r\n",
               attempt,
               BQ76940_APP_BRINGUP_RETRY_LIMIT);

        ret = BQ76940_AppBringUpOnce(ctx);
        if (ret == 0U)
        {
            ctx->diag_state.bringup_fault_active = 0U;

            printf("[BQ76940 BRINGUP] success, attempt = %d\r\n", attempt);
            return 0U;
        }

        printf("[BQ76940 BRINGUP] fail, attempt = %d, ret = %d, stage = %d, error = %d\r\n",
               attempt,
               ret,
               ctx->diag_state.bringup_last_stage,
               ctx->diag_state.bringup_last_error);

        delay_ms(100);
    }

    ctx->diag_state.bringup_fault_active = 1U;

    printf("[BQ76940 BRINGUP] final fail, attempts = %d, stage = %d, error = %d\r\n",
           ctx->diag_state.bringup_attempt_count,
           ctx->diag_state.bringup_last_stage,
           ctx->diag_state.bringup_last_error);

    return 100U;
}




static uint8_t BQ76940_AppHandleTempProtect(BQ76940_AppCtx_t *ctx)
{
    uint8_t ret;
    BQ76940_OtProtectRequest_t req;

    if (ctx == 0)
    {
        return 1U;
    }

    /*
     * 兼容旧接口：
     *   1. Decide  只判断
     *   2. ApplyHw 只写硬件
     *   3. Commit  只更新 ctx
     *
     * 注意：
     *   本函数本身不负责加锁。
     *   后续在 FreeRTOS ProtectTask 中，应直接调用三段式接口。
     */
    ret = BQ76940_AppOtProtectDecide(ctx, &req);
    if (ret != 0U)
    {
        return 10U;
    }

    ret = BQ76940_AppOtProtectApplyHw(&req);
    if (ret != 0U)
    {
        return 20U;
    }

    ret = BQ76940_AppOtProtectCommit(ctx, &req);
    if (ret != 0U)
    {
        return 30U;
    }

    return 0U;
}

void BQ76940_AppOtProtectRequestClear(BQ76940_OtProtectRequest_t *req)
{
    if (req == 0)
    {
        return;
    }

    req->action = BQ76940_OT_ACTION_NONE;

    req->ot_now = 0U;
    req->ov_now = 0U;
    req->uv_now = 0U;

    req->ot_cutoff_active_snapshot = 0U;
}

uint8_t BQ76940_AppOtProtectDecide(const BQ76940_AppCtx_t *ctx,
                                    BQ76940_OtProtectRequest_t *req)
{
    if ((ctx == 0) || (req == 0))
    {
        return 1U;
    }

    BQ76940_AppOtProtectRequestClear(req);

    /*
     * 保存当前保护相关状态快照。
     *
     * 注意：
     *   Decide 阶段只读取 ctx，不访问 BQ76940 I2C，
     *   也不修改 ctx。
     */
    req->ot_now = ctx->alarm_state.ot_flag;
    req->ov_now = ctx->alarm_state.ov_flag;
    req->uv_now = ctx->alarm_state.uv_flag;

    req->ot_cutoff_active_snapshot = ctx->ot_cutoff_active;

    /*
     * 情况 1：
     * OT 过温触发，并且当前还没有进入 OT 截止状态。
     * 需要关闭 CHG / DSG。
     */
    if ((req->ot_now != 0U) &&
        (req->ot_cutoff_active_snapshot == 0U))
    {
        req->action = BQ76940_OT_ACTION_FET_OFF;
        return 0U;
    }

    /*
     * 情况 2：
     * OT 已恢复，并且之前处于 OT 截止状态。
     *
     * 保持你原来的逻辑：
     *   只有当前没有 OV / UV 时，才允许恢复 CHG / DSG。
     */
    if ((req->ot_now == 0U) &&
        (req->ot_cutoff_active_snapshot != 0U))
    {
        if ((req->ov_now == 0U) &&
            (req->uv_now == 0U))
        {
            req->action = BQ76940_OT_ACTION_FET_ON;
            return 0U;
        }
    }

    /*
     * 情况 3：
     * 本轮不需要硬件动作。
     */
    req->action = BQ76940_OT_ACTION_NONE;

    return 0U;
}

uint8_t BQ76940_AppOtProtectApplyHw(const BQ76940_OtProtectRequest_t *req)
{
    uint8_t ret;

    if (req == 0)
    {
        return 1U;
    }

    /*
     * 无动作时，不访问 I2C。
     */
    if (req->action == BQ76940_OT_ACTION_NONE)
    {
        return 0U;
    }

    /*
     * OT active：
     *   CHG = OFF
     *   DSG = OFF
     */
    if (req->action == BQ76940_OT_ACTION_FET_OFF)
    {
        ret = BQ76940_SetFETState(0U, 0U);
        if (ret != 0U)
        {
            return 2U;
        }

        return 0U;
    }

    /*
     * OT recover：
     *   CHG = ON
     *   DSG = ON
     *
     * 注意：
     *   是否允许恢复已经在 Decide 阶段判断。
     */
    if (req->action == BQ76940_OT_ACTION_FET_ON)
    {
        ret = BQ76940_SetFETState(1U, 1U);
        if (ret != 0U)
        {
            return 3U;
        }

        return 0U;
    }

    return 4U;
}


uint8_t BQ76940_AppOtProtectCommit(BQ76940_AppCtx_t *ctx,
                                    const BQ76940_OtProtectRequest_t *req)
{
    if ((ctx == 0) || (req == 0))
    {
        return 1U;
    }

    /*
     * 无动作时，不修改 ctx。
     */
    if (req->action == BQ76940_OT_ACTION_NONE)
    {
        return 0U;
    }

    /*
     * OT 触发后，提交截止状态。
     */
    if (req->action == BQ76940_OT_ACTION_FET_OFF)
    {
        ctx->ot_cutoff_active = 1U;

#if (BQ76940_PROTECT_EVENT_PRINT_ENABLE != 0U)
        printf("[PROT] OT -> CHG OFF, DSG OFF\r\n");
#endif

        return 0U;
    }

    /*
     * OT 恢复后，清除截止状态。
     */
    if (req->action == BQ76940_OT_ACTION_FET_ON)
    {
        ctx->ot_cutoff_active = 0U;

#if (BQ76940_PROTECT_EVENT_PRINT_ENABLE != 0U)
        printf("[PROT] OT recover -> CHG ON, DSG ON\r\n");
#endif

        return 0U;
    }

    return 2U;
}


uint8_t BQ76940_AppProtectUpdateAlarms(BQ76940_AppCtx_t *ctx)
{
    uint8_t ret;

    if (ctx == 0)
    {
        return 1U;
    }

    /*
     * 1. 更新单体电压相关软件告警：
     *    UV / OV / DIFF
     */
    ret = BQ76940_UpdateAlarmState9(ctx->cell_mV,
                                    &ctx->cell_stats,
                                    &ctx->alarm_th,
                                    &ctx->alarm_state);
    if (ret != 0U)
    {
        return 13U;
    }

    /*
     * 2. 更新 TS1 过温告警
     */
    ret = BQ76940_UpdateTempAlarmTs1(ctx->ts1_temp_dC,
                                     &ctx->alarm_th,
                                     &ctx->alarm_state);
    if (ret != 0U)
    {
        printf("[TEMP ALARM] update fail, ret = %d\r\n", ret);
        return 20U;
    }

    /*
     * 3. 更新 TS1 低温告警
     */
    ret = BQ76940_UpdateLowTempAlarmTs1(ctx->ts1_temp_dC,
                                        &ctx->alarm_th,
                                        &ctx->alarm_state);
    if (ret != 0U)
    {
        printf("[LOW TEMP ALARM] update fail, ret = %d\r\n", ret);
        return 22U;
    }

    return 0U;
}

static uint8_t BQ76940_AppHandleLowTempProtect(BQ76940_AppCtx_t *ctx)
{
    uint8_t ret;
    BQ76940_UtProtectRequest_t req;

    if (ctx == 0)
    {
        return 1U;
    }

    /*
     * 兼容旧接口：
     *   1. Decide  只判断
     *   2. ApplyHw 只写硬件
     *   3. Commit  只更新 ctx
     *
     * 注意：
     *   本函数本身不负责加锁。
     *   后续在 FreeRTOS ProtectTask 中，应直接调用三段式接口。
     */
    ret = BQ76940_AppUtProtectDecide(ctx, &req);
    if (ret != 0U)
    {
        return 10U;
    }

    ret = BQ76940_AppUtProtectApplyHw(&req);
    if (ret != 0U)
    {
        return 20U;
    }

    ret = BQ76940_AppUtProtectCommit(ctx, &req);
    if (ret != 0U)
    {
        return 30U;
    }

    return 0U;
}


void BQ76940_AppUtProtectRequestClear(BQ76940_UtProtectRequest_t *req)
{
    if (req == 0)
    {
        return;
    }

    req->action = BQ76940_UT_ACTION_NONE;

    req->ut_now = 0U;
    req->ov_now = 0U;
    req->ot_now = 0U;

    req->ot_cutoff_active_snapshot  = 0U;
    req->ut_chg_block_active_snapshot = 0U;
}

uint8_t BQ76940_AppUtProtectDecide(const BQ76940_AppCtx_t *ctx,
                                    BQ76940_UtProtectRequest_t *req)
{
    if ((ctx == 0) || (req == 0))
    {
        return 1U;
    }

    BQ76940_AppUtProtectRequestClear(req);

    /*
     * 保存当前保护相关状态快照。
     *
     * Decide 阶段只读取 ctx：
     *   - 不访问 BQ76940 I2C
     *   - 不修改 ctx
     */
    req->ut_now = ctx->alarm_state.ut_flag;
    req->ov_now = ctx->alarm_state.ov_flag;
    req->ot_now = ctx->alarm_state.ot_flag;

    req->ot_cutoff_active_snapshot   = ctx->ot_cutoff_active;
    req->ut_chg_block_active_snapshot = ctx->ut_chg_block_active;

    /*
     * 情况 1：
     * UT 低温触发，并且当前还没有进入 CHG 阻断状态。
     *
     * 低温保护策略：
     *   只关闭 CHG，不关闭 DSG。
     */
    if ((req->ut_now != 0U) &&
        (req->ut_chg_block_active_snapshot == 0U))
    {
        req->action = BQ76940_UT_ACTION_CHG_OFF;
        return 0U;
    }

    /*
     * 情况 2：
     * UT 已恢复，并且之前处于 CHG 阻断状态。
     *
     * 保持你原来的恢复条件：
     *   - 当前没有 OV
     *   - 当前没有 OT
     *   - 当前没有 OT 截止状态
     */
    if ((req->ut_now == 0U) &&
        (req->ut_chg_block_active_snapshot != 0U))
    {
        if ((req->ov_now == 0U) &&
            (req->ot_now == 0U) &&
            (req->ot_cutoff_active_snapshot == 0U))
        {
            req->action = BQ76940_UT_ACTION_CHG_ON;
            return 0U;
        }
    }

    /*
     * 情况 3：
     * 本轮不需要硬件动作。
     */
    req->action = BQ76940_UT_ACTION_NONE;

    return 0U;
}

uint8_t BQ76940_AppUtProtectApplyHw(const BQ76940_UtProtectRequest_t *req)
{
    uint8_t ret;

    if (req == 0)
    {
        return 1U;
    }

    /*
     * 无动作时，不访问 I2C。
     */
    if (req->action == BQ76940_UT_ACTION_NONE)
    {
        return 0U;
    }

    /*
     * UT active：
     *   CHG = OFF
     *   DSG = unchanged
     */
    if (req->action == BQ76940_UT_ACTION_CHG_OFF)
    {
        ret = BQ76940_SetCHGState(0U);
        if (ret != 0U)
        {
            return 2U;
        }

#if (BQ76940_PROTECT_DBG_ENABLE != 0U)
        /*
         * 读回 SYS_CTRL2 只用于调试确认。
         * 默认关闭，避免增加 I2C 访问和 Flash 字符串。
         */
        ret = BQ76940_AppPrintSysCtrl2Readback("UT ACTIVE READBACK");
        if (ret != 0U)
        {
            return 4U;
        }
#endif

        return 0U;
    }

    /*
     * UT recover：
     *   CHG = ON
     *   DSG = unchanged
     *
     * 是否允许恢复已经在 Decide 阶段判断。
     */
    if (req->action == BQ76940_UT_ACTION_CHG_ON)
    {
        ret = BQ76940_SetCHGState(1U);
        if (ret != 0U)
        {
            return 3U;
        }

#if (BQ76940_PROTECT_DBG_ENABLE != 0U)
        /*
         * 恢复后读回 SYS_CTRL2，仅用于调试确认。
         */
        ret = BQ76940_AppPrintSysCtrl2Readback("UT RECOVER READBACK");
        if (ret != 0U)
        {
            return 5U;
        }
#endif

        return 0U;
    }

    return 6U;
}

uint8_t BQ76940_AppUtProtectCommit(BQ76940_AppCtx_t *ctx,
                                    const BQ76940_UtProtectRequest_t *req)
{
    if ((ctx == 0) || (req == 0))
    {
        return 1U;
    }

    /*
     * 无动作时，不修改 ctx。
     */
    if (req->action == BQ76940_UT_ACTION_NONE)
    {
        return 0U;
    }

    /*
     * UT 触发后，提交 CHG 阻断状态。
     */
    if (req->action == BQ76940_UT_ACTION_CHG_OFF)
    {
        ctx->ut_chg_block_active = 1U;

#if (BQ76940_PROTECT_EVENT_PRINT_ENABLE != 0U)
        printf("[PROT] UT -> CHG OFF\r\n");
#endif

        return 0U;
    }

    /*
     * UT 恢复后，清除 CHG 阻断状态。
     */
    if (req->action == BQ76940_UT_ACTION_CHG_ON)
    {
        ctx->ut_chg_block_active = 0U;

#if (BQ76940_PROTECT_EVENT_PRINT_ENABLE != 0U)
        printf("[PROT] UT recover -> CHG ON\r\n");
#endif

        return 0U;
    }

    return 2U;
}


void BQ76940_AppOcdScdRequestClear(BQ76940_OcdScdRequest_t *req)
{
    if (req == 0)
    {
        return;
    }

    req->action            = BQ76940_OCDSCD_ACTION_NONE;
    req->sys_stat_snapshot = 0U;
    req->hw_fault_now      = 0U;
    req->ocd_now           = 0U;
    req->scd_now           = 0U;
    req->recover_request   = 0U;
}

uint8_t BQ76940_AppOcdScdDecide(const BQ76940_AppCtx_t *ctx,
                                BQ76940_OcdScdRequest_t *req)
{
    if ((ctx == 0) || (req == 0))
    {
        return 1U;
    }

    BQ76940_AppOcdScdRequestClear(req);

    /*
     * 保存当前 SYS_STAT 快照。
     *
     * 注意：
     *   ctx->sys_stat 已经在 SampleTask 采样阶段读取并提交到 app。
     *   这里不直接读 BQ76940。
     */
    req->sys_stat_snapshot = ctx->sys_stat;

    req->hw_fault_now = (uint8_t)(ctx->sys_stat &
                                  (BQ76940_SYS_STAT_OCD |
                                   BQ76940_SYS_STAT_SCD));

    if ((ctx->sys_stat & BQ76940_SYS_STAT_OCD) != 0U)
    {
        req->ocd_now = 1U;
    }

    if ((ctx->sys_stat & BQ76940_SYS_STAT_SCD) != 0U)
    {
        req->scd_now = 1U;
    }

    /*
     * 情况 1：
     * 当前硬件检测到 OCD / SCD，并且 DSG 还没有被阻断。
     * 需要执行 DSG_OFF。
     */
    if ((req->hw_fault_now != 0U) &&
        (ctx->hw_dsg_block_active == 0U))
    {
        req->action = BQ76940_OCDSCD_ACTION_DSG_OFF;
        return 0U;
    }

    /*
     * 情况 2：
     * 手动恢复：
     *   - 已经人工发起恢复请求
     *   - 当前 DSG 处于阻断状态
     *   - 当前硬件 OCD / SCD 位已经清除
     *   - 当前没有 UV / OT 阻塞
     */
    if ((ctx->hw_fault_recover_once_enable != 0U) &&
        (ctx->hw_dsg_block_active != 0U) &&
        (req->hw_fault_now == 0U))
    {
        if ((ctx->alarm_state.uv_flag == 0U) &&
            (ctx->alarm_state.ot_flag == 0U))
        {
            req->action = BQ76940_OCDSCD_ACTION_DSG_ON;
            req->recover_request = 1U;
            return 0U;
        }
    }

    /*
     * 情况 3：
     * 无需硬件动作。
     * 但 Commit 阶段仍然可以根据 ocd_now / scd_now 锁存状态。
     */
    req->action = BQ76940_OCDSCD_ACTION_NONE;

    return 0U;
}

uint8_t BQ76940_AppOcdScdApplyHw(const BQ76940_OcdScdRequest_t *req)
{
    uint8_t ret;

    if (req == 0)
    {
        return 1U;
    }

    /*
     * 无动作时，不访问 I2C。
     */
    if (req->action == BQ76940_OCDSCD_ACTION_NONE)
    {
        return 0U;
    }

    if (req->action == BQ76940_OCDSCD_ACTION_DSG_OFF)
    {
        ret = BQ76940_SetDSGState(0U);
        if (ret != 0U)
        {
            return 2U;
        }

#if (BQ76940_PROTECT_DBG_ENABLE != 0U)
        /*
         * 读回 SYS_CTRL2 只用于调试确认。
         * 默认关闭，避免增加 I2C 访问和 Flash 字符串。
         */
        ret = BQ76940_AppPrintSysCtrl2Readback("OCD/SCD ACTIVE READBACK");
        if (ret != 0U)
        {
            return 3U;
        }
#endif

        return 0U;
    }

    if (req->action == BQ76940_OCDSCD_ACTION_DSG_ON)
    {
        ret = BQ76940_SetDSGState(1U);
        if (ret != 0U)
        {
            return 4U;
        }

#if (BQ76940_PROTECT_DBG_ENABLE != 0U)
        ret = BQ76940_AppPrintSysCtrl2Readback("OCD/SCD RECOVER READBACK");
        if (ret != 0U)
        {
            return 5U;
        }
#endif

        return 0U;
    }

    return 6U;
}

uint8_t BQ76940_AppOcdScdCommit(BQ76940_AppCtx_t *ctx,
                                const BQ76940_OcdScdRequest_t *req)
{
    if ((ctx == 0) || (req == 0))
    {
        return 1U;
    }

    /*
     * 锁存当前是否发生过 OCD / SCD。
     * 即使本轮没有硬件动作，也要记录当前故障状态。
     */
    if (req->ocd_now != 0U)
    {
        ctx->hw_ocd_active = 1U;
    }

    if (req->scd_now != 0U)
    {
        ctx->hw_scd_active = 1U;
    }

#if (BQ76940_PROTECT_DBG_ENABLE != 0U)
    printf("[OCD/SCD DBG] SYS=%02X OCD=%u SCD=%u DSG_BLK=%u\r\n",
           req->sys_stat_snapshot,
           ctx->hw_ocd_active,
           ctx->hw_scd_active,
           ctx->hw_dsg_block_active);
#endif

    /*
     * DSG_OFF 执行成功后，提交 DSG 阻断状态。
     */
    if (req->action == BQ76940_OCDSCD_ACTION_DSG_OFF)
    {
        ctx->hw_dsg_block_active = 1U;

#if (BQ76940_PROTECT_EVENT_PRINT_ENABLE != 0U)
        if (req->scd_now != 0U)
        {
            printf("[HW] SCD -> DSG OFF\r\n");
        }
        else if (req->ocd_now != 0U)
        {
            printf("[HW] OCD -> DSG OFF\r\n");
        }
        else
        {
            printf("[HW] OCD/SCD -> DSG OFF\r\n");
        }
#endif
    }
    /*
     * DSG_ON 执行成功后，清除锁存状态和恢复请求。
     */
    else if (req->action == BQ76940_OCDSCD_ACTION_DSG_ON)
    {
        ctx->hw_dsg_block_active          = 0U;
        ctx->hw_ocd_active                = 0U;
        ctx->hw_scd_active                = 0U;
        ctx->hw_fault_recover_once_enable = 0U;

#if (BQ76940_PROTECT_EVENT_PRINT_ENABLE != 0U)
        printf("[HW] OCD/SCD recover -> DSG ON\r\n");
#endif
    }

    return 0U;
}




static uint8_t BQ76940_AppHandleOcdScdProtect(BQ76940_AppCtx_t *ctx)
{
    uint8_t ret;
    BQ76940_OcdScdRequest_t req;

    if (ctx == 0)
    {
        return 1U;
    }

    /*
     * 兼容旧接口：
     *   1. Decide  只判断
     *   2. ApplyHw 只写硬件
     *   3. Commit  只更新 ctx
     *
     * 注意：
     *   本函数不负责加锁。
     *   后续在 FreeRTOS ProtectTask 中，应直接调用三段式接口。
     */
    ret = BQ76940_AppOcdScdDecide(ctx, &req);
    if (ret != 0U)
    {
        return 10U;
    }

    ret = BQ76940_AppOcdScdApplyHw(&req);
    if (ret != 0U)
    {
        return 20U;
    }

    ret = BQ76940_AppOcdScdCommit(ctx, &req);
    if (ret != 0U)
    {
        return 30U;
    }

    return 0U;
}

uint8_t BQ76940_AppProtectUpdateBase(BQ76940_AppCtx_t *ctx)
{
    if (ctx == 0)
    {
        return 1U;
    }

    /*
     * Base 阶段只做告警更新：
     *   - UV / OV / DIFF
     *   - OT
     *   - UT
     *
     * 不执行 CHG / DSG / CELLBAL 等硬件动作。
     * 不访问 I2C。
     */
    return BQ76940_AppProtectUpdateAlarms(ctx);
}


uint8_t BQ76940_AppRunCycle(BQ76940_AppCtx_t *ctx)
{
    uint8_t ret;

    if (ctx == 0)
    {
        return 1U;
    }

    /*
     * 1. 采样阶段：
     * 电压 / 电流 / 温度 / 硬件状态读取
     */
    ret = BQ76940_AppSampleUpdate(ctx);
    if (ret != 0U)
    {
        return ret;
    }

    /*
     * 2. 保护阶段：
     * UV / OV / DIFF / OT / UT / OCD / SCD
     */
    ret = BQ76940_AppProtectUpdate(ctx);
    if (ret != 0U)
    {
        return ret;
    }

    /*
     * 3. 均衡阶段：
     * 自动均衡判断与 CELLBAL 控制
     */
    ret = BQ76940_AppBalanceUpdate(ctx);
    if (ret != 0U)
    {
        return ret;
    }

    /*
     * 4. 执行控制阶段：
     * 根据保护状态刷新 BQ76200 执行层
     */
    ret = BQ76940_AppControlUpdate(ctx);
    if (ret != 0U)
    {
        return ret;
    }

    /*
     * 5. 调试打印阶段
     */
    BQ76940_AppPrintRuntime(ctx);

    return 0U;
}



uint8_t BQ76940_AppSampleReadHw(const BQ76940_AdcCalib_t *calib,
                                BQ76940_AppSampleData_t *sample)
{
    uint8_t ret;

    if ((calib == 0) || (sample == 0))
    {
        return 1U;
    }

    /*
     * 1. 读取 9 节映射电压。
     *
     * 注意：
     *   该函数内部会通过 I2C 访问 BQ76940。
     *   因此调用本函数前，上层任务应已经拿到 I2C 总线互斥锁。
     */
    ret = BQ76940_ReadAllMappedCellVoltages9_mV(calib,
                                                sample->cell_raw,
                                                sample->cell_mV);
    if (ret != 0U)
    {
        return 11U;
    }

    /*
     * 2. 触发一次 CC 1-shot 电流采样。
     */
    ret = BQ76940_CC_StartOneShot();
    if (ret != 0U)
    {
        printf("[CC] StartOneShot fail, ret = %d\r\n", ret);
        return 14U;
    }

    /*
     * 3. 等待 CC_READY。
     *
     * 当前版本保持原来的逻辑：
     *   等待期间仍然认为属于 BQ76940 电流采样事务的一部分。
     */
    ret = BQ76940_CC_WaitReady(600U);
    if (ret != 0U)
    {
        printf("[CC] WaitReady fail, ret = %d\r\n", ret);
        return 15U;
    }

    /*
     * 4. 读取 CC 原始值。
     */
    ret = BQ76940_CC_ReadRaw(&sample->cc_raw);
    if (ret != 0U)
    {
        printf("[CC] ReadRaw fail, ret = %d\r\n", ret);
        return 16U;
    }

    /*
     * 5. 读取 TS1 原始 ADC。
     */
    ret = BQ76940_ReadTS1Raw(&sample->ts1_raw_adc);
    if (ret != 0U)
    {
        printf("[TEMP] Read TS1 fail, ret = %d\r\n", ret);
        return 18U;
    }

    /*
     * 6. 读取 BQ76940 硬件故障状态 SYS_STAT。
     */
    ret = BQ76940_ProtectReadFaultStatus(&sample->sys_stat);
    if (ret != 0U)
    {
        printf("[HW FAULT] read SYS_STAT fail, ret = %d\r\n", ret);
        return 23U;
    }

    return 0U;
}



uint8_t BQ76940_AppSampleProcess(BQ76940_AppSampleData_t *sample)
{
    uint8_t ret;

    if (sample == 0)
    {
        return 1U;
    }

    /*
     * 1. 计算 Pack 总压。
     * 这一步只依赖已经读取到的 cell_mV，不访问 I2C。
     */
    sample->pack_total_mV = BQ76940_CalcPackVoltage9_mV(sample->cell_mV);

    /*
     * 2. 统计最高单体、最低单体、压差。
     */
    ret = BQ76940_AnalyzeCellVoltages9(sample->cell_mV,
                                       &sample->cell_stats);
    if (ret != 0U)
    {
        return 12U;
    }

    /*
     * 3. CC 原始值换算为 Pack 电流。
     */
    ret = BQ76940_CC_ConvertToCurrent_mA(sample->cc_raw.raw_s16,
                                         BQ76940_RSENSE_UOHM,
                                         &sample->pack_current_mA);
    if (ret != 0U)
    {
        printf("[CC] ConvertToCurrent fail, ret = %d\r\n", ret);
        return 17U;
    }

    /*
     * 4. 判断电流方向：充电 / 放电 / 近零。
     */
    sample->pack_current_dir =
        BQ76940_AppJudgeCurrentDir(sample->pack_current_mA);

    /*
     * 5. TS1 原始 ADC 换算为温度。
     */
    ret = BQ76940_ConvertTS1Temp_dC(sample->ts1_raw_adc,
                                    &sample->ts1_temp_dC);
    if (ret != 0U)
    {
        printf("[TEMP] Convert TS1 fail, ret = %d\r\n", ret);
        return 19U;
    }

    /*
     * 6. 从 SYS_STAT 中提取当前激活的硬件故障位。
     */
    ret = BQ76940_ProtectGetActiveFaultMask(sample->sys_stat,
                                            &sample->fault_mask_active);
    if (ret != 0U)
    {
        printf("[HW FAULT] get active mask fail, ret = %d\r\n", ret);
        return 24U;
    }

    return 0U;
}


uint8_t BQ76940_AppSampleCommit(BQ76940_AppCtx_t *ctx,
                                const BQ76940_AppSampleData_t *sample)
{
    if ((ctx == 0) || (sample == 0))
    {
        return 1U;
    }

    /*
     * 将局部采样快照提交到全局 app。
     *
     * 注意：
     *   该函数会修改 BQ76940_AppCtx_t，
     *   因此在 FreeRTOS 任务中调用时，应持有 g_bms_ctx_mutex。
     */
    memcpy(ctx->cell_raw,
           sample->cell_raw,
           sizeof(ctx->cell_raw));

    memcpy(ctx->cell_mV,
           sample->cell_mV,
           sizeof(ctx->cell_mV));

    ctx->pack_total_mV = sample->pack_total_mV;
    ctx->cell_stats    = sample->cell_stats;

    ctx->cc_raw           = sample->cc_raw;
    ctx->pack_current_mA  = sample->pack_current_mA;
    ctx->pack_current_dir = sample->pack_current_dir;

    ctx->ts1_raw_adc = sample->ts1_raw_adc;
    ctx->ts1_temp_dC = sample->ts1_temp_dC;

    ctx->sys_stat          = sample->sys_stat;
    ctx->fault_mask_active = sample->fault_mask_active;

    return 0U;
}


uint8_t BQ76940_AppSampleUpdate(BQ76940_AppCtx_t *ctx)
{
    uint8_t ret;
    BQ76940_AppSampleData_t sample;

    if (ctx == 0)
    {
        return 1U;
    }

    /*
     * 兼容旧接口：
     *   1. 读取硬件数据
     *   2. 处理采样数据
     *   3. 提交到 app
     *
     * 注意：
     *   该函数本身不负责加锁。
     *   在 FreeRTOS 任务中，推荐直接调用
     *   ReadHw / Process / Commit 三段式接口。
     */
    ret = BQ76940_AppSampleReadHw(&ctx->calib, &sample);
    if (ret != 0U)
    {
        return ret;
    }

    ret = BQ76940_AppSampleProcess(&sample);
    if (ret != 0U)
    {
        return ret;
    }

    ret = BQ76940_AppSampleCommit(ctx, &sample);
    if (ret != 0U)
    {
        return ret;
    }

    return 0U;
}


uint8_t BQ76940_AppProtectUpdate(BQ76940_AppCtx_t *ctx)
{
    uint8_t ret;

    if (ctx == 0)
    {
        return 1U;
    }

    /*
     * 1. 更新单体电压相关软件告警：
     * UV / OV / DIFF
     */
    ret = BQ76940_UpdateAlarmState9(ctx->cell_mV,
                                    &ctx->cell_stats,
                                    &ctx->alarm_th,
                                    &ctx->alarm_state);
    if (ret != 0U)
    {
        return 13U;
    }

    /*
     * 2. 更新 TS1 过温告警
     */
    ret = BQ76940_UpdateTempAlarmTs1(ctx->ts1_temp_dC,
                                     &ctx->alarm_th,
                                     &ctx->alarm_state);
    if (ret != 0U)
    {
        printf("[TEMP ALARM] update fail, ret = %d\r\n", ret);
        return 20U;
    }

    /*
     * 3. 更新 TS1 低温告警
     */
    ret = BQ76940_UpdateLowTempAlarmTs1(ctx->ts1_temp_dC,
                                        &ctx->alarm_th,
                                        &ctx->alarm_state);
    if (ret != 0U)
    {
        printf("[LOW TEMP ALARM] update fail, ret = %d\r\n", ret);
        return 22U;
    }

    /*
     * 4. 处理过温联动控制：
     * OT 生效时，充放电都禁止。
     */
    ret = BQ76940_AppHandleTempProtect(ctx);
    if (ret != 0U)
    {
        printf("[TEMP PROTECT] handle fail, ret = %d\r\n", ret);
        return 21U;
    }

    /*
     * 5. 处理低温联动控制：
     * UT 生效时，只禁止充电。
     */
    ret = BQ76940_AppHandleLowTempProtect(ctx);
    if (ret != 0U)
    {
        printf("[LOW TEMP PROTECT] handle fail, ret = %d\r\n", ret);
        return 25U;
    }

    /*
     * 6. 处理 OCD / SCD 放电侧硬件保护状态
     */
    ret = BQ76940_AppHandleOcdScdProtect(ctx);
    if (ret != 0U)
    {
        printf("[OCD/SCD PROTECT] handle fail, ret = %d\r\n", ret);
        return 27U;
    }

    return 0U;
}

uint8_t BQ76940_AppControlUpdate(BQ76940_AppCtx_t *ctx)
{
    uint8_t ret;
    BQ76200_ExecInput_t exec_input;

    if (ctx == 0)
    {
        return 1U;
    }

    /*
     * 组织 BQ76200 执行层输入。
     *
     * ot_cutoff_active:
     *     过温保护生效，充放电都禁止
     *
     * ut_chg_block_active:
     *     低温禁充，只禁止充电
     *
     * hw_dsg_block_active:
     *     OCD/SCD 等放电侧故障，只禁止放电
     */
    exec_input.ot_cutoff_active    = ctx->ot_cutoff_active;
    exec_input.ut_chg_block_active = ctx->ut_chg_block_active;
    exec_input.hw_dsg_block_active = ctx->hw_dsg_block_active;

    /*
     * 更新 BQ76200 执行层状态机
     */
    ret = BQ76200_ExecUpdate(&ctx->bq76200_exec, &exec_input);
    if (ret != 0U)
    {
        printf("[BQ76200 EXEC] update fail, ret = %d\r\n", ret);
        return 31U;
    }

    return 0U;
}

//void BQ76940_AppPrintRuntime(const BQ76940_AppCtx_t *ctx)
//{
//    if (ctx == 0)
//    {
//        return;
//    }

//    printf("----------------------------------------\r\n");

//    BQ76940_PrintAllMappedCellVoltages9(ctx->cell_raw,
//                                        ctx->cell_mV,
//                                        ctx->pack_total_mV);

//    BQ76940_PrintCellStats9(&ctx->cell_stats, ctx->pack_total_mV);

//    BQ76940_PrintAlarmFlags9(&ctx->alarm_state);

//    BQ76940_PrintPackCurrent(&ctx->cc_raw,
//                             ctx->pack_current_mA,
//                             ctx->pack_current_dir);

//    BQ76940_PrintCycleSummary9(ctx->pack_total_mV,
//                               &ctx->cell_stats,
//                               &ctx->alarm_state,
//                               ctx->pack_current_mA,
//                               ctx->pack_current_dir,
//                               ctx->ts1_temp_dC,
//                               ctx->ot_cutoff_active,
//                               ctx->ut_chg_block_active,
//                               ctx->hw_dsg_block_active,
//                               ctx->hw_ocd_active,
//                               ctx->hw_scd_active);

//    BQ76940_PrintTS1Temp(ctx->ts1_raw_adc, ctx->ts1_temp_dC);

//    BQ76940_PrintTempAlarmTs1(&ctx->alarm_state);
//    BQ76940_PrintLowTempAlarmTs1(&ctx->alarm_state);

//    BQ76940_ProtectPrintFaultStatus(ctx->sys_stat);

//    BQ76940_PrintBalanceAutoState(ctx->bal_active,
//                                  ctx->bal_target_label,
//                                  &ctx->bal_auto_rd);

//    BQ76200_ExecPrintState(&ctx->bq76200_exec);

//    printf("----------------------------------------\r\n");
//}


#define BMS_PRINT_CELL_DETAIL_ENABLE    0U


void BQ76940_AppPrintRuntime(const BQ76940_AppCtx_t *ctx)
{
    uint8_t alm_flags = 0U;
    uint8_t prot_flags = 0U;

    if (ctx == 0)
    {
        return;
    }

#if (BMS_PRINT_CELL_DETAIL_ENABLE != 0U)
    BQ76940_PrintAllMappedCellVoltages9(ctx->cell_raw,
                                        ctx->cell_mV,
                                        ctx->pack_total_mV);
#endif

    if (ctx->alarm_state.uv_flag != 0U)   alm_flags |= 0x01U;
    if (ctx->alarm_state.ov_flag != 0U)   alm_flags |= 0x02U;
    if (ctx->alarm_state.diff_flag != 0U) alm_flags |= 0x04U;
    if (ctx->alarm_state.ot_flag != 0U)   alm_flags |= 0x08U;
    if (ctx->alarm_state.ut_flag != 0U)   alm_flags |= 0x10U;

    if (ctx->ot_cutoff_active != 0U)    prot_flags |= 0x01U;
    if (ctx->ut_chg_block_active != 0U) prot_flags |= 0x02U;
    if (ctx->hw_dsg_block_active != 0U) prot_flags |= 0x04U;
    if (ctx->hw_ocd_active != 0U)       prot_flags |= 0x08U;
    if (ctx->hw_scd_active != 0U)       prot_flags |= 0x10U;
    if (ctx->bal_active != 0U)          prot_flags |= 0x20U;

    printf("[BMS] P=%lumV MAX=VC%u:%umV MIN=VC%u:%umV D=%umV I=%ldmA T=%ddC ALM=%02X PROT=%02X BAL=%u:VC%u SYS=%02X\r\n",
           (unsigned long)ctx->pack_total_mV,
           ctx->cell_stats.max_cell_label,
           ctx->cell_stats.max_mV,
           ctx->cell_stats.min_cell_label,
           ctx->cell_stats.min_mV,
           ctx->cell_stats.diff_mV,
           (long)ctx->pack_current_mA,
           ctx->ts1_temp_dC,
           alm_flags,
           prot_flags,
           ctx->bal_active,
           ctx->bal_target_label,
           ctx->sys_stat);
}
