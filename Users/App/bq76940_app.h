#ifndef __BQ76940_APP_H
#define __BQ76940_APP_H

#include "bq76940_drv.h"
#include "bq76940_alarm.h"
#include "bq76940_protect.h"
#include "bq76200_exec.h"
#include "bq76940_app_balance.h"


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


/* BQ76940 应用层上下文
 * 这一层不是底层驱动，而是把“当前版本运行所需的数据”集中起来
 */
typedef struct BQ76940_AppCtx
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
uint8_t BQ76940_AppProtectUpdateBase(BQ76940_AppCtx_t *ctx);

/*ocd csd相关*/
void BQ76940_AppOcdScdRequestClear(BQ76940_OcdScdRequest_t *req);

uint8_t BQ76940_AppOcdScdDecide(const BQ76940_AppCtx_t *ctx,
                                BQ76940_OcdScdRequest_t *req);

uint8_t BQ76940_AppOcdScdApplyHw(const BQ76940_OcdScdRequest_t *req);

uint8_t BQ76940_AppOcdScdCommit(BQ76940_AppCtx_t *ctx,
                                const BQ76940_OcdScdRequest_t *req);


//过温保护的相关函数
void BQ76940_AppOtProtectRequestClear(BQ76940_OtProtectRequest_t *req);

uint8_t BQ76940_AppOtProtectDecide(const BQ76940_AppCtx_t *ctx,
                                    BQ76940_OtProtectRequest_t *req);

uint8_t BQ76940_AppOtProtectApplyHw(const BQ76940_OtProtectRequest_t *req);

uint8_t BQ76940_AppOtProtectCommit(BQ76940_AppCtx_t *ctx,
                                    const BQ76940_OtProtectRequest_t *req);

//低温保护的相关函数
void BQ76940_AppUtProtectRequestClear(BQ76940_UtProtectRequest_t *req);

uint8_t BQ76940_AppUtProtectDecide(const BQ76940_AppCtx_t *ctx,
                                    BQ76940_UtProtectRequest_t *req);

uint8_t BQ76940_AppUtProtectApplyHw(const BQ76940_UtProtectRequest_t *req);

uint8_t BQ76940_AppUtProtectCommit(BQ76940_AppCtx_t *ctx,
                                    const BQ76940_UtProtectRequest_t *req);


uint8_t BQ76940_AppSampleUpdate(BQ76940_AppCtx_t *ctx);
uint8_t BQ76940_AppProtectUpdate(BQ76940_AppCtx_t *ctx);
uint8_t BQ76940_AppControlUpdate(BQ76940_AppCtx_t *ctx);
void    BQ76940_AppPrintRuntime(const BQ76940_AppCtx_t *ctx);
void    BQ76940_AppSendCanTelemetry(const BQ76940_AppCtx_t *ctx);

/*
 * 只更新保护相关告警状态：
 *   UV / OV / DIFF
 *   OT / UT
 *
 * 注意：
 *   本函数不执行 CHG / DSG / CELLBAL 等硬件动作。
 *   不访问 I2C。
 */
uint8_t BQ76940_AppProtectUpdateAlarms(BQ76940_AppCtx_t *ctx);


#endif


