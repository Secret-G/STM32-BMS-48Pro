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

/*
 * 自动均衡动作类型
 *
 * NONE:
 *   本轮不需要操作 CELLBAL。
 *
 * START:
 *   本轮需要开启某个单体的均衡。
 *
 * STOP:
 *   本轮需要关闭所有自动均衡。
 */
#define BQ76940_BAL_ACTION_NONE      0U
#define BQ76940_BAL_ACTION_START     1U
#define BQ76940_BAL_ACTION_STOP      2U

/*
 * 自动均衡停止原因
 */
#define BQ76940_BAL_REASON_NONE          0U
#define BQ76940_BAL_REASON_NOT_ALLOWED   1U
#define BQ76940_BAL_REASON_DIFF_EXIT     2U

/*
 * 自动均衡请求结构体
 *
 * 作用：
 *   把“均衡判断”和“CELLBAL 硬件写入”拆开。
 *
 * 使用流程：
 *   1. Decide  阶段：根据 app 状态生成 req
 *   2. ApplyHw 阶段：根据 req 写 BQ76940 CELLBAL
 *   3. Commit  阶段：将执行结果提交回 app
 */
typedef struct
{
    uint8_t action;        /* NONE / START / STOP */
    uint8_t target_label;  /* START 时的目标电芯编号 */
    uint8_t reason;        /* STOP 原因 */

    BQ76940_CellBalRegs_t wr;  /* 准备写入的 CELLBAL */
    BQ76940_CellBalRegs_t rd;  /* 写入后读回的 CELLBAL */
} BQ76940_BalanceRequest_t;



typedef struct
{
    uint8_t bringup_attempt_count;   /* 上电 bring-up 尝试次数 */
    uint8_t bringup_fault_active;    /* 上电 bring-up 是否最终失败 */
    uint8_t bringup_last_stage;      /* 最近一次失败发生在哪个阶段 */
    uint8_t bringup_last_error;      /* 最近一次失败的底层错误码 */
} BQ76940_DiagState_t;



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
		
		
				    /* 诊断状态 */
    BQ76940_DiagState_t diag_state;
		
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


/* 故障安全关断：关闭 BQ76200 驱动引脚，尝试关闭 BQ76940 FET 和均衡 */
uint8_t BQ76940_AppForceSafeOff(BQ76940_AppCtx_t *ctx);


/*故障错误发送can帧*/
void BQ76940_AppSendBringUpFaultCan(const BQ76940_AppCtx_t *ctx,
                                    uint8_t main_ret,
                                    uint8_t safe_off_result);



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
uint8_t BQ76940_AppSampleCommit(BQ76940_AppCtx_t *ctx,
                                const BQ76940_AppSampleData_t *sample);


/*自动均衡相关*/
void BQ76940_AppBalanceRequestClear(BQ76940_BalanceRequest_t *req);

uint8_t BQ76940_AppBalanceDecide(const BQ76940_AppCtx_t *ctx,
                                  BQ76940_BalanceRequest_t *req);

uint8_t BQ76940_AppBalanceApplyHw(BQ76940_BalanceRequest_t *req);

uint8_t BQ76940_AppBalanceCommit(BQ76940_AppCtx_t *ctx,
                                  const BQ76940_BalanceRequest_t *req);


uint8_t BQ76940_AppSampleUpdate(BQ76940_AppCtx_t *ctx);
uint8_t BQ76940_AppProtectUpdate(BQ76940_AppCtx_t *ctx);
uint8_t BQ76940_AppBalanceUpdate(BQ76940_AppCtx_t *ctx);
uint8_t BQ76940_AppControlUpdate(BQ76940_AppCtx_t *ctx);
void    BQ76940_AppPrintRuntime(const BQ76940_AppCtx_t *ctx);
void    BQ76940_AppSendCanTelemetry(const BQ76940_AppCtx_t *ctx);

#endif


