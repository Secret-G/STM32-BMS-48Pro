#include "bq76940_drv.h"
#include "bms_log.h"
#include "soft_i2c1.h"
#include "delay.h"
#include <math.h>

/* ==================== 内部辅助函数 ==================== */

/* CRC8 计算函数
 * 这里直接照配套例程的实现思路移植：
 * CRC8(DataBuffer, 3, 7)
 * key = 7
 */
static uint8_t BQ76940_CRC8(uint8_t *ptr, uint8_t len, uint8_t key)
{
    uint8_t i;
    uint8_t crc = 0;

    while (len-- != 0)
    {
        for (i = 0x80; i != 0; i /= 2)
        {
            if ((crc & 0x80) != 0)
            {
                crc *= 2;
                crc ^= key;
            }
            else
            {
                crc *= 2;
            }

            if ((*ptr & i) != 0)
            {
                crc ^= key;
            }
        }
        ptr++;
    }

    return crc;
}

/* ==================== 对外接口 ==================== */

/* 普通读寄存器
 * 当前你的普通读已经验证可用，所以保留
 */
uint8_t BQ76940_ReadReg(uint8_t reg_addr, uint8_t *data)
{
    if (data == 0)
    {
        return 1;
    }

    return SoftI2C1_ReadReg(BQ76940_I2C_ADDR, reg_addr, data);
}

/* 普通写寄存器
 * 先保留，后面对比 CRC 写效果
 */
uint8_t BQ76940_WriteReg(uint8_t reg_addr, uint8_t data)
{
    return SoftI2C1_WriteReg(BQ76940_I2C_ADDR, reg_addr, data);
}

/* CRC 写寄存器
 * 参考配套例程：
 * DataBuffer[0] = 0x08 << 1
 * DataBuffer[1] = WriteAddr
 * DataBuffer[2] = Data
 * DataBuffer[3] = CRC8(DataBuffer, 3, 7)
 */
uint8_t BQ76940_WriteReg_CRC(uint8_t reg_addr, uint8_t data)
{
    uint8_t tx[4];

    tx[0] = (BQ76940_I2C_ADDR << 1);   /* 设备地址 + 写位0 */
    tx[1] = reg_addr;
    tx[2] = data;
    tx[3] = BQ76940_CRC8(tx, 3, 7);

    SoftI2C1_Start();

    /* 发送设备地址 */
    SoftI2C1_SendByte(tx[0]);
    if (SoftI2C1_WaitAck())
    {
        SoftI2C1_Stop();
        return 1;
    }

    /* 发送寄存器地址 */
    SoftI2C1_SendByte(tx[1]);
    if (SoftI2C1_WaitAck())
    {
        SoftI2C1_Stop();
        return 2;
    }

    /* 发送数据 */
    SoftI2C1_SendByte(tx[2]);
    if (SoftI2C1_WaitAck())
    {
        SoftI2C1_Stop();
        return 3;
    }

    /* 发送 CRC */
    SoftI2C1_SendByte(tx[3]);
    if (SoftI2C1_WaitAck())
    {
        SoftI2C1_Stop();
        return 4;
    }

    SoftI2C1_Stop();

    /* 配套例程里每次写后都 delay_ms(10) */
    delay_ms(10);

    return 0;
}

uint8_t BQ76940_ReadBasicRegs(BQ76940_BasicRegs_t *regs)
{
    if (regs == 0)
    {
        return 1;
    }

    if (BQ76940_ReadReg(BQ76940_REG_SYS_STAT,  &regs->sys_stat)  != 0) return 11;
    if (BQ76940_ReadReg(BQ76940_REG_SYS_CTRL1, &regs->sys_ctrl1) != 0) return 12;
    if (BQ76940_ReadReg(BQ76940_REG_SYS_CTRL2, &regs->sys_ctrl2) != 0) return 13;
    if (BQ76940_ReadReg(BQ76940_REG_PROTECT1,  &regs->protect1)  != 0) return 14;
    if (BQ76940_ReadReg(BQ76940_REG_PROTECT2,  &regs->protect2)  != 0) return 15;
    if (BQ76940_ReadReg(BQ76940_REG_PROTECT3,  &regs->protect3)  != 0) return 16;
    if (BQ76940_ReadReg(BQ76940_REG_OV_TRIP,   &regs->ov_trip)   != 0) return 17;
    if (BQ76940_ReadReg(BQ76940_REG_UV_TRIP,   &regs->uv_trip)   != 0) return 18;
    if (BQ76940_ReadReg(BQ76940_REG_CC_CFG,    &regs->cc_cfg)    != 0) return 19;

    return 0;
}

uint8_t BQ76940_InitForBringUp(void)
{
    uint8_t ret = 0;

    static const uint8_t reg_table[] = {
        BQ76940_REG_SYS_STAT,
        BQ76940_REG_CELLBAL1,
        BQ76940_REG_CELLBAL2,
        BQ76940_REG_CELLBAL3,
        BQ76940_REG_SYS_CTRL1,
        BQ76940_REG_SYS_CTRL2,
        BQ76940_REG_PROTECT1,
        BQ76940_REG_PROTECT2,
        BQ76940_REG_PROTECT3,
        BQ76940_REG_CC_CFG
    };

    static const uint8_t data_table[] = {
        0xFF,   /* 清 SYS_STAT */
        0x00,   /* 关均衡 */
        0x00,
        0x00,
        0x18,   /* SYS_CTRL1: 使能 ADC */
        0x43,   /* SYS_CTRL2: 保持你当前已验证通过的配置 */
        0x00,   /* OCD/SCD 暂不启用 */
        0x00,
        0x00,   /* PROTECT3 = 0x00 -> UV/OV 延时都为 1s */
        0x19    /* CC_CFG must be 0x19 */
    };

    uint8_t i;

    for (i = 0; i < sizeof(reg_table); i++)
    {
        ret = BQ76940_WriteReg_CRC(reg_table[i], data_table[i]);
        if (ret != 0)
        {
            return (20 + i);
        }
    }

    return 0;
}



/* 读取 ADC 校准参数
 * 这一步直接参考你上传的配套工程：
 * 1. 读 ADCGAIN1 / ADCGAIN2
 * 2. 组合出 ADC_GAIN
 * 3. gain = 365 + ADC_GAIN
 * 4. 读 ADCOFFSET
 */
uint8_t BQ76940_GetAdcCalib(BQ76940_AdcCalib_t *calib)
{
    uint8_t gain1 = 0;
    uint8_t gain2 = 0;
    uint8_t adc_offset = 0;
    uint8_t adc_gain = 0;

    if (calib == 0)
    {
        return 1;
    }

    /* 配套代码里就是先读这 3 个寄存器 */
    if (BQ76940_ReadReg(BQ76940_REG_ADCGAIN1, &gain1) != 0)
    {
        return 2;
    }

    if (BQ76940_ReadReg(BQ76940_REG_ADCGAIN2, &gain2) != 0)
    {
        return 3;
    }

    if (BQ76940_ReadReg(BQ76940_REG_ADCOFFSET, &adc_offset) != 0)
    {
        return 4;
    }

    /* 参考配套代码的位拼接方式 */
    adc_gain = ((gain1 & 0x0C) << 1) + ((gain2 & 0xE0) >> 5);

    /* 配套代码中的公式：GAIN = 365 + ADC_GAIN */
    calib->gain_uV_per_lsb = (uint16_t)(365 + adc_gain);

    /* 当前先严格按配套代码思路，直接把 ADCOFFSET 当成整数使用 */
    calib->offset_mV = (int16_t)((int8_t)adc_offset);

    return 0;
}

uint8_t BQ76940_ReadRegs(uint8_t start_reg, uint8_t *buf, uint8_t len)
{
    uint8_t i;

    if ((buf == 0) || (len == 0))
    {
        return BQ76940_ERR_PARAM;
    }

    /* 1. 起始信号 */
    SoftI2C1_Start();

    /* 2. 发送器件地址 + 写 */
    SoftI2C1_SendByte((BQ76940_I2C_ADDR << 1) | 0x00);
    if (SoftI2C1_WaitAck())
    {
        SoftI2C1_Stop();
        return 1;
    }

    /* 3. 发送起始寄存器地址 */
    SoftI2C1_SendByte(start_reg);
    if (SoftI2C1_WaitAck())
    {
        SoftI2C1_Stop();
        return 2;
    }

    /* 4. 重启信号 */
    SoftI2C1_Start();

    /* 5. 发送器件地址 + 读 */
    SoftI2C1_SendByte((BQ76940_I2C_ADDR << 1) | 0x01);
    if (SoftI2C1_WaitAck())
    {
        SoftI2C1_Stop();
        return 3;
    }

    /* 6. 连续读多个字节
     * 前 len-1 个字节读完后发 ACK
     * 最后 1 个字节读完后发 NACK
     */
    for (i = 0; i < len; i++)
    {
        if (i < (uint8_t)(len - 1))
        {
            buf[i] = SoftI2C1_ReadByte(1);
        }
        else
        {
            buf[i] = SoftI2C1_ReadByte(0);
        }
    }

    /* 7. 停止信号 */
    SoftI2C1_Stop();

    return BQ76940_OK;
}

/* 读取单节电池的原始 ADC 值
 * reg_hi / reg_lo 由调用者传入，这样后面扩到 VC2~VC15 很方便
 */
uint8_t BQ76940_ReadCellVoltageRaw(uint8_t reg_hi, uint8_t reg_lo, uint16_t *raw_adc)
{
    uint8_t hi = 0;
    uint8_t lo = 0;

    if (raw_adc == 0)
    {
        return 1;
    }

    if (BQ76940_ReadReg(reg_hi, &hi) != 0)
    {
        return 2;
    }

    if (BQ76940_ReadReg(reg_lo, &lo) != 0)
    {
        return 3;
    }

    *raw_adc = ((uint16_t)hi << 8) | lo;

    return 0;
}

/* 把原始 ADC 值换算成 mV
 * 参考配套代码公式：
 * cell_mV = ((raw_adc * GAIN) / 1000) + ADC_offset
 */
uint8_t BQ76940_ConvertCellVoltage_mV(uint16_t raw_adc,
                                      const BQ76940_AdcCalib_t *calib,
                                      uint16_t *voltage_mV)
{
    int32_t temp_mV = 0;

    if ((calib == 0) || (voltage_mV == 0))
    {
        return 1;
    }

    temp_mV = (int32_t)(((uint32_t)raw_adc * calib->gain_uV_per_lsb) / 1000U);
    temp_mV += (int32_t)calib->offset_mV;

    if (temp_mV < 0)
    {
        temp_mV = 0;
    }
    else if (temp_mV > 65535)
    {
        temp_mV = 65535;
    }

    *voltage_mV = (uint16_t)temp_mV;

    return 0;
}

/* 直接读取第 1 节电池电压
 * 当前先做 VC1，验证通了以后再扩到 VC2~VC15
 */
uint8_t BQ76940_ReadCell1Voltage_mV(const BQ76940_AdcCalib_t *calib,
                                    uint16_t *raw_adc,
                                    uint16_t *voltage_mV)
{
    uint8_t ret = 0;
    uint16_t local_raw = 0;

    if ((calib == 0) || (raw_adc == 0) || (voltage_mV == 0))
    {
        return 1;
    }

    ret = BQ76940_ReadCellVoltageRaw(BQ76940_REG_VC1_HI,
                                     BQ76940_REG_VC1_LO,
                                     &local_raw);
    if (ret != 0)
    {
        return 2;
    }

    ret = BQ76940_ConvertCellVoltage_mV(local_raw, calib, voltage_mV);
    if (ret != 0)
    {
        return 3;
    }

    *raw_adc = local_raw;

    return 0;
}

/* 读取指定单节电压
 * cell_index: 1~9
 * 当前先按 9 节来做，和你实际电池数量一致
 */
/* 读取板子实际 9 节映射中的某一节电压
 * 这里不是连续 VC1~VC9，而是参考配套代码实际使用的 9 路：
 * VC1、VC2、VC5、VC6、VC7、VC10、VC11、VC12、VC15
 */
uint8_t BQ76940_ReadMappedCellVoltage9_mV(uint8_t cell_index,
                                          const BQ76940_AdcCalib_t *calib,
                                          uint16_t *raw_adc,
                                          uint16_t *voltage_mV)
{
    static const uint8_t cell_hi_reg[BQ76940_CELL_COUNT_9] =
    {
        BQ76940_REG_VC1_HI,
        BQ76940_REG_VC2_HI,
        BQ76940_REG_VC5_HI,
        BQ76940_REG_VC6_HI,
        BQ76940_REG_VC7_HI,
        BQ76940_REG_VC10_HI,
        BQ76940_REG_VC11_HI,
        BQ76940_REG_VC12_HI,
        BQ76940_REG_VC15_HI
    };

    static const uint8_t cell_lo_reg[BQ76940_CELL_COUNT_9] =
    {
        BQ76940_REG_VC1_LO,
        BQ76940_REG_VC2_LO,
        BQ76940_REG_VC5_LO,
        BQ76940_REG_VC6_LO,
        BQ76940_REG_VC7_LO,
        BQ76940_REG_VC10_LO,
        BQ76940_REG_VC11_LO,
        BQ76940_REG_VC12_LO,
        BQ76940_REG_VC15_LO
    };

    uint8_t ret = 0;
    uint16_t local_raw = 0;

    if ((cell_index < 1) || (cell_index > BQ76940_CELL_COUNT_9))
    {
        return 1;
    }

    if ((calib == 0) || (raw_adc == 0) || (voltage_mV == 0))
    {
        return 2;
    }

    /* 先读该映射通道的原始 ADC 值 */
    ret = BQ76940_ReadCellVoltageRaw(cell_hi_reg[cell_index - 1],
                                     cell_lo_reg[cell_index - 1],
                                     &local_raw);
    if (ret != 0)
    {
        return 3;
    }

    /* 再按校准参数换算成 mV */
    ret = BQ76940_ConvertCellVoltage_mV(local_raw, calib, voltage_mV);
    if (ret != 0)
    {
        return 4;
    }

    *raw_adc = local_raw;

    return 0;
}

/* 一次性读取板子实际 9 节映射电压 */
uint8_t BQ76940_ReadAllMappedCellVoltages9_mV(const BQ76940_AdcCalib_t *calib,
                                              uint16_t raw_adc[BQ76940_CELL_COUNT_9],
                                              uint16_t voltage_mV[BQ76940_CELL_COUNT_9])
{
    uint8_t i;
    uint8_t ret = 0;

    if ((calib == 0) || (raw_adc == 0) || (voltage_mV == 0))
    {
        return 1;
    }

    for (i = 0; i < BQ76940_CELL_COUNT_9; i++)
    {
        ret = BQ76940_ReadMappedCellVoltage9_mV((uint8_t)(i + 1),
                                                calib,
                                                &raw_adc[i],
                                                &voltage_mV[i]);
        if (ret != 0)
        {
            /* 10~18 分别表示第 1~9 路映射通道读取失败 */
            return (uint8_t)(10 + i);
        }
    }

    return 0;
}

/* 计算 9 节总压 */
uint32_t BQ76940_CalcPackVoltage9_mV(const uint16_t voltage_mV[BQ76940_CELL_COUNT_9])
{
    uint8_t i;
    uint32_t total_mV = 0;

    if (voltage_mV == 0)
    {
        return 0;
    }

    for (i = 0; i < BQ76940_CELL_COUNT_9; i++)
    {
        total_mV += voltage_mV[i];
    }

    return total_mV;
}

/* 分析 9 节映射电压
 * 输入:
 *   voltage_mV[] : 9 路映射单体电压
 * 输出:
 *   stats        : 最高/最低/压差结果
 *
 * 当前这块板子的 9 路映射标签顺序是：
 * 1, 2, 5, 6, 7, 10, 11, 12, 15
 */
uint8_t BQ76940_AnalyzeCellVoltages9(const uint16_t voltage_mV[BQ76940_CELL_COUNT_9],
                                     BQ76940_CellStats9_t *stats)
{
    static const uint8_t cell_label[BQ76940_CELL_COUNT_9] = {1, 2, 5, 6, 7, 10, 11, 12, 15};

    uint8_t i;
    uint8_t max_idx = 0;
    uint8_t min_idx = 0;

    if ((voltage_mV == 0) || (stats == 0))
    {
        return 1;
    }

    /* 先默认第 1 路为初始最大/最小 */
    for (i = 1; i < BQ76940_CELL_COUNT_9; i++)
    {
        if (voltage_mV[i] > voltage_mV[max_idx])
        {
            max_idx = i;
        }

        if (voltage_mV[i] < voltage_mV[min_idx])
        {
            min_idx = i;
        }
    }

    stats->max_mV = voltage_mV[max_idx];
    stats->min_mV = voltage_mV[min_idx];
    stats->diff_mV = (uint16_t)(stats->max_mV - stats->min_mV);

    stats->max_cell_label = cell_label[max_idx];
    stats->min_cell_label = cell_label[min_idx];

    return 0;
}


static const BQ76940_RegBitDesc_t g_bq76940_sys_stat_desc[] =
{
    {"CC_READY",   7},
    {"XREADY",     5},
    {"OVRD_ALERT", 4},
    {"UV",         3},
    {"OV",         2},
    {"SCD",        1},
    {"OCD",        0},
};

static const BQ76940_RegBitDesc_t g_bq76940_sys_ctrl2_desc[] =
{
    {"DELAY_DIS",  7},
    {"CC_EN",      6},
    {"CC_ONESHOT", 5},
    {"DSG_ON",     1},
    {"CHG_ON",     0},
};

uint8_t BQ76940_ClearSysStatBits(uint8_t mask)
{
    /* 注意：SYS_STAT 是“写1清除对应位” */
    return BQ76940_WriteReg_CRC(BQ76940_REG_SYS_STAT, mask);
}



static uint8_t BQ76940_ReadByteReg(uint8_t reg, uint8_t *value)
{
    if (value == 0)
    {
        return 1;
    }

    return BQ76940_ReadReg(reg, value);
}


static void BQ76940_PrintRegBits(const char *title,
                          uint8_t reg_value,
                          const BQ76940_RegBitDesc_t *desc,
                          uint8_t desc_count)
{
    uint8_t i;

    if ((title == 0) || (desc == 0))
    {
        return;
    }

    printf("[%s]\r\n", title);
    printf("%s = 0x%02X\r\n", title, reg_value);

    for (i = 0; i < desc_count; i++)
    {
        printf("%-12s = %d\r\n", desc[i].name, (reg_value >> desc[i].bit) & 0x01);
    }
}

uint8_t BQ76940_ReadSysStat(uint8_t *sys_stat)
{
    return BQ76940_ReadByteReg(BQ76940_REG_SYS_STAT, sys_stat);
}

void BQ76940_PrintSysStat(uint8_t sys_stat)
{
    BQ76940_PrintRegBits("SYS_STAT",
                         sys_stat,
                         g_bq76940_sys_stat_desc,
                         sizeof(g_bq76940_sys_stat_desc) / sizeof(g_bq76940_sys_stat_desc[0]));
}


uint8_t BQ76940_ReadSysCtrl2(uint8_t *sys_ctrl2)
{
    return BQ76940_ReadByteReg(BQ76940_REG_SYS_CTRL2, sys_ctrl2);
}

void BQ76940_PrintSysCtrl2(uint8_t sys_ctrl2)
{
    BQ76940_PrintRegBits("SYS_CTRL2",
                         sys_ctrl2,
                         g_bq76940_sys_ctrl2_desc,
                         sizeof(g_bq76940_sys_ctrl2_desc) / sizeof(g_bq76940_sys_ctrl2_desc[0]));
}

/* =========================
 * 触发一次 CC 1-shot 测量
 * 步骤：
 * 1. 读 SYS_CTRL2
 * 2. 确保 CC_EN = 0
 * 3. 清 CC_READY
 * 4. 置位 CC_ONESHOT
 * ========================= */
uint8_t BQ76940_CC_StartOneShot(void)
{
    uint8_t sys_ctrl2;

    /* 1) 先读当前 SYS_CTRL2，避免把 CHG_ON / DSG_ON 等别的位误改了 */
    if (BQ76940_ReadReg(BQ76940_REG_SYS_CTRL2, &sys_ctrl2) != BQ76940_OK)
    {
        return BQ76940_ERR_COMM;
    }

    /* 2) 关闭连续 CC，保证当前走的是 1-shot 路线 */
    sys_ctrl2 &= (uint8_t)(~BQ76940_SYS_CTRL2_CC_EN);

    /* 顺手先把 CC_ONESHOT 也清掉，保证触发边沿干净 */
    sys_ctrl2 &= (uint8_t)(~BQ76940_SYS_CTRL2_CC_ONESHOT);

    if (BQ76940_WriteReg_CRC(BQ76940_REG_SYS_CTRL2, sys_ctrl2) != BQ76940_OK)
    {
        return BQ76940_ERR_COMM;
    }

    /* 3) 清掉旧的 CC_READY 锁存位：SYS_STAT 写 1 清位 */
		BQ76940_ClearSysStatBits(BQ76940_SYS_STAT_CC_READY);

    /* 4) 再次读回 SYS_CTRL2，基于当前值置位 CC_ONESHOT */
    if (BQ76940_ReadReg(BQ76940_REG_SYS_CTRL2, &sys_ctrl2) != BQ76940_OK)
    {
        return BQ76940_ERR_COMM;
    }

    sys_ctrl2 &= (uint8_t)(~BQ76940_SYS_CTRL2_CC_EN);
    sys_ctrl2 |= BQ76940_SYS_CTRL2_CC_ONESHOT;

    if (BQ76940_WriteReg_CRC(BQ76940_REG_SYS_CTRL2, sys_ctrl2) != BQ76940_OK)
    {
        return BQ76940_ERR_COMM;
    }

    return BQ76940_OK;
}

/* =========================
 * 等待 CC_READY = 1
 * timeout_ms 建议先给 600ms
 * 因为 first measurement 可能最坏接近 500ms
 * ========================= */
uint8_t BQ76940_CC_WaitReady(uint16_t timeout_ms)
{
    uint8_t sys_stat;
    uint16_t elapsed_ms = 0;

    while (elapsed_ms < timeout_ms)
    {
        if (BQ76940_ReadReg(BQ76940_REG_SYS_STAT, & sys_stat) != BQ76940_OK)
        {
            return BQ76940_ERR_COMM;
        }

        if ((sys_stat & BQ76940_SYS_STAT_CC_READY) != 0U)
        {
            return BQ76940_OK;
        }

        delay_ms(10);
        elapsed_ms += 10;
    }

    return BQ76940_ERR_TIMEOUT;
}

/* =========================
 * 读取 CC 原始值
 * 推荐：一次 transaction 连续读 0x32 / 0x33
 * 如果你还没有 BQ76940_ReadRegs()，先用两次单字节读也行，
 * 只是后续最好补成连续读
 * ========================= */
uint8_t BQ76940_CC_ReadRaw(BQ76940_CCRaw_t *cc)
{
    uint8_t buf[2];
	uint16_t ret = 0;

    if (cc == 0)
    {
        return BQ76940_ERR_PARAM;
    }

//    /* 推荐路径：一次 transaction 连续读两个字节 */
//    if (BQ76940_ReadRegs(BQ76940_REG_CC_HI, buf, 2) != BQ76940_OK)
//    {
//        return BQ76940_ERR_COMM;
//    }
		  /* 2. 单字节分别读 */
    ret = BQ76940_ReadReg(BQ76940_REG_CC_HI,&buf[0]);
    if (ret != 0)
    {
        BMS_LOG_ERROR("[CC] HI:%d\r\n", ret);
        return 2;
    }

    ret = BQ76940_ReadReg(BQ76940_REG_CC_LO, &buf[1]);
    if (ret != 0)
    {
        BMS_LOG_ERROR("[CC] LO:%d\r\n", ret);
        return 3;
    }
		

    cc->raw_hi  = buf[0];
    cc->raw_lo  = buf[1];
    cc->raw_u16 = ((uint16_t)buf[0] << 8) | (uint16_t)buf[1];
    cc->raw_s16 = (int16_t)(cc->raw_u16);

    /* 读完以后把 CC_READY 清掉，便于下一轮判断“新结果到了没有” */
    if (BQ76940_WriteReg_CRC(BQ76940_REG_SYS_STAT, BQ76940_SYS_STAT_CC_READY) != BQ76940_OK)
    {
        return BQ76940_ERR_COMM;
    }

    return BQ76940_OK;
}

uint8_t BQ76940_CC_ConvertToCurrent_mA(int16_t cc_raw_s16,
                                       uint32_t rsense_uohm,
                                       int32_t *current_mA)
{
    int32_t temp_mA;

    if ((current_mA == 0) || (rsense_uohm == 0U))
    {
        return BQ76940_ERR_PARAM;
    }

    /* 公式推导：
     * CC_LSB ≈ 8.44 uV/LSB
     * Current = Vsense / Rsense
     *
     * 若 Rsense 用 μΩ，输出想要 mA，则：
     * current_mA = cc_raw_s16 * 8440 / rsense_uohm
     *
     * 解释：
     * 8.44(uV/LSB) × 1000 = 8440
     */
    temp_mA = ((int32_t)cc_raw_s16 * 8440) / (int32_t)rsense_uohm;

    *current_mA = temp_mA;

    return BQ76940_OK;
}

uint8_t BQ76940_ReadTS1Raw(uint16_t *raw_adc)
{
    uint8_t hi = 0;
    uint8_t lo = 0;

    if (raw_adc == 0)
    {
        return BQ76940_ERR_PARAM;
    }

    if (BQ76940_ReadReg(BQ76940_REG_TS1_HI, &hi) != 0)
    {
        return BQ76940_ERR_COMM;
    }

    if (BQ76940_ReadReg(BQ76940_REG_TS1_LO, &lo) != 0)
    {
        return BQ76940_ERR_COMM;
    }

    /* 先按案例工程思路，直接高低字节拼 16 位 */
    *raw_adc = ((uint16_t)hi << 8) | lo;

    return BQ76940_OK;
}

uint8_t BQ76940_ConvertTS1Temp_dC(uint16_t raw_adc, int16_t *temp_dC)
{
    float vts_mV;
    float rt_ntc;
    float temp_C;

    /* 对应案例工程参数 */
    const float Rp = 10000.0f;        /* 分压电阻 10k */
    const float Bx = 3380.0f;         /* NTC B值 */
    const float T2 = 273.15f + 25.0f; /* 25°C 对应开尔文 */
    const float Ka = 273.15f;

    if (temp_dC == 0)
    {
        return BQ76940_ERR_PARAM;
    }

    /* 案例工程思路：
     * VTS = raw_adc * 382uV/LSB
     * 这里转成 mV
     */
    vts_mV = (float)raw_adc * 382.0f / 1000.0f;

    /* 防止分母异常 */
    if ((3300.0f - vts_mV) <= 1.0f)
    {
        return BQ76940_ERR_PARAM;
    }

    /* 按案例工程公式先反推 NTC 电阻 */
    rt_ntc = (10000.0f * vts_mV) / (3300.0f - vts_mV);

    if (rt_ntc <= 0.0f)
    {
        return BQ76940_ERR_PARAM;
    }

    /* B 参数公式 */
    temp_C = 1.0f / (1.0f / T2 + logf(rt_ntc / Rp) / Bx) - Ka;

    /* 转成 0.1°C */
    *temp_dC = (int16_t)(temp_C * 10.0f + (temp_C >= 0 ? 0.5f : -0.5f));

    return BQ76940_OK;
}

uint8_t BQ76940_SetFETState(uint8_t chg_on, uint8_t dsg_on)
{
		uint8_t sys_ctrl2;
	
		if ((chg_on > 1U) || (dsg_on > 1U))
		{
			return 1;
		}
		
		 /* 先读当前 SYS_CTRL2，避免误改其他位 */
		if(BQ76940_ReadReg(BQ76940_REG_SYS_CTRL2,&sys_ctrl2) != BQ76940_OK)
		{
				return BQ76940_ERR_COMM;
		}
		
		//控制CHG
		if(chg_on != 0U)
		{
				sys_ctrl2 |= BQ76940_SYS_CTRL2_CHG_ON;
		}
	  else
		{
				sys_ctrl2 &= (uint8_t)(~BQ76940_SYS_CTRL2_CHG_ON);
		}
		
		
		//控制DSG
		
		if(dsg_on != 0U)
		{
				sys_ctrl2 |= BQ76940_SYS_CTRL2_DSG_ON;
		}
	  else
		{
				sys_ctrl2 &= (uint8_t)(~BQ76940_SYS_CTRL2_DSG_ON);
		}
		
		
		if(BQ76940_WriteReg_CRC(BQ76940_REG_SYS_CTRL2,sys_ctrl2) != BQ76940_OK)
		{
				return BQ76940_ERR_COMM;
		}
		
		return BQ76940_OK;
		
}


uint8_t BQ76940_SetCHGState(uint8_t chg_on)
{
    uint8_t sys_ctrl2;

    if (chg_on > 1U)
    {
        return BQ76940_ERR_PARAM;
    }

    /* 先读当前 SYS_CTRL2，避免误改 DSG / CC 相关位 */
    if (BQ76940_ReadReg(BQ76940_REG_SYS_CTRL2, &sys_ctrl2) != BQ76940_OK)
    {
        return BQ76940_ERR_COMM;
    }

    if (chg_on != 0U)
    {
        sys_ctrl2 |= BQ76940_SYS_CTRL2_CHG_ON;
    }
    else
    {
        sys_ctrl2 &= (uint8_t)(~BQ76940_SYS_CTRL2_CHG_ON);
    }

    if (BQ76940_WriteReg_CRC(BQ76940_REG_SYS_CTRL2, sys_ctrl2) != BQ76940_OK)
    {
        return BQ76940_ERR_COMM;
    }

    return BQ76940_OK;
}


uint8_t BQ76940_SetDSGState(uint8_t dsg_on)
{
    uint8_t sys_ctrl2;

    if (dsg_on > 1U)
    {
        return BQ76940_ERR_PARAM;
    }

    /* 先读当前 SYS_CTRL2，避免误改 CHG / CC 相关位 */
    if (BQ76940_ReadReg(BQ76940_REG_SYS_CTRL2, &sys_ctrl2) != BQ76940_OK)
    {
        return BQ76940_ERR_COMM;
    }

    if (dsg_on != 0U)
    {
        sys_ctrl2 |= BQ76940_SYS_CTRL2_DSG_ON;
    }
    else
    {
        sys_ctrl2 &= (uint8_t)(~BQ76940_SYS_CTRL2_DSG_ON);
    }

    if (BQ76940_WriteReg_CRC(BQ76940_REG_SYS_CTRL2, sys_ctrl2) != BQ76940_OK)
    {
        return BQ76940_ERR_COMM;
    }

    return BQ76940_OK;
}

uint8_t BQ76940_ReadCellBalRegs(BQ76940_CellBalRegs_t *regs)
{
    if (regs == 0)
    {
        return BQ76940_ERR_PARAM;
    }

    if (BQ76940_ReadReg(BQ76940_REG_CELLBAL1, &regs->cellbal1) != BQ76940_OK)
    {
        return BQ76940_ERR_COMM;
    }

    if (BQ76940_ReadReg(BQ76940_REG_CELLBAL2, &regs->cellbal2) != BQ76940_OK)
    {
        return BQ76940_ERR_COMM;
    }

    if (BQ76940_ReadReg(BQ76940_REG_CELLBAL3, &regs->cellbal3) != BQ76940_OK)
    {
        return BQ76940_ERR_COMM;
    }

    return BQ76940_OK;
}

uint8_t BQ76940_WriteCellBalRegs(const BQ76940_CellBalRegs_t *regs)
{
    if (regs == 0)
    {
        return BQ76940_ERR_PARAM;
    }

    if (BQ76940_WriteReg_CRC(BQ76940_REG_CELLBAL1, regs->cellbal1) != BQ76940_OK)
    {
        return BQ76940_ERR_COMM;
    }

    if (BQ76940_WriteReg_CRC(BQ76940_REG_CELLBAL2, regs->cellbal2) != BQ76940_OK)
    {
        return BQ76940_ERR_COMM;
    }

    if (BQ76940_WriteReg_CRC(BQ76940_REG_CELLBAL3, regs->cellbal3) != BQ76940_OK)
    {
        return BQ76940_ERR_COMM;
    }

    return BQ76940_OK;
}

void BQ76940_ClearCellBalRegs(BQ76940_CellBalRegs_t *regs)
{
    if (regs == 0)
    {
        return;
    }

    regs->cellbal1 = 0;
    regs->cellbal2 = 0;
    regs->cellbal3 = 0;
}

uint8_t BQ76940_BuildSingleCellBalMask(uint8_t cell_label,
                                       BQ76940_CellBalRegs_t *regs)
{
    if (regs == 0)
    {
        return BQ76940_ERR_PARAM;
    }

    BQ76940_ClearCellBalRegs(regs);

    if ((cell_label >= 1U) && (cell_label <= 5U))
    {
        regs->cellbal1 = (uint8_t)(1U << (cell_label - 1U));
    }
    else if ((cell_label >= 6U) && (cell_label <= 10U))
    {
        regs->cellbal2 = (uint8_t)(1U << (cell_label - 6U));
    }
    else if ((cell_label >= 11U) && (cell_label <= 15U))
    {
        regs->cellbal3 = (uint8_t)(1U << (cell_label - 11U));
    }
    else
    {
        return BQ76940_ERR_PARAM;
    }

    return BQ76940_OK;
}
