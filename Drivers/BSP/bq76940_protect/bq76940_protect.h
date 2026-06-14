#ifndef __BQ76940_PROTECT_H
#define __BQ76940_PROTECT_H

#include "bq76940_drv.h"


/* =========================
 * SYS_STAT 位定义
 * BQ76940 SYS_STAT:
 * bit7 CC_READY
 * bit6 RSVD
 * bit5 DEVICE_XREADY
 * bit4 OVRD_ALERT
 * bit3 UV
 * bit2 OV
 * bit1 SCD
 * bit0 OCD
 * ========================= */
#define BQ76940_SYS_STAT_OCD             (1U << 0)
#define BQ76940_SYS_STAT_SCD             (1U << 1)
#define BQ76940_SYS_STAT_OV              (1U << 2)
#define BQ76940_SYS_STAT_UV              (1U << 3)
#define BQ76940_SYS_STAT_OVRD_ALERT      (1U << 4)
#define BQ76940_SYS_STAT_DEVICE_XREADY   (1U << 5)
#define BQ76940_SYS_STAT_RSVD            (1U << 6)

#ifndef BQ76940_SYS_STAT_CC_READY
#define BQ76940_SYS_STAT_CC_READY        (1U << 7)
#endif

#define BQ76940_SYS_STAT_CURRENT_FAULT_MASK \
    (BQ76940_SYS_STAT_OCD | BQ76940_SYS_STAT_SCD)

#define BQ76940_SYS_STAT_VOLTAGE_FAULT_MASK \
    (BQ76940_SYS_STAT_OV | BQ76940_SYS_STAT_UV)

#define BQ76940_SYS_STAT_AFE_FAULT_MASK \
    (BQ76940_SYS_STAT_DEVICE_XREADY)

#define BQ76940_SYS_STAT_ALERT_STATUS_MASK \
    (BQ76940_SYS_STAT_OVRD_ALERT)

#define BQ76940_SYS_STAT_EVENT_MASK \
    (BQ76940_SYS_STAT_CC_READY)

#define BQ76940_SYS_STAT_HW_LATCH_MASK \
    (BQ76940_SYS_STAT_CURRENT_FAULT_MASK | \
     BQ76940_SYS_STAT_VOLTAGE_FAULT_MASK | \
     BQ76940_SYS_STAT_AFE_FAULT_MASK | \
     BQ76940_SYS_STAT_ALERT_STATUS_MASK)


/* =========================
 * SCD 配置码
 * ========================= */
#define BQ76940_SCD_DELAY_50US    0x0
#define BQ76940_SCD_DELAY_100US   0x1
#define BQ76940_SCD_DELAY_200US   0x2
#define BQ76940_SCD_DELAY_400US   0x3

#define BQ76940_SCD_THRESH_44MV   0x0
#define BQ76940_SCD_THRESH_67MV   0x1
#define BQ76940_SCD_THRESH_89MV   0x2
#define BQ76940_SCD_THRESH_111MV  0x3
#define BQ76940_SCD_THRESH_133MV  0x4
#define BQ76940_SCD_THRESH_155MV  0x5
#define BQ76940_SCD_THRESH_178MV  0x6
#define BQ76940_SCD_THRESH_200MV  0x7

/* =========================
 * OCD 配置码
 * ========================= */
#define BQ76940_OCD_DELAY_10MS    0x0
#define BQ76940_OCD_DELAY_20MS    0x1
#define BQ76940_OCD_DELAY_40MS    0x2
#define BQ76940_OCD_DELAY_80MS    0x3
#define BQ76940_OCD_DELAY_160MS   0x4
#define BQ76940_OCD_DELAY_320MS   0x5
#define BQ76940_OCD_DELAY_640MS   0x6
#define BQ76940_OCD_DELAY_1280MS  0x7

#define BQ76940_OCD_THRESH_17MV   0x0
#define BQ76940_OCD_THRESH_22MV   0x1
#define BQ76940_OCD_THRESH_28MV   0x2
#define BQ76940_OCD_THRESH_33MV   0x3
#define BQ76940_OCD_THRESH_39MV   0x4
#define BQ76940_OCD_THRESH_44MV   0x5
#define BQ76940_OCD_THRESH_50MV   0x6
#define BQ76940_OCD_THRESH_56MV   0x7
#define BQ76940_OCD_THRESH_61MV   0x8
#define BQ76940_OCD_THRESH_67MV   0x9
#define BQ76940_OCD_THRESH_72MV   0xA
#define BQ76940_OCD_THRESH_78MV   0xB
#define BQ76940_OCD_THRESH_83MV   0xC
#define BQ76940_OCD_THRESH_89MV   0xD
#define BQ76940_OCD_THRESH_94MV   0xE
#define BQ76940_OCD_THRESH_100MV  0xF


typedef struct
{
    uint8_t scd_delay_code;
    uint8_t scd_thresh_code;
    uint8_t ocd_delay_code;
    uint8_t ocd_thresh_code;
} BQ76940_OcdScdConfig_t;

uint8_t BQ76940_ProtectLoadOcdScd(const BQ76940_OcdScdConfig_t *cfg,
                                  BQ76940_BasicRegs_t *regs);

uint8_t BQ76940_ProtectReadFaultStatus(uint8_t *sys_stat);

void BQ76940_ProtectPrintFaultStatus(uint8_t sys_stat);
/* 硬件保护参数配置
 * 这里是“硬件保护层”的真实目标值，不是软件实验阈值
 */
typedef struct
{
    uint16_t ov_target_mV;   /* 例如 4200mV */
    uint16_t uv_target_mV;   /* 例如 3000mV */
    uint8_t  protect3;       /* 例如 0x50 -> OV延时2s, UV延时4s */
} BQ76940_HwProtectCfg_t;

/* 上电后最小 bring-up：
 * 1. BQ76940_InitForBringUp()
 * 2. 读基础寄存器
 * 3. 读 ADC 校准参数
 */
uint8_t BQ76940_ProtectBringUp(BQ76940_BasicRegs_t *regs,
                               BQ76940_AdcCalib_t *calib);

/* 计算并加载硬件保护参数：
 * 1. 由目标 mV 计算 OV_TRIP / UV_TRIP
 * 2. 写 PROTECT3 / OV_TRIP / UV_TRIP
 * 3. 再读基础寄存器确认
 */
uint8_t BQ76940_ProtectLoadConfig(const BQ76940_HwProtectCfg_t *cfg,
                                  const BQ76940_AdcCalib_t *calib,
                                  BQ76940_BasicRegs_t *regs);

/* 读取当前硬件状态寄存器 */
uint8_t BQ76940_ProtectReadStatus(uint8_t *sys_stat, uint8_t *sys_ctrl2);

/* 打印当前硬件状态寄存器 */
void BQ76940_ProtectPrintStatus(uint8_t sys_stat, uint8_t sys_ctrl2);
																	
																	
uint8_t BQ76940_LoadProtectionParams(uint8_t protect3,
                                     uint8_t ov_trip,
                                     uint8_t uv_trip);
																	
uint8_t BQ76940_CalcOvTripFrommV(uint16_t ov_mV, uint16_t gain_uV_per_lsb, int16_t offset_mV);
																	
uint8_t BQ76940_CalcUvTripFrommV(uint16_t uv_mV, uint16_t gain_uV_per_lsb, int16_t offset_mV);
																	
																	
																	
uint8_t BQ76940_ProtectGetActiveFaultMask(uint8_t sys_stat, uint8_t *fault_mask);

uint8_t BQ76940_ProtectClearFaultBits(uint8_t fault_mask);

void BQ76940_ProtectPrintClearCompare(uint8_t before_stat,
                                      uint8_t clear_mask,
                                      uint8_t after_stat);

#endif
	

