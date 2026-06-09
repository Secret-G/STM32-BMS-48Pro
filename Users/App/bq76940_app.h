#ifndef __BQ76940_APP_H
#define __BQ76940_APP_H

#include "bq76940_drv.h"
#include "bq76940_alarm.h"
#include "bq76940_protect.h"
#include "bq76200_exec.h"


typedef struct
{
    uint16_t diff_enter_mV;       /* 压差进入阈值 */
    uint16_t diff_exit_mV;        /* 压差退出阈值 */
    uint16_t min_cell_mV;         /* 允许均衡的最低最高单体电压门限 */
    int32_t  max_abs_current_mA;  /* 允许均衡的最大电流绝对值 */
} BQ76940_BalanceConfig_t;

/* BQ76940 应用层上下文
 * 这一层不是底层驱动，而是把“当前版本运行所需的数据”集中起来
 */
typedef struct
{
    /* 硬件寄存器与校准 */
    BQ76940_BasicRegs_t regs;
    BQ76940_AdcCalib_t calib;

    /* 状态寄存器 */
    uint8_t sys_stat;
    uint8_t sys_ctrl2;

    /* 单体采样数据 */
    uint16_t cell_raw[BQ76940_CELL_COUNT_9];
    uint16_t cell_mV[BQ76940_CELL_COUNT_9];
    uint32_t pack_total_mV;
    BQ76940_CellStats9_t cell_stats;

    /* 软件告警 */
    BQ76940_AlarmState9_t alarm_state;
    BQ76940_AlarmThreshold9_t alarm_th;

    /* 硬件保护配置 */
    BQ76940_HwProtectCfg_t hw_cfg;
	
		/* CC 原始值 */
		BQ76940_CCRaw_t cc_raw;

		/* 包电流换算结果 */
		int32_t pack_current_mA;

		/* 方向判定结果：
		 *  1  -> charge
		 *  0  -> near zero
		 * -1  -> discharge
		 */
		int8_t pack_current_dir;
		
		uint16_t ts1_raw_adc;
		int16_t  ts1_temp_dC;


		uint8_t ot_cutoff_active;   /* 是否已经因过温执行过关断动作 */
		
		uint8_t ut_chg_block_active;   /* 是否已经因低温禁止充电 */
		
		BQ76940_OcdScdConfig_t ocdscd_cfg;
		
		
		uint8_t fault_clear_once_enable;   /* 单次清位测试开关 */
		uint8_t fault_mask_active;         /* 当前激活的硬件故障位 */
		uint8_t sys_stat_before_clear;     /* 清位前 SYS_STAT */
		uint8_t sys_stat_after_clear;      /* 清位后 SYS_STAT */
		
		
		uint8_t hw_ocd_active;              /* 软件记录：是否已经发生过 OCD */
		uint8_t hw_scd_active;              /* 软件记录：是否已经发生过 SCD */
		uint8_t hw_dsg_block_active;        /* 是否已经因为 OCD/SCD 阻断放电 */
		uint8_t hw_fault_recover_once_enable; /* 手动触发一次 DSG 恢复 */
		
		
		BQ76940_CellBalRegs_t cellbal_wr;
		BQ76940_CellBalRegs_t cellbal_rd;

		uint8_t bal_test_once_enable;   /* 手动均衡测试开关：1=执行一次 */
		uint8_t bal_test_target_label;  /* 目标电芯编号，例如 1/2/5/6/7/10/11/12/15 */
		
		
		BQ76940_BalanceConfig_t bal_cfg;

		uint8_t bal_active;              /* 当前是否处于自动均衡状态 */
		uint8_t bal_target_label;        /* 当前正在均衡的单体编号 */

		BQ76940_CellBalRegs_t bal_auto_wr;
		BQ76940_CellBalRegs_t bal_auto_rd;
		
		/* BQ76200 执行层 */
    BQ76200_ExecCtx_t bq76200_exec;
				
		
} BQ76940_AppCtx_t;





/* 初始化默认配置
 * 1. 软件实验阈值
 * 2. 硬件保护目标值
 * 3. 运行状态清零
 */
void BQ76940_AppInitDefaultConfig(BQ76940_AppCtx_t *ctx);

/* 上电 bring-up + 自检 */
uint8_t BQ76940_AppBringUpAndSelfTest(BQ76940_AppCtx_t *ctx);

/* 周期运行一次 */
uint8_t BQ76940_AppRunCycle(BQ76940_AppCtx_t *ctx);

/*can发送函数*/
void BQ76940_AppSendCanTelemetry(const BQ76940_AppCtx_t *ctx);

#endif


