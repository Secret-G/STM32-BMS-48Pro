#include "bq76940_protect.h"
#include "bms_log.h"

#include "delay.h"
#include "stdio.h"

uint8_t BQ76940_ProtectBringUp(BQ76940_BasicRegs_t *regs,
                               BQ76940_AdcCalib_t *calib)
{
    uint8_t ret;

    if ((regs == 0) || (calib == 0))
    {
        return 1;
    }

    /* 1. 最小 bring-up 初始化 */
    ret = BQ76940_InitForBringUp();
    if (ret != 0)
    {
        return 11;
    }

    /* 2. 读基础寄存器 */
    ret = BQ76940_ReadBasicRegs(regs);
    if (ret != 0)
    {
        return 12;
    }

    /* 3. 读 ADC 校准参数 */
    ret = BQ76940_GetAdcCalib(calib);
    if (ret != 0)
    {
        return 13;
    }

    return 0;
}

uint8_t BQ76940_ProtectLoadConfig(const BQ76940_HwProtectCfg_t *cfg,
                                  const BQ76940_AdcCalib_t *calib,
                                  BQ76940_BasicRegs_t *regs)
{
    uint8_t ret;
    uint8_t ov_trip_cfg;
    uint8_t uv_trip_cfg;

    if ((cfg == 0) || (calib == 0) || (regs == 0))
    {
        return 1;
    }

    /* 1. 由目标 mV 反算 OV_TRIP / UV_TRIP */
    ov_trip_cfg = BQ76940_CalcOvTripFrommV(cfg->ov_target_mV,
                                           calib->gain_uV_per_lsb,
                                           calib->offset_mV);

    uv_trip_cfg = BQ76940_CalcUvTripFrommV(cfg->uv_target_mV,
                                           calib->gain_uV_per_lsb,
                                           calib->offset_mV);

    BMS_LOG_PERIODIC("[CFG] protect\r\n");
    BMS_LOG_PERIODIC("[CFG] OV:%d/%02X\r\n", cfg->ov_target_mV, ov_trip_cfg);
    BMS_LOG_PERIODIC("[CFG] UV:%d/%02X\r\n", cfg->uv_target_mV, uv_trip_cfg);

    /* 2. 加载保护参数 */
    ret = BQ76940_LoadProtectionParams(cfg->protect3, ov_trip_cfg, uv_trip_cfg);
    if (ret != 0)
    {
        return 21;
    }

    BMS_LOG_PERIODIC("[CFG] protect ok\r\n");

    delay_ms(5);

    /* 3. 再读基础寄存器确认 */
    ret = BQ76940_ReadBasicRegs(regs);
    if (ret != 0)
    {
        return 22;
    }

    BMS_LOG_PERIODIC("[CFG] readback ok\r\n");

    return 0;
}

uint8_t BQ76940_ProtectReadStatus(uint8_t *sys_stat, uint8_t *sys_ctrl2)
{
    uint8_t ret;

    if ((sys_stat == 0) || (sys_ctrl2 == 0))
    {
        return 1;
    }

    ret = BQ76940_ReadSysStat(sys_stat);
    if (ret != 0)
    {
        return 11;
    }

    ret = BQ76940_ReadSysCtrl2(sys_ctrl2);
    if (ret != 0)
    {
        return 12;
    }

    return 0;
}

void BQ76940_ProtectPrintStatus(uint8_t sys_stat, uint8_t sys_ctrl2)
{
    BQ76940_PrintSysStat(sys_stat);
    BQ76940_PrintSysCtrl2(sys_ctrl2);
}

uint8_t BQ76940_LoadProtectionParams(uint8_t protect3,
                                     uint8_t ov_trip,
                                     uint8_t uv_trip)
{
    uint8_t ret;
    uint8_t rd;

    /* 1. 写 PROTECT3 */
    ret = BQ76940_WriteReg_CRC(BQ76940_REG_PROTECT3, protect3);
    if (ret != 0) return 1;

    delay_ms(5);

    ret = BQ76940_ReadReg(BQ76940_REG_PROTECT3, &rd);
    if (ret != 0) return 11;
    if (rd != protect3) return 21;

    /* 2. 写 OV_TRIP */
    ret = BQ76940_WriteReg_CRC(BQ76940_REG_OV_TRIP, ov_trip);
    if (ret != 0) return 2;

    delay_ms(5);

    ret = BQ76940_ReadReg(BQ76940_REG_OV_TRIP, &rd);
    if (ret != 0) return 12;
    if (rd != ov_trip) return 22;

    /* 3. 写 UV_TRIP */
    ret = BQ76940_WriteReg_CRC(BQ76940_REG_UV_TRIP, uv_trip);
    if (ret != 0) return 3;

    delay_ms(5);

    ret = BQ76940_ReadReg(BQ76940_REG_UV_TRIP, &rd);
    if (ret != 0) return 13;
    if (rd != uv_trip) return 23;

    return 0;
}

uint8_t BQ76940_CalcOvTripFrommV(uint16_t ov_mV, uint16_t gain_uV_per_lsb, int16_t offset_mV)
{
    uint32_t full_adc;

    /* full_adc ≈ (ov_mV - offset_mV) / (gain_uV_per_lsb / 1000) */
    full_adc = ((uint32_t)(ov_mV - offset_mV) * 1000U) / gain_uV_per_lsb;

    /* 取14位值的中间8位 */
    return (uint8_t)((full_adc >> 4) & 0xFF);
}

uint8_t BQ76940_CalcUvTripFrommV(uint16_t uv_mV, uint16_t gain_uV_per_lsb, int16_t offset_mV)
{
    uint32_t full_adc;

    full_adc = ((uint32_t)(uv_mV - offset_mV) * 1000U) / gain_uV_per_lsb;

    return (uint8_t)((full_adc >> 4) & 0xFF);
}



uint8_t BQ76940_ProtectLoadOcdScd(const BQ76940_OcdScdConfig_t *cfg,
                                  BQ76940_BasicRegs_t *regs)
{
    uint8_t protect1;
    uint8_t protect2;

    if (cfg == 0)
    {
        return 1;
    }

    /* PROTECT1: SCD_DELAY[4:3] + SCD_THRESH[2:0] */
    protect1 = (uint8_t)(((cfg->scd_delay_code & 0x03U) << 3) |
                         (cfg->scd_thresh_code & 0x07U));

    /* PROTECT2: OCD_DELAY[6:4] + OCD_THRESH[3:0] */
    protect2 = (uint8_t)(((cfg->ocd_delay_code & 0x07U) << 4) |
                         (cfg->ocd_thresh_code & 0x0FU));

    if (BQ76940_WriteReg_CRC(BQ76940_REG_PROTECT1, protect1) != BQ76940_OK)
    {
        return 2;
    }

    if (BQ76940_WriteReg_CRC(BQ76940_REG_PROTECT2, protect2) != BQ76940_OK)
    {
        return 3;
    }

    if (regs != 0)
    {
        regs->protect1 = protect1;
        regs->protect2 = protect2;
    }

    return 0;
}

uint8_t BQ76940_ProtectReadFaultStatus(uint8_t *sys_stat)
{
    if (sys_stat == 0)
    {
        return 1;
    }

    if (BQ76940_ReadReg(BQ76940_REG_SYS_STAT, sys_stat) != BQ76940_OK)
    {
        return 2;
    }

    return 0;
}

void BQ76940_ProtectPrintFaultStatus(uint8_t sys_stat)
{
    printf("[HW FAULT STATUS]\r\n");
    printf("SYS_STAT = 0x%02X\r\n", sys_stat);
    printf("HW_UV    = %d\r\n", (sys_stat & BQ76940_SYS_STAT_UV)  ? 1 : 0);
    printf("HW_OV    = %d\r\n", (sys_stat & BQ76940_SYS_STAT_OV)  ? 1 : 0);
    printf("HW_OCD   = %d\r\n", (sys_stat & BQ76940_SYS_STAT_OCD) ? 1 : 0);
    printf("HW_SCD   = %d\r\n", (sys_stat & BQ76940_SYS_STAT_SCD) ? 1 : 0);
}


uint8_t BQ76940_ProtectGetActiveFaultMask(uint8_t sys_stat, uint8_t *fault_mask)
{
    if (fault_mask == 0)
    {
        return 1;
    }

    *fault_mask = (uint8_t)(sys_stat & BQ76940_SYS_STAT_HW_LATCH_MASK);

    return 0;
}

uint8_t BQ76940_ProtectClearFaultBits(uint8_t fault_mask)
{
    if (fault_mask == 0U)
    {
        return 0;
    }

    /* SYS_STAT: 写1清对应位 */
    return BQ76940_WriteReg_CRC(BQ76940_REG_SYS_STAT, fault_mask);
}

void BQ76940_ProtectPrintClearCompare(uint8_t before_stat,
                                      uint8_t clear_mask,
                                      uint8_t after_stat)
{
    printf("[HW FAULT CLEAR]\r\n");
    printf("BEFORE     = 0x%02X\r\n", before_stat);
    printf("CLEAR_MASK = 0x%02X\r\n", clear_mask);
    printf("AFTER      = 0x%02X\r\n", after_stat);
}
