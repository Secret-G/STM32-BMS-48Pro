#include "bq76940_app.h"
#include "bq76940_print.h"

#include "bq76200_exec.h"
#include "io_ctrl.h"
#include "delay.h"
#include "stdio.h"
#include "string.h"


/* BQ76940 上电 bring-up 最大尝试次数 */
#define BQ76940_APP_BRINGUP_RETRY_LIMIT     3U

/* BQ76940 bring-up 阶段码 */
#define BQ76940_BRINGUP_STAGE_WAKE          1U
#define BQ76940_BRINGUP_STAGE_BASIC         2U
#define BQ76940_BRINGUP_STAGE_HW_CONFIG     3U
#define BQ76940_BRINGUP_STAGE_STATUS        4U
#define BQ76940_BRINGUP_STAGE_OCDSCD        5U
#define BQ76940_BRINGUP_STAGE_SAMPLE        6U





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
