#ifndef __BQ76940_DRV_H
#define __BQ76940_DRV_H

#include "sys.h"
#include "stdio.h"

/* BQ76940 的 7 位 I2C 地址 */
#define BQ76940_I2C_ADDR           0x08

/* ==================== 常用寄存器 ==================== */
#define BQ76940_REG_SYS_STAT       0x00
#define BQ76940_REG_CELLBAL1       0x01
#define BQ76940_REG_CELLBAL2       0x02
#define BQ76940_REG_CELLBAL3       0x03
#define BQ76940_REG_SYS_CTRL1      0x04
#define BQ76940_REG_SYS_CTRL2      0x05
#define BQ76940_REG_PROTECT1       0x06
#define BQ76940_REG_PROTECT2       0x07
#define BQ76940_REG_PROTECT3       0x08
#define BQ76940_REG_OV_TRIP        0x09
#define BQ76940_REG_UV_TRIP        0x0A
#define BQ76940_REG_CC_CFG         0x0B
#define BQ76940_REG_CC_HI         0x32
#define BQ76940_REG_CC_LO         0x33


/* =========================
 * SYS_STAT 位定义
 * ========================= */
#define BQ76940_SYS_STAT_CC_READY    (1U << 7)

/* =========================
 * SYS_CTRL2 位定义
 * ========================= */
#define BQ76940_SYS_CTRL2_CC_EN      (1U << 6)
#define BQ76940_SYS_CTRL2_CC_ONESHOT (1U << 5)


/* =========================
 * 返回码定义
 * ========================= */
#define BQ76940_OK                   0
#define BQ76940_ERR_PARAM            1
#define BQ76940_ERR_COMM             2
#define BQ76940_ERR_TIMEOUT          3

/* =========================
 * CC 原始结果结构体
 * ========================= */
typedef struct
{
    uint8_t  raw_hi;     /* 原始高字节 */
    uint8_t  raw_lo;     /* 原始低字节 */
    uint16_t raw_u16;    /* 合并后的无符号 16 位 */
    int16_t  raw_s16;    /* 合并后的有符号 16 位（2's complement） */
} BQ76940_CCRaw_t;



/* ==================== 电压采样相关寄存器 ==================== */
#define BQ76940_REG_VC1_HI         0x0C
#define BQ76940_REG_VC1_LO         0x0D
#define BQ76940_REG_VC2_HI         0x0E
#define BQ76940_REG_VC2_LO         0x0F
#define BQ76940_REG_VC3_HI         0x10
#define BQ76940_REG_VC3_LO         0x11
#define BQ76940_REG_VC4_HI         0x12
#define BQ76940_REG_VC4_LO         0x13
#define BQ76940_REG_VC5_HI         0x14
#define BQ76940_REG_VC5_LO         0x15
#define BQ76940_REG_VC6_HI         0x16
#define BQ76940_REG_VC6_LO         0x17
#define BQ76940_REG_VC7_HI         0x18
#define BQ76940_REG_VC7_LO         0x19
#define BQ76940_REG_VC8_HI         0x1A
#define BQ76940_REG_VC8_LO         0x1B
#define BQ76940_REG_VC9_HI         0x1C
#define BQ76940_REG_VC9_LO         0x1D
#define BQ76940_REG_VC10_HI        0x1E
#define BQ76940_REG_VC10_LO        0x1F
#define BQ76940_REG_VC11_HI        0x20
#define BQ76940_REG_VC11_LO        0x21
#define BQ76940_REG_VC12_HI        0x22
#define BQ76940_REG_VC12_LO        0x23
#define BQ76940_REG_VC13_HI        0x24
#define BQ76940_REG_VC13_LO        0x25
#define BQ76940_REG_VC14_HI        0x26
#define BQ76940_REG_VC14_LO        0x27
#define BQ76940_REG_VC15_HI        0x28
#define BQ76940_REG_VC15_LO        0x29

/* ==================== ADC 校准相关寄存器 ==================== */
#define BQ76940_REG_ADCGAIN1       0x50
#define BQ76940_REG_ADCOFFSET      0x51
#define BQ76940_REG_ADCGAIN2       0x59


/* SYS_CTRL2 位定义（如果前面已有就不要重复定义） */
#define BQ76940_SYS_CTRL2_DSG_ON      (1U << 1)
#define BQ76940_SYS_CTRL2_CHG_ON      (1U << 0)


#define BQ76940_CELL_COUNT_9       9

typedef struct
{
    uint8_t sys_stat;
    uint8_t sys_ctrl1;
    uint8_t sys_ctrl2;
    uint8_t protect1;
    uint8_t protect2;
    uint8_t protect3;
    uint8_t ov_trip;
    uint8_t uv_trip;
    uint8_t cc_cfg;
} BQ76940_BasicRegs_t;



/* ==================== 温度相关寄存器 ==================== */
#define BQ76940_REG_TS1_HI         0x2C
#define BQ76940_REG_TS1_LO         0x2D

typedef struct
{
    uint16_t raw_adc;      /* TS1 原始 ADC */
    int16_t  temp_dC;      /* 温度，单位 0.1°C */
} BQ76940_TempTs1_t;


typedef struct
{
    uint16_t gain_uV_per_lsb;
    int16_t  offset_mV;
} BQ76940_AdcCalib_t;


/* 9 节单体统计结果
 * max_cell_label / min_cell_label 不是数组下标，
 * 而是实际显示用的电芯编号，例如 1、2、5、6、7、10、11、12、15
 */
typedef struct
{
    uint16_t max_mV;          /* 最高单体电压 */
    uint16_t min_mV;          /* 最低单体电压 */
    uint16_t diff_mV;         /* 单体压差 = max - min */

    uint8_t  max_cell_label;  /* 最高电压对应哪一路映射通道 */
    uint8_t  min_cell_label;  /* 最低电压对应哪一路映射通道 */
} BQ76940_CellStats9_t;


typedef struct
{
    const char *name;   /* 这一位显示名 */
    uint8_t bit;        /* 位号 */
} BQ76940_RegBitDesc_t;



typedef struct
{
    uint8_t cellbal1;
    uint8_t cellbal2;
    uint8_t cellbal3;
} BQ76940_CellBalRegs_t;


/* 基础读写 */
uint8_t BQ76940_ReadReg(uint8_t reg_addr, uint8_t *data);
uint8_t BQ76940_WriteReg(uint8_t reg_addr, uint8_t data);
uint8_t BQ76940_WriteReg_CRC(uint8_t reg_addr, uint8_t data);



/* 基础寄存器读取 */
uint8_t BQ76940_ReadBasicRegs(BQ76940_BasicRegs_t *regs);

/* ADC 校准参数读取 */
uint8_t BQ76940_GetAdcCalib(BQ76940_AdcCalib_t *calib);

/* 读单节电池原始 ADC 值 */
uint8_t BQ76940_ReadCellVoltageRaw(uint8_t reg_hi, uint8_t reg_lo, uint16_t *raw_adc);

/* 原始 ADC 值换算成 mV */
uint8_t BQ76940_ConvertCellVoltage_mV(uint16_t raw_adc,
                                      const BQ76940_AdcCalib_t *calib,
                                      uint16_t *voltage_mV);

/* 读取第 1 节电池电压，bring-up 调试用 */
uint8_t BQ76940_ReadCell1Voltage_mV(const BQ76940_AdcCalib_t *calib,
                                    uint16_t *raw_adc,
                                    uint16_t *voltage_mV);

/* 读取“板子实际 9 节映射”的单节电压
 * cell_index 取 1~9，但内部映射不是连续 VC1~VC9
 */
uint8_t BQ76940_ReadMappedCellVoltage9_mV(uint8_t cell_index,
                                          const BQ76940_AdcCalib_t *calib,
                                          uint16_t *raw_adc,
                                          uint16_t *voltage_mV);

/* 读取板子实际 9 节电压 */
uint8_t BQ76940_ReadAllMappedCellVoltages9_mV(const BQ76940_AdcCalib_t *calib,
                                              uint16_t raw_adc[BQ76940_CELL_COUNT_9],
                                              uint16_t voltage_mV[BQ76940_CELL_COUNT_9]);

/* 计算 9 节总压 */
uint32_t BQ76940_CalcPackVoltage9_mV(const uint16_t voltage_mV[BQ76940_CELL_COUNT_9]);


/* 分析 9 节映射电压，求：
 * 1. 最高单体电压
 * 2. 最低单体电压
 * 3. 单体压差
 */
uint8_t BQ76940_AnalyzeCellVoltages9(const uint16_t voltage_mV[BQ76940_CELL_COUNT_9],
                                     BQ76940_CellStats9_t *stats);



																	
uint8_t BQ76940_InitForBringUp(void);
																	

uint8_t BQ76940_ClearSysStatBits(uint8_t mask);
																								
																								
uint8_t BQ76940_ReadSysStat(uint8_t *sys_stat);

void BQ76940_PrintSysStat(uint8_t sys_stat);
																								
				
uint8_t BQ76940_ReadSysCtrl2(uint8_t *sys_ctrl2);
void BQ76940_PrintSysCtrl2(uint8_t sys_ctrl2);		


uint8_t BQ76940_ReadRegs(uint8_t start_reg, uint8_t *buf, uint8_t len);

/* CC 相关接口 */
uint8_t BQ76940_CC_StartOneShot(void);
uint8_t BQ76940_CC_WaitReady(uint16_t timeout_ms);
uint8_t BQ76940_CC_ReadRaw(BQ76940_CCRaw_t *cc);

/* CC 原始值换算为包电流（mA）
 * rsense_uohm: 采样电阻，单位 μΩ
 * 例如：
 *   1mΩ  = 1000μΩ
 *   4mΩ  = 4000μΩ
 */
uint8_t BQ76940_CC_ConvertToCurrent_mA(int16_t cc_raw_s16,
                                       uint32_t rsense_uohm,
                                       int32_t *current_mA);
																			 


/* TS1 温度采样相关接口 */
uint8_t BQ76940_ReadTS1Raw(uint16_t *raw_adc);
uint8_t BQ76940_ConvertTS1Temp_dC(uint16_t raw_adc, int16_t *temp_dC);


/* FET 控制接口 */
uint8_t BQ76940_SetFETState(uint8_t chg_on, uint8_t dsg_on);

uint8_t BQ76940_SetCHGState(uint8_t chg_on);


uint8_t BQ76940_SetDSGState(uint8_t dsg_on);



uint8_t BQ76940_ReadCellBalRegs(BQ76940_CellBalRegs_t *regs);
uint8_t BQ76940_WriteCellBalRegs(const BQ76940_CellBalRegs_t *regs);
void    BQ76940_ClearCellBalRegs(BQ76940_CellBalRegs_t *regs);

/* 根据实际电芯编号（1~15）构造单节均衡位 */
uint8_t BQ76940_BuildSingleCellBalMask(uint8_t cell_label,
                                       BQ76940_CellBalRegs_t *regs);

#endif
