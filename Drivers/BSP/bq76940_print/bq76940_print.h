#ifndef __BQ76940_PRINTF_H__
#define __BQ76940_PRINTF_H__

#include "bq76940_drv.h"
#include "bq76940_alarm.h"


void BQ76940_PrintBasicRegs(const BQ76940_BasicRegs_t *regs);

void BQ76940_PrintAllMappedCellVoltages9(const uint16_t raw_adc[BQ76940_CELL_COUNT_9],
                                                const uint16_t voltage_mV[BQ76940_CELL_COUNT_9],
                                                uint32_t total_mV);

void BQ76940_PrintCellStats9(const BQ76940_CellStats9_t *stats, uint32_t total_mV);

void BQ76940_PrintAlarmFlags9(const BQ76940_AlarmState9_t *state);

void BQ76940_PrintAlarmThresholds9(const BQ76940_AlarmThreshold9_t *th);
																								void BQ76940_PrintPackCurrent(const BQ76940_CCRaw_t *cc,
                              int32_t current_mA,
                              int8_t current_dir);
																								
void BQ76940_PrintCycleSummary9(uint32_t pack_total_mV,
                                const BQ76940_CellStats9_t *stats,
                                const BQ76940_AlarmState9_t *alarm,
                                int32_t current_mA,
                                int8_t current_dir,
                                int16_t ts1_temp_dC,
                                uint8_t ot_cutoff_active,
                                uint8_t ut_chg_block_active,
                                uint8_t hw_dsg_block_active,
                                uint8_t hw_ocd_active,
                                uint8_t hw_scd_active);
																
void BQ76940_PrintTS1Temp(uint16_t raw_adc, int16_t temp_dC);
																
void BQ76940_PrintTempAlarmTs1(const BQ76940_AlarmState9_t *state);
																
void BQ76940_PrintLowTempAlarmTs1(const BQ76940_AlarmState9_t *state);
																
void BQ76940_PrintCellBalRegs(const BQ76940_CellBalRegs_t *regs, const char *tag);
																
void BQ76940_PrintBalanceAutoState(uint8_t bal_active,
                                   uint8_t bal_target_label,
                                   const BQ76940_CellBalRegs_t *regs);
																								
#endif
