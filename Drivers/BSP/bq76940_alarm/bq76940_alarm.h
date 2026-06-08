#ifndef __BQ76940_ALARM_H
#define __BQ76940_ALARM_H

#include "bq76940_drv.h"

typedef struct
{
    uint8_t uv_flag;            /* 当前是否处于欠压状态 */
    uint8_t ov_flag;            /* 当前是否处于过压状态 */
    uint8_t diff_flag;          /* 当前是否处于压差过大状态 */

    uint8_t uv_min_cell_label;  /* 方案B：当前仍低于 uv_exit_mV、阻碍UV恢复退出的最低电芯 */
    uint8_t ov_max_cell_label;  /* 方案B：当前仍高于 ov_exit_mV、阻碍OV恢复退出的最高电芯 */

    uint8_t uv_count;           /* 方案B：当前仍低于 uv_exit_mV、阻碍UV恢复退出的节数 */
    uint8_t ov_count;           /* 方案B：当前仍高于 ov_exit_mV、阻碍OV恢复退出的节数 */
	
	  /* 新增：过温状态 */
    uint8_t ot_flag;
    int16_t ot_temp_dC;
	
    /* 新增：低温状态 */
    uint8_t ut_flag;
    int16_t ut_temp_dC;
	
} BQ76940_AlarmState9_t;


typedef struct
{
    uint16_t uv_enter_mV;     /* 欠压进入阈值，例如 3000 */
    uint16_t uv_exit_mV;      /* 欠压恢复阈值，例如 3100 */

    uint16_t ov_enter_mV;     /* 过压进入阈值，例如 4200 */
    uint16_t ov_exit_mV;      /* 过压恢复阈值，例如 4100 */

    uint16_t diff_enter_mV;   /* 压差进入阈值，例如 200 */
    uint16_t diff_exit_mV;    /* 压差恢复阈值，例如 150 */
	
	
	    /* 新增：软件过温阈值，单位 0.1°C */
    int16_t  ot_enter_dC;
    int16_t  ot_exit_dC;
	
			    /* 新增：低温软件告警阈值，单位 0.1°C */
    int16_t  ut_enter_dC;
    int16_t  ut_exit_dC;
	
} BQ76940_AlarmThreshold9_t;



uint8_t BQ76940_UpdateAlarmState9(const uint16_t voltage_mV[BQ76940_CELL_COUNT_9],
                                  const BQ76940_CellStats9_t *stats,
                                  const BQ76940_AlarmThreshold9_t *th,
                                  BQ76940_AlarmState9_t *state);
																	
																	
/* 新增：TS1 过温软件告警 */
uint8_t BQ76940_UpdateTempAlarmTs1(int16_t ts1_temp_dC,
                                   const BQ76940_AlarmThreshold9_t *th,
                                   BQ76940_AlarmState9_t *state);
																	 
/* 新增：TS1 低温软件告警 */
uint8_t BQ76940_UpdateLowTempAlarmTs1(int16_t ts1_temp_dC,
                                      const BQ76940_AlarmThreshold9_t *th,
                                      BQ76940_AlarmState9_t *state);

#endif
