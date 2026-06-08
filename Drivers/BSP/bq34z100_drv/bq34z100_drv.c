#include "bq34z100_reg.h"
#include "bq34z100_drv.h"
#include "soft_i2c1.h"

/*
 * 读取 1 字节命令
 *
 * 例如：
 * StateOfCharge() = 0x02
 * MaxError()      = 0x03
 */
uint8_t BQ34Z100_ReadByte(uint8_t cmd, uint8_t *data)
{
    if (data == 0)
    {
        return BQ34Z100_ERR_PARAM;
    }

    if (SoftI2C1_ReadReg(BQ34Z100_I2C_ADDR, cmd, data) != 0U)
    {
        return BQ34Z100_ERR_COMM;
    }

    return BQ34Z100_OK;
}

/*
 * 读取 2 字节命令
 *
 * BQ34Z100-G1 标准命令中，2 字节数据一般是：
 * buf[0] = low byte
 * buf[1] = high byte
 */
uint8_t BQ34Z100_ReadWord(uint8_t cmd, uint16_t *data)
{
    uint8_t buf[2];

    if (data == 0)
    {
        return BQ34Z100_ERR_PARAM;
    }

    if (SoftI2C1_ReadRegs(BQ34Z100_I2C_ADDR, cmd, buf, 2U) != 0U)
    {
        return BQ34Z100_ERR_COMM;
    }

    *data = ((uint16_t)buf[1] << 8) | buf[0];

    return BQ34Z100_OK;
}

/* SOC，单位：% */
uint8_t BQ34Z100_ReadSOC(uint8_t *soc_percent)
{
    return BQ34Z100_ReadByte(BQ34Z100_CMD_STATE_OF_CHARGE, soc_percent);
}

/* 最大误差，单位：% */
uint8_t BQ34Z100_ReadMaxError(uint8_t *max_error_percent)
{
    return BQ34Z100_ReadByte(BQ34Z100_CMD_MAX_ERROR, max_error_percent);
}

/* 剩余容量，单位：mAh */
uint8_t BQ34Z100_ReadRemainingCapacity_mAh(uint16_t *rm_mAh)
{
    return BQ34Z100_ReadWord(BQ34Z100_CMD_REMAINING_CAPACITY, rm_mAh);
}

/* 满充容量，单位：mAh */
uint8_t BQ34Z100_ReadFullChargeCapacity_mAh(uint16_t *fcc_mAh)
{
    return BQ34Z100_ReadWord(BQ34Z100_CMD_FULL_CHARGE_CAPACITY, fcc_mAh);
}

/* 电池包电压，单位：mV */
uint8_t BQ34Z100_ReadVoltage_mV(uint16_t *voltage_mV)
{
    return BQ34Z100_ReadWord(BQ34Z100_CMD_VOLTAGE, voltage_mV);
}

/*
 * 平均电流，单位：mA
 *
 * 电流是有符号数：
 * 正负方向由芯片定义和系统配置决定。
 */
uint8_t BQ34Z100_ReadAverageCurrent_mA(int16_t *avg_current_mA)
{
    uint16_t raw;
    uint8_t ret;

    if (avg_current_mA == 0)
    {
        return BQ34Z100_ERR_PARAM;
    }

    ret = BQ34Z100_ReadWord(BQ34Z100_CMD_AVERAGE_CURRENT, &raw);
    if (ret != BQ34Z100_OK)
    {
        return ret;
    }

    *avg_current_mA = (int16_t)raw;

    return BQ34Z100_OK;
}

/*
 * 当前电流，单位：mA
 *
 * 和 AverageCurrent 一样，需要按 int16_t 解释。
 */
uint8_t BQ34Z100_ReadCurrent_mA(int16_t *current_mA)
{
    uint16_t raw;
    uint8_t ret;

    if (current_mA == 0)
    {
        return BQ34Z100_ERR_PARAM;
    }

    ret = BQ34Z100_ReadWord(BQ34Z100_CMD_CURRENT, &raw);
    if (ret != BQ34Z100_OK)
    {
        return ret;
    }

    *current_mA = (int16_t)raw;

    return BQ34Z100_OK;
}

/*
 * 温度原始读取
 *
 * 单位：0.1K
 * 例如：
 * 2981 = 298.1K
 */
uint8_t BQ34Z100_ReadTemperature_0p1K(uint16_t *temp_0p1K)
{
    return BQ34Z100_ReadWord(BQ34Z100_CMD_TEMPERATURE, temp_0p1K);
}

/*
 * 温度转换为 0.1℃
 *
 * BQ34Z100 Temperature 单位是 0.1K。
 *
 * 0℃ = 273.15K
 * 0.1K 单位下约等于 2732
 *
 * 例如：
 * temp_0p1K = 2981
 * temp_dC   = 2981 - 2732 = 249
 * 表示 24.9℃
 */
uint8_t BQ34Z100_ReadTemperature_dC(int16_t *temp_dC)
{
    uint16_t temp_0p1K;
    uint8_t ret;

    if (temp_dC == 0)
    {
        return BQ34Z100_ERR_PARAM;
    }

    ret = BQ34Z100_ReadTemperature_0p1K(&temp_0p1K);
    if (ret != BQ34Z100_OK)
    {
        return ret;
    }

    *temp_dC = (int16_t)temp_0p1K - 2732;

    return BQ34Z100_OK;
}

/* Flags 状态位 */
uint8_t BQ34Z100_ReadFlags(uint16_t *flags)
{
    return BQ34Z100_ReadWord(BQ34Z100_CMD_FLAGS, flags);
}

/* FlagsB 扩展状态位 */
uint8_t BQ34Z100_ReadFlagsB(uint16_t *flags_b)
{
    return BQ34Z100_ReadWord(BQ34Z100_CMD_FLAGS_B, flags_b);
}

/* 循环次数 */
uint8_t BQ34Z100_ReadCycleCount(uint16_t *cycle_count)
{
    return BQ34Z100_ReadWord(BQ34Z100_CMD_CYCLE_COUNT, cycle_count);
}

/*
 * SOH 健康状态
 *
 * StateOfHealth() 是 2 字节命令。
 * 常用情况下低字节表示 SOH 百分比。
 * 高字节可作为状态信息后续再单独解析。
 */
uint8_t BQ34Z100_ReadSOH(uint8_t *soh_percent)
{
    uint16_t raw;
    uint8_t ret;

    if (soh_percent == 0)
    {
        return BQ34Z100_ERR_PARAM;
    }

    ret = BQ34Z100_ReadWord(BQ34Z100_CMD_STATE_OF_HEALTH, &raw);
    if (ret != BQ34Z100_OK)
    {
        return ret;
    }

    *soh_percent = (uint8_t)(raw & 0x00FFU);

    return BQ34Z100_OK;
}