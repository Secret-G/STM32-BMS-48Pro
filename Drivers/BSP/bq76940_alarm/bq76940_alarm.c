#include "bq76940_alarm.h"


uint8_t BQ76940_UpdateAlarmState9(const uint16_t voltage_mV[BQ76940_CELL_COUNT_9],
                                  const BQ76940_CellStats9_t *stats,
                                  const BQ76940_AlarmThreshold9_t *th,
                                  BQ76940_AlarmState9_t *state)
{
    uint8_t i;

    /* 当前板子的真实9节映射标签，不是连续1~9 */
    static const uint8_t cell_label[BQ76940_CELL_COUNT_9] = {1, 2, 5, 6, 7, 10, 11, 12, 15};

    /* =========================
     * 1. 参数检查
     * ========================= */
    if ((voltage_mV == 0) || (stats == 0) || (th == 0) || (state == 0))
    {
        return 1;
    }

    /* =========================================================
     * 2. 更新 UV 状态（带回差，方案B）
     *
     * 进入条件：
     *   只要任意一节 < uv_enter_mV，则 UV_FLAG 进入 1
     *
     * 退出条件：
     *   只有当所有电芯都 >= uv_exit_mV，才允许 UV_FLAG 退出到 0
     *
     * 对外显示约定（方案B）：
     *   uv_count          = 当前仍低于 uv_exit_mV、阻碍UV恢复退出的节数
     *   uv_min_cell_label = 当前这些阻碍退出电芯里，最低的是哪一节
     * ========================================================= */
    {
        uint8_t uv_enter_count_now = 0;
        uint8_t uv_block_count_now = 0;

        uint16_t uv_block_min_mV_now = 0xFFFF;
        uint8_t uv_block_min_label_now = 0;

        uint8_t all_cell_above_uv_exit = 1;

        for (i = 0; i < BQ76940_CELL_COUNT_9; i++)
        {
            /* 2.1 统计“进入条件”：低于欠压进入阈值 */
            if (voltage_mV[i] < th->uv_enter_mV)
            {
                uv_enter_count_now++;
            }

            /* 2.2 统计“阻碍退出”的条件：低于欠压恢复阈值
             * 这里采用“>= uv_exit_mV 即可恢复”的定义，
             * 所以只要 < uv_exit_mV，就还不能退出。
             */
            if (voltage_mV[i] < th->uv_exit_mV)
            {
                uv_block_count_now++;
                all_cell_above_uv_exit = 0;

                if (voltage_mV[i] < uv_block_min_mV_now)
                {
                    uv_block_min_mV_now = voltage_mV[i];
                    uv_block_min_label_now = cell_label[i];
                }
            }
        }

        /* 2.3 更新 UV 状态机 */
        if (state->uv_flag == 0)
        {
            /* 当前不在 UV 状态：只看是否满足进入条件 */
            if (uv_enter_count_now > 0)
            {
                state->uv_flag = 1;
            }
            else
            {
                state->uv_flag = 0;
            }
        }
        else
        {
            /* 当前已经在 UV 状态：只看是否满足退出条件 */
            if (all_cell_above_uv_exit == 1)
            {
                state->uv_flag = 0;
            }
            else
            {
                state->uv_flag = 1;
            }
        }

        /* 2.4 按方案B更新对外显示字段 */
        if (state->uv_flag == 1)
        {
            state->uv_count = uv_block_count_now;
            state->uv_min_cell_label = uv_block_min_label_now;
        }
        else
        {
            state->uv_count = 0;
            state->uv_min_cell_label = 0;
        }
    }

    /* =========================================================
     * 3. 更新 OV 状态（带回差，方案B）
     *
     * 进入条件：
     *   只要任意一节 > ov_enter_mV，则 OV_FLAG 进入 1
     *
     * 退出条件：
     *   只有当所有电芯都 <= ov_exit_mV，才允许 OV_FLAG 退出到 0
     *
     * 对外显示约定（方案B）：
     *   ov_count          = 当前仍高于 ov_exit_mV、阻碍OV恢复退出的节数
     *   ov_max_cell_label = 当前这些阻碍退出电芯里，最高的是哪一节
     * ========================================================= */
    {
        uint8_t ov_enter_count_now = 0;
        uint8_t ov_block_count_now = 0;

        uint16_t ov_block_max_mV_now = 0;
        uint8_t ov_block_max_label_now = 0;

        uint8_t all_cell_below_ov_exit = 1;

        for (i = 0; i < BQ76940_CELL_COUNT_9; i++)
        {
            /* 3.1 统计“进入条件”：高于过压进入阈值 */
            if (voltage_mV[i] > th->ov_enter_mV)
            {
                ov_enter_count_now++;
            }

            /* 3.2 统计“阻碍退出”的条件：高于过压恢复阈值
             * 这里采用“<= ov_exit_mV 即可恢复”的定义，
             * 所以只要 > ov_exit_mV，就还不能退出。
             */
            if (voltage_mV[i] > th->ov_exit_mV)
            {
                ov_block_count_now++;
                all_cell_below_ov_exit = 0;

                if (voltage_mV[i] > ov_block_max_mV_now)
                {
                    ov_block_max_mV_now = voltage_mV[i];
                    ov_block_max_label_now = cell_label[i];
                }
            }
        }

        /* 3.3 更新 OV 状态机 */
        if (state->ov_flag == 0)
        {
            /* 当前不在 OV 状态：只看是否满足进入条件 */
            if (ov_enter_count_now > 0)
            {
                state->ov_flag = 1;
            }
            else
            {
                state->ov_flag = 0;
            }
        }
        else
        {
            /* 当前已经在 OV 状态：只看是否满足退出条件 */
            if (all_cell_below_ov_exit == 1)
            {
                state->ov_flag = 0;
            }
            else
            {
                state->ov_flag = 1;
            }
        }

        /* 3.4 按方案B更新对外显示字段 */
        if (state->ov_flag == 1)
        {
            state->ov_count = ov_block_count_now;
            state->ov_max_cell_label = ov_block_max_label_now;
        }
        else
        {
            state->ov_count = 0;
            state->ov_max_cell_label = 0;
        }
    }

    /* =========================================================
     * 4. 更新 DIFF 状态（带回差）
     *
     * 进入条件：
     *   diff_mV > diff_enter_mV
     *
     * 退出条件：
     *   diff_mV < diff_exit_mV
     * ========================================================= */
    if (state->diff_flag == 0)
    {
        if (stats->diff_mV > th->diff_enter_mV)
        {
            state->diff_flag = 1;
        }
        else
        {
            state->diff_flag = 0;
        }
    }
    else
    {
        if (stats->diff_mV < th->diff_exit_mV)
        {
            state->diff_flag = 0;
        }
        else
        {
            state->diff_flag = 1;
        }
    }

    return 0;
}


uint8_t BQ76940_UpdateTempAlarmTs1(int16_t ts1_temp_dC,
                                   const BQ76940_AlarmThreshold9_t *th,
                                   BQ76940_AlarmState9_t *state)
{
    if ((th == 0) || (state == 0))
    {
        return 1;
    }

    /* 记录当前温度，便于打印 */
    state->ot_temp_dC = ts1_temp_dC;

    /* 当前没有过温告警：看是否达到进入阈值 */
    if (state->ot_flag == 0)
    {
        if (ts1_temp_dC >= th->ot_enter_dC)
        {
            state->ot_flag = 1;
        }
    }
    else
    {
        /* 当前已经过温告警：看是否降到退出阈值 */
        if (ts1_temp_dC <= th->ot_exit_dC)
        {
            state->ot_flag = 0;
        }
    }

    return 0;
}

uint8_t BQ76940_UpdateLowTempAlarmTs1(int16_t ts1_temp_dC,
                                      const BQ76940_AlarmThreshold9_t *th,
                                      BQ76940_AlarmState9_t *state)
{
    if ((th == 0) || (state == 0))
    {
        return 1;
    }

    /* 记录当前温度，便于打印 */
    state->ut_temp_dC = ts1_temp_dC;

    /* 当前没有低温告警：看是否达到进入阈值 */
    if (state->ut_flag == 0U)
    {
        if (ts1_temp_dC <= th->ut_enter_dC)
        {
            state->ut_flag = 1U;
        }
    }
    else
    {
        /* 当前已经低温告警：看是否回升到退出阈值 */
        if (ts1_temp_dC >= th->ut_exit_dC)
        {
            state->ut_flag = 0U;
        }
    }

    return 0;
}
