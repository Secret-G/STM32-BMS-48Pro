#ifndef __BMS_CONFIG_H
#define __BMS_CONFIG_H

/* =========================
 * Task stack size
 * ========================= */
#define BMS_SAMPLE_TASK_STACK_WORDS 			 512U
#define BMS_PROTECT_TASK_STACK_WORDS       512U
#define BMS_BALANCE_TASK_STACK_WORDS       384U
#define BMS_CONTROL_TASK_STACK_WORDS       384U
#define BMS_CAN_TASK_STACK_WORDS           256U
#define BMS_GAUGE_TASK_STACK_WORDS         256U
#define BMS_AUX_TASK_STACK_WORDS           256U
#define BMS_HW_FAULT_TASK_STACK_WORDS      384U
#define BMS_RUNTIME_TASK_STACK_WORDS  		 512U
#define BMS_ALERT_SIM_TASK_STACK_WORDS     256U
/* =========================
 * Task priority
 *
 * Note:
 * These macros are used in bms_tasks.c.
 * bms_tasks.c already includes FreeRTOS.h/task.h before bms_config.h.
 * ========================= */
#define BMS_HW_FAULT_TASK_PRIORITY         (tskIDLE_PRIORITY + 5U)
#define BMS_RUNTIME_TASK_PRIORITY          (tskIDLE_PRIORITY + 4U)
#define BMS_SAMPLE_TASK_PRIORITY           (tskIDLE_PRIORITY + 4U)
#define BMS_PROTECT_TASK_PRIORITY          (tskIDLE_PRIORITY + 3U)
#define BMS_BALANCE_TASK_PRIORITY          (tskIDLE_PRIORITY + 3U)
#define BMS_CONTROL_TASK_PRIORITY          (tskIDLE_PRIORITY + 3U)
#define BMS_CAN_TASK_PRIORITY              (tskIDLE_PRIORITY + 2U)
#define BMS_GAUGE_TASK_PRIORITY            (tskIDLE_PRIORITY + 1U)
#define BMS_AUX_TASK_PRIORITY              (tskIDLE_PRIORITY + 1U)
#define BMS_ALERT_SIM_TASK_PRIORITY        (tskIDLE_PRIORITY + 1U)

/* =========================
 * Task period
 * ========================= */
#define BMS_SAMPLE_TASK_PERIOD_MS          500U
#define BMS_CAN_TASK_PERIOD_MS             1000U
#define BMS_GAUGE_TASK_PERIOD_MS           1000U
#define BMS_AUX_TASK_PERIOD_MS             1000U

/* =========================
 * Feature switches
 * ========================= */
#define BMS_ENABLE_GAUGE_TASK              0U

/* =========================
 * BQ34Z100 refresh
 * ========================= */
#define BMS_CORE_BQ34Z100_PERIOD_CNT       10U

/* =========================
 * Mutex / timeout
 * ========================= */
#define BMS_I2C_MUTEX_TIMEOUT_MS           100U
#define BMS_HW_FAULT_I2C_TIMEOUT_MS   	   300U
#define BMS_TASK_RET_I2C_LOCK_TIMEOUT      0xF0U

/* =========================
 * Runtime fault test
 * ========================= */
#define BMS_TEST_FORCE_RUNTIME_FAULT       0U
#define BMS_TEST_FORCE_FAIL_START_CYCLE    5U
#define BMS_TEST_FORCE_FAIL_TIMES          3U
#define BMS_TEST_SAFE_OFF_READBACK_ENABLE  0U


/* =========================
 * HwFault ApplyHw retry
 * ========================= */
#define BMS_HW_FAULT_APPLY_MAX_TRIES       3U
#define BMS_HW_FAULT_APPLY_RETRY_DELAY_MS  30U

/* =========================
 * ALERT / HwFault test
 *
 * 1: use fake test path
 * 0: use real hardware path
 * ========================= */
#define BMS_TEST_FAKE_ALERT_EXTI           0U
#define BMS_TEST_FAKE_HW_FAULT             0U


#endif
