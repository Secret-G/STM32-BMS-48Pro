#include "bq76940_print.h"


 void BQ76940_PrintBasicRegs(const BQ76940_BasicRegs_t *regs)
{
    printf("\r\n[BQ76940 Basic Registers]\r\n");
    printf("SYS_STAT  = 0x%02X\r\n", regs->sys_stat);
    printf("SYS_CTRL1 = 0x%02X\r\n", regs->sys_ctrl1);
    printf("SYS_CTRL2 = 0x%02X\r\n", regs->sys_ctrl2);
    printf("PROTECT1  = 0x%02X\r\n", regs->protect1);
    printf("PROTECT2  = 0x%02X\r\n", regs->protect2);
    printf("PROTECT3  = 0x%02X\r\n", regs->protect3);
		printf("OV_TRIP  = 0x%02X\r\n", regs->ov_trip);
    printf("UV_TRIP  = 0x%02X\r\n", regs->uv_trip);
    printf("CC_CFG    = 0x%02X\r\n", regs->cc_cfg);
}

 void BQ76940_PrintAllMappedCellVoltages9(const uint16_t raw_adc[BQ76940_CELL_COUNT_9],
                                                const uint16_t voltage_mV[BQ76940_CELL_COUNT_9],
                                                uint32_t total_mV)
{
    static const uint8_t cell_label[BQ76940_CELL_COUNT_9] = {1, 2, 5, 6, 7, 10, 11, 12, 15};
    uint8_t i;

    printf("\r\n[Mapped Cell Voltages]\r\n");
    for (i = 0; i < BQ76940_CELL_COUNT_9; i++)
    {
        printf("VC%d_RAW = 0x%04X, VC%d_mV = %d\r\n",
               cell_label[i], raw_adc[i],
               cell_label[i], voltage_mV[i]);
    }

    printf("PACK_TOTAL_mV = %lu\r\n", (unsigned long)total_mV);
}

 void BQ76940_PrintCellStats9(const BQ76940_CellStats9_t *stats, uint32_t total_mV)
{
    printf("\r\n[Cell Statistics]\r\n");
    printf("VC_MAX_mV        = %d\r\n", stats->max_mV);
    printf("VC_MAX_CHANNEL   = VC%d\r\n", stats->max_cell_label);

    printf("VC_MIN_mV        = %d\r\n", stats->min_mV);
    printf("VC_MIN_CHANNEL   = VC%d\r\n", stats->min_cell_label);

    printf("VC_DIFF_mV       = %d\r\n", stats->diff_mV);
    printf("PACK_TOTAL_mV    = %lu\r\n", (unsigned long)total_mV);
}

void BQ76940_PrintAlarmFlags9(const BQ76940_AlarmState9_t *state)
{
    if (state == 0)
    {
        return;
    }

    printf("[Alarm Flags]\r\n");

    printf("UV_FLAG        = %d\r\n", state->uv_flag);
    if (state->uv_min_cell_label != 0)
    {
        printf("UV_MIN_CHANNEL = VC%d\r\n", state->uv_min_cell_label);
    }
    else
    {
        printf("UV_MIN_CHANNEL = NONE\r\n");
    }
    printf("UV_COUNT       = %d\r\n", state->uv_count);

    printf("OV_FLAG        = %d\r\n", state->ov_flag);
    if (state->ov_max_cell_label != 0)
    {
        printf("OV_MAX_CHANNEL = VC%d\r\n", state->ov_max_cell_label);
    }
    else
    {
        printf("OV_MAX_CHANNEL = NONE\r\n");
    }
    printf("OV_COUNT       = %d\r\n", state->ov_count);

    printf("DIFF_FLAG      = %d\r\n", state->diff_flag);
}

void BQ76940_PrintAlarmThresholds9(const BQ76940_AlarmThreshold9_t *th)
{
	printf("\r\n[Cell Statistics]\r\n");
    printf("uv_enter_mV  = %d\r\n", th->uv_enter_mV);
    printf("uv_exit_mV   = %d\r\n", th->uv_exit_mV);

    printf("ov_enter_mV  = %d\r\n", th->ov_enter_mV);
    printf("ov_exit_mV   = %d\r\n", th->ov_exit_mV);

    printf("diff_enter_mV   = %d\r\n", th->diff_enter_mV);
    printf("diff_exit_mV    = %d\r\n", th->diff_exit_mV);
}


void BQ76940_PrintPackCurrent(const BQ76940_CCRaw_t *cc,
                              int32_t current_mA,
                              int8_t current_dir)
{
    int32_t abs_current_mA = 0;

    if (cc == 0)
    {
        return;
    }

    if (current_mA >= 0)
    {
        abs_current_mA = current_mA;
    }
    else
    {
        abs_current_mA = -current_mA;
    }

    printf("[CC RAW]\r\n");
    printf("CC_HI  = 0x%02X\r\n", cc->raw_hi);
    printf("CC_LO  = 0x%02X\r\n", cc->raw_lo);
    printf("CC_U16 = 0x%04X\r\n", cc->raw_u16);
    printf("CC_S16 = %d\r\n", cc->raw_s16);

    printf("[PACK CURRENT]\r\n");
    printf("CURRENT_mA = %ld\r\n", (long)current_mA);

    if (current_mA < 0)
    {
        printf("CURRENT_A  = -%ld.%03ld\r\n",
               (long)(abs_current_mA / 1000),
               (long)(abs_current_mA % 1000));
    }
    else
    {
        printf("CURRENT_A  = %ld.%03ld\r\n",
               (long)(abs_current_mA / 1000),
               (long)(abs_current_mA % 1000));
    }

    if (current_dir > 0)
    {
        printf("CC_DIR = CHARGE(+)\r\n");
    }
    else if (current_dir < 0)
    {
        printf("CC_DIR = DISCHARGE(-)\r\n");
    }
    else
    {
        printf("CC_DIR = NEAR_ZERO\r\n");
    }
}

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
                                uint8_t hw_scd_active)
{
    int32_t abs_current_mA;
    int16_t abs_temp_dC;

    if ((stats == 0) || (alarm == 0))
    {
        return;
    }

    if (current_mA >= 0)
    {
        abs_current_mA = current_mA;
    }
    else
    {
        abs_current_mA = -current_mA;
    }

    printf("[CYCLE SUMMARY]\r\n");

    printf("PACK = %lu mV\r\n", (unsigned long)pack_total_mV);

    printf("VMAX = VC%u:%u mV\r\n",
           stats->max_cell_label,
           stats->max_mV);

    printf("VMIN = VC%u:%u mV\r\n",
           stats->min_cell_label,
           stats->min_mV);

    printf("DIFF = %u mV\r\n", stats->diff_mV);

    printf("I    = ");
    if (current_mA < 0)
    {
        printf("-%ld.%03ld A",
               (long)(abs_current_mA / 1000),
               (long)(abs_current_mA % 1000));
    }
    else
    {
        printf("%ld.%03ld A",
               (long)(abs_current_mA / 1000),
               (long)(abs_current_mA % 1000));
    }

    if (current_dir > 0)
    {
        printf("  CHARGE\r\n");
    }
    else if (current_dir < 0)
    {
        printf("  DISCHARGE\r\n");
    }
    else
    {
        printf("  NEAR_ZERO\r\n");
    }

    printf("T    = ");
    if (ts1_temp_dC < 0)
    {
        abs_temp_dC = (int16_t)(-ts1_temp_dC);
        printf("-%d.%d C\r\n",
               abs_temp_dC / 10,
               abs_temp_dC % 10);
    }
    else
    {
        printf("%d.%d C\r\n",
               ts1_temp_dC / 10,
               ts1_temp_dC % 10);
    }

		printf("ALM  = UV:%u OV:%u DIFF:%u OT:%u UT:%u\r\n",
       alarm->uv_flag,
       alarm->ov_flag,
       alarm->diff_flag,
       alarm->ot_flag,
       alarm->ut_flag);

		printf("PROT = OT_CUTOFF:%u UT_CHG_BLOCK:%u DSG_BLOCK:%u OCD:%u SCD:%u\r\n",
					 ot_cutoff_active,
					 ut_chg_block_active,
					 hw_dsg_block_active,
					 hw_ocd_active,
					 hw_scd_active);
}

void BQ76940_PrintTS1Temp(uint16_t raw_adc, int16_t temp_dC)
{
    int16_t abs_temp;

    if (temp_dC < 0)
    {
        abs_temp = (int16_t)(-temp_dC);

        printf("[TS1 TEMP]\r\n");
        printf("RAW_ADC = %u\r\n", raw_adc);
        printf("TEMP    = -%d.%d C\r\n",
               abs_temp / 10,
               abs_temp % 10);
    }
    else
    {
        printf("[TS1 TEMP]\r\n");
        printf("RAW_ADC = %u\r\n", raw_adc);
        printf("TEMP    = %d.%d C\r\n",
               temp_dC / 10,
               temp_dC % 10);
    }
}


void BQ76940_PrintTempAlarmTs1(const BQ76940_AlarmState9_t *state)
{
    int16_t abs_temp;

    if (state == 0)
    {
        return;
    }

    printf("[TEMP ALARM]\r\n");

    if (state->ot_temp_dC < 0)
    {
        abs_temp = (int16_t)(-state->ot_temp_dC);
        printf("TS1_TEMP = -%d.%d C\r\n",
               abs_temp / 10,
               abs_temp % 10);
    }
    else
    {
        printf("TS1_TEMP = %d.%d C\r\n",
               state->ot_temp_dC / 10,
               state->ot_temp_dC % 10);
    }

    printf("OT_FLAG  = %d\r\n", state->ot_flag);
}

void BQ76940_PrintLowTempAlarmTs1(const BQ76940_AlarmState9_t *state)
{
    int16_t abs_temp;

    if (state == 0)
    {
        return;
    }

    printf("[LOW TEMP ALARM]\r\n");

    if (state->ut_temp_dC < 0)
    {
        abs_temp = (int16_t)(-state->ut_temp_dC);
        printf("TS1_TEMP = -%d.%d C\r\n",
               abs_temp / 10,
               abs_temp % 10);
    }
    else
    {
        printf("TS1_TEMP = %d.%d C\r\n",
               state->ut_temp_dC / 10,
               state->ut_temp_dC % 10);
    }

    printf("UT_FLAG  = %d\r\n", state->ut_flag);
}



void BQ76940_PrintCellBalRegs(const BQ76940_CellBalRegs_t *regs, const char *tag)
{
    if ((regs == 0) || (tag == 0))
    {
        return;
    }

    printf("[%s]\r\n", tag);
    printf("CELLBAL1 = 0x%02X\r\n", regs->cellbal1);
    printf("CELLBAL2 = 0x%02X\r\n", regs->cellbal2);
    printf("CELLBAL3 = 0x%02X\r\n", regs->cellbal3);
}

void BQ76940_PrintBalanceAutoState(uint8_t bal_active,
                                   uint8_t bal_target_label,
                                   const BQ76940_CellBalRegs_t *regs)
{
    if (regs == 0)
    {
        return;
    }

    printf("[BALANCE AUTO]\r\n");
    printf("BAL_ACTIVE       = %d\r\n", bal_active);
    printf("BAL_TARGET_LABEL = %d\r\n", bal_target_label);
    printf("CELLBAL1         = 0x%02X\r\n", regs->cellbal1);
    printf("CELLBAL2         = 0x%02X\r\n", regs->cellbal2);
    printf("CELLBAL3         = 0x%02X\r\n", regs->cellbal3);
}
