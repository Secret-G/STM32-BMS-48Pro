#ifndef __BQ34Z100_DRV_H
#define __BQ34Z100_DRV_H

#include "stdint.h"
#include "bq34z100_reg.h"

#define BQ34Z100_OK                       0U
#define BQ34Z100_ERR_PARAM                1U
#define BQ34Z100_ERR_COMM                 2U

/* 基础读取接口 */
uint8_t BQ34Z100_ReadByte(uint8_t cmd, uint8_t *data);
uint8_t BQ34Z100_ReadWord(uint8_t cmd, uint16_t *data);

/* 标准命令封装 */
uint8_t BQ34Z100_ReadSOC(uint8_t *soc_percent);
uint8_t BQ34Z100_ReadMaxError(uint8_t *max_error_percent);

uint8_t BQ34Z100_ReadRemainingCapacity_mAh(uint16_t *rm_mAh);
uint8_t BQ34Z100_ReadFullChargeCapacity_mAh(uint16_t *fcc_mAh);

uint8_t BQ34Z100_ReadVoltage_mV(uint16_t *voltage_mV);

uint8_t BQ34Z100_ReadAverageCurrent_mA(int16_t *avg_current_mA);
uint8_t BQ34Z100_ReadCurrent_mA(int16_t *current_mA);

uint8_t BQ34Z100_ReadTemperature_0p1K(uint16_t *temp_0p1K);
uint8_t BQ34Z100_ReadTemperature_dC(int16_t *temp_dC);

uint8_t BQ34Z100_ReadFlags(uint16_t *flags);
uint8_t BQ34Z100_ReadFlagsB(uint16_t *flags_b);

uint8_t BQ34Z100_ReadCycleCount(uint16_t *cycle_count);
uint8_t BQ34Z100_ReadSOH(uint8_t *soh_percent);

#endif
