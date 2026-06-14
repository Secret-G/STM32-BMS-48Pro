#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "bms_tasks.h"

#include "stdio.h"
#include "bms_log.h"
#include "led.h"
#include "bq34z100_app.h"
#include "bq76200_exec_port.h"
#include "bq76940_alert_sim.h"

#define BMS_SAMPLE_TASK_STACK_WORDS 512U
#define BMS_PROTECT_TASK_STACK_WORDS 512U
#define BMS_BALANCE_TASK_STACK_WORDS 384U
#define BMS_CONTROL_TASK_STACK_WORDS 384U
#define BMS_CAN_TASK_STACK_WORDS 256U
#define BMS_GAUGE_TASK_STACK_WORDS 256U
#define BMS_AUX_TASK_STACK_WORDS 256U
#define BMS_RUNTIME_TASK_STACK_WORDS 512U

#define BMS_HW_FAULT_TASK_PRIORITY     (tskIDLE_PRIORITY + 5U)
#define BMS_RUNTIME_TASK_PRIORITY (tskIDLE_PRIORITY + 4U)
#define BMS_SAMPLE_TASK_PRIORITY (tskIDLE_PRIORITY + 4U)
#define BMS_PROTECT_TASK_PRIORITY (tskIDLE_PRIORITY + 3U)
#define BMS_BALANCE_TASK_PRIORITY (tskIDLE_PRIORITY + 3U)
#define BMS_CONTROL_TASK_PRIORITY (tskIDLE_PRIORITY + 3U)
#define BMS_CAN_TASK_PRIORITY (tskIDLE_PRIORITY + 2U)
#define BMS_GAUGE_TASK_PRIORITY (tskIDLE_PRIORITY + 1U)
#define BMS_AUX_TASK_PRIORITY (tskIDLE_PRIORITY + 1U)

#define BMS_SAMPLE_TASK_PERIOD_MS 500U
#define BMS_CAN_TASK_PERIOD_MS 1000U
#define BMS_GAUGE_TASK_PERIOD_MS 1000U
#define BMS_AUX_TASK_PERIOD_MS 1000U

#define BMS_ENABLE_GAUGE_TASK 0U

#if (BMS_ENABLE_GAUGE_TASK != 0U)
static BQ34Z100_AppCtx_t g_bq34z100_ctx;
#endif

/*
 * BQ34Z100 刷新周期计数
 * 当前 BMS_SampleTask 周期约 100ms，
 * 10 次约等于 1s。
 */
#define BMS_CORE_BQ34Z100_PERIOD_CNT 10U

/*
 * I2C 总线互斥锁超时时间。
 * 作用：
 *   防止某个任务长期占用 I2C 总线后，其他任务永久阻塞。
 */
#define BMS_I2C_MUTEX_TIMEOUT_MS 100U

/*
 * FreeRTOS 任务层内部错误码：
 * 表示当前任务在规定时间内没有拿到 I2C 总线锁。
 */
#define BMS_TASK_RET_I2C_LOCK_TIMEOUT 0xF0U

/*
 * AFE 硬件写禁止标志。
 *
 * 一旦进入 runtime fault：
 * - 禁止 ProtectTask 操作 CHG/DSG
 * - 禁止 BalanceTask操作 CELLBAL
 * - 只有 RuntimeTask 可以执行 Safe-Off
 */
static volatile uint8_t g_afe_write_inhibit = 0U;

/*
 * BMS 上下文互斥锁：
 *   保护 BQ76940_AppCtx_t 这个全局状态结构体。
 */
static SemaphoreHandle_t g_bms_ctx_mutex = NULL;

/*-----------------运行异常测试宏--------------*/
#define BMS_TEST_FORCE_RUNTIME_FAULT 0U
#define BMS_TEST_FORCE_FAIL_START_CYCLE 5U
#define BMS_TEST_FORCE_FAIL_TIMES 3U
#define BMS_TEST_SAFE_OFF_READBACK_ENABLE 0U

#define BMS_TEST_FAKE_HW_FAULT_SYS_STAT   (BQ76940_SYS_STAT_OCD | BQ76940_SYS_STAT_SCD)

/*-------------------------------------------*/
/*
 * I2C 总线互斥锁：
 *   保护 SoftI2C1 总线，防止多个任务同时访问 BQ76940 / BQ34Z100。
 */
static SemaphoreHandle_t g_i2c_bus_mutex = NULL;

static SemaphoreHandle_t g_protect_sem = NULL;
static SemaphoreHandle_t g_balance_sem = NULL;
static SemaphoreHandle_t g_control_sem = NULL;
static SemaphoreHandle_t g_runtime_sem = NULL;
static SemaphoreHandle_t g_hw_fault_sem = NULL;

static void BMS_SampleTask(void *argument);
static void BMS_ProtectTask(void *argument);
static void BMS_BalanceTask(void *argument);
static void BMS_ControlTask(void *argument);
static void BMS_CANTask(void *argument);
static void BMS_RuntimeTask(void *argument);
static void BMS_HwFaultTask(void *argument);

#if (BMS_TEST_FAKE_ALERT_EXTI != 0U)
static void BMS_AlertSimTestTask(void *argument);
#endif

static void BMS_AfeWriteInhibitSet(void);
static uint8_t BMS_AfeWriteIsInhibited(void);
static uint8_t BMS_HwFaultReadSysStat(uint8_t *sys_stat);

#if (BMS_TEST_SAFE_OFF_READBACK_ENABLE != 0U)
static void BMS_RuntimeSafeOffReadback(BQ76940_AppCtx_t *app);
#endif

#if (BMS_ENABLE_GAUGE_TASK != 0U)
static void BMS_GaugeTask(void *argument);
#endif

static void BMS_AuxTask(void *argument);

void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name)
{
    (void)task;

    BMS_LOG_ERROR("[RTOS] stack:%s\r\n",
           (task_name != NULL) ? task_name : "unknown");

    taskDISABLE_INTERRUPTS();
    while (1)
    {
    }
}

void vApplicationMallocFailedHook(void)
{
    BMS_LOG_ERROR("[RTOS] malloc fail\r\n");

    taskDISABLE_INTERRUPTS();
    while (1)
    {
    }
}

BaseType_t BMS_TasksCreate(BQ76940_AppCtx_t *app)
{
    BaseType_t result;

    if (app == NULL)
    {
        return pdFAIL;
    }

    /*
     * 创建 BMS 上下文互斥锁。
     * 作用：
     *   保护 BQ76940_AppCtx_t 全局状态，防止多个任务同时读写 app。
     */
    g_bms_ctx_mutex = xSemaphoreCreateMutex();
    if (g_bms_ctx_mutex == NULL)
    {
        BMS_LOG_ERROR("[RTOS] ctx mutex fail\r\n");
        return pdFAIL;
    }

    /*
     * 创建 I2C 总线互斥锁。
     * 作用：
     *   保护 SoftI2C1 总线，保证一次 I2C 读写过程不会被其他任务插入。
     *
     * 注意：
     *   g_bms_ctx_mutex 保护的是“数据结构”；
     *   g_i2c_bus_mutex 保护的是“I2C 总线”。
     */
    g_i2c_bus_mutex = xSemaphoreCreateMutex();
    if (g_i2c_bus_mutex == NULL)
    {
        BMS_LOG_ERROR("[RTOS] i2c mutex fail\r\n");
        return pdFAIL;
    }

    /*
     * 创建任务链路接力信号量：
     *
     * SampleTask  -> g_protect_sem -> ProtectTask
     * ProtectTask -> g_balance_sem -> BalanceTask
     * BalanceTask -> g_control_sem -> ControlTask
     */
    g_protect_sem = xSemaphoreCreateBinary();
    g_balance_sem = xSemaphoreCreateBinary();
    g_control_sem = xSemaphoreCreateBinary();
    g_runtime_sem = xSemaphoreCreateBinary();
    g_hw_fault_sem = xSemaphoreCreateBinary();

    if ((g_protect_sem == NULL) ||
        (g_balance_sem == NULL) ||
        (g_control_sem == NULL) ||
        (g_runtime_sem == NULL) ||
        (g_hw_fault_sem == NULL))
    {
        BMS_LOG_ERROR("[RTOS] sem fail\r\n");
        return pdFAIL;
    }

    result = xTaskCreate(BMS_SampleTask,
                         "BMS_Sample",
                         BMS_SAMPLE_TASK_STACK_WORDS,
                         app,
                         BMS_SAMPLE_TASK_PRIORITY,
                         NULL);
    if (result != pdPASS)
    {
        BMS_LOG_ERROR("[RTOS] sample task fail\r\n");
        return result;
    }
    result = xTaskCreate(BMS_HwFaultTask,
                         "BMS_HwFault",
                         384U,
                         app,
                         BMS_HW_FAULT_TASK_PRIORITY,
                         NULL);
    if (result != pdPASS)
    {
        BMS_LOG_ERROR("[RTOS] hw task fail\r\n");
        return result;
    }

    result = xTaskCreate(BMS_ProtectTask,
                         "BMS_Protect",
                         BMS_PROTECT_TASK_STACK_WORDS,
                         app,
                         BMS_PROTECT_TASK_PRIORITY,
                         NULL);
    if (result != pdPASS)
    {
        BMS_LOG_ERROR("[RTOS] protect task fail\r\n");
        return result;
    }

    result = xTaskCreate(BMS_BalanceTask,
                         "BMS_Balance",
                         BMS_BALANCE_TASK_STACK_WORDS,
                         app,
                         BMS_BALANCE_TASK_PRIORITY,
                         NULL);
    if (result != pdPASS)
    {
        BMS_LOG_ERROR("[RTOS] balance task fail\r\n");
        return result;
    }

    result = xTaskCreate(BMS_ControlTask,
                         "BMS_Control",
                         BMS_CONTROL_TASK_STACK_WORDS,
                         app,
                         BMS_CONTROL_TASK_PRIORITY,
                         NULL);
    if (result != pdPASS)
    {
        BMS_LOG_ERROR("[RTOS] control task fail\r\n");
        return result;
    }

    result = xTaskCreate(BMS_RuntimeTask,
                         "BMS_Runtime",
                         BMS_RUNTIME_TASK_STACK_WORDS,
                         app,
                         BMS_RUNTIME_TASK_PRIORITY,
                         NULL);
    if (result != pdPASS)
    {
        BMS_LOG_ERROR("[RTOS] runtime task fail\r\n");
        return result;
    }

    result = xTaskCreate(BMS_CANTask,
                         "BMS_CAN",
                         BMS_CAN_TASK_STACK_WORDS,
                         app,
                         BMS_CAN_TASK_PRIORITY,
                         NULL);
    if (result != pdPASS)
    {
        BMS_LOG_ERROR("[RTOS] CAN task fail\r\n");
        return result;
    }

#if (BMS_ENABLE_GAUGE_TASK != 0U)
    BQ34Z100_AppInit(&g_bq34z100_ctx);

    result = xTaskCreate(BMS_GaugeTask,
                         "BMS_Gauge",
                         BMS_GAUGE_TASK_STACK_WORDS,
                         NULL,
                         BMS_GAUGE_TASK_PRIORITY,
                         NULL);
    if (result != pdPASS)
    {
        BMS_LOG_ERROR("[RTOS] gauge task fail\r\n");
        return result;
    }
#endif

    result = xTaskCreate(BMS_AuxTask,
                         "BMS_Aux",
                         BMS_AUX_TASK_STACK_WORDS,
                         app,
                         BMS_AUX_TASK_PRIORITY,
                         NULL);
    if (result != pdPASS)
    {
        BMS_LOG_ERROR("[RTOS] aux task fail\r\n");
        return result;
    }

		
#if (BMS_TEST_FAKE_ALERT_EXTI != 0U)
    result = xTaskCreate(BMS_AlertSimTestTask,
                         "AlertSimTest",
                         256U,
                         NULL,
                         tskIDLE_PRIORITY + 1U,
                         NULL);
    if (result != pdPASS)
    {
        BMS_LOG_ERROR("[RTOS] alert test task fail\r\n");
        return result;
    }
#endif

    return pdPASS;
}

static void BMS_SampleTask(void *argument)
{
    BQ76940_AppCtx_t *app = (BQ76940_AppCtx_t *)argument;

    uint8_t ret;
    uint8_t fault_code;
    uint8_t fault_stage;
    uint8_t enter_fault;
    uint8_t recovered;
	  uint8_t notify_protect = 0U;

#if (BMS_TEST_FORCE_RUNTIME_FAULT != 0U)
    uint8_t test_cycle_count = 0U;
    uint8_t test_fail_left = 0U;
#endif

    BQ76940_AdcCalib_t calib_snapshot;
    BQ76940_AppSampleData_t sample;

    for (;;)
    {
        ret = 0U;
        fault_code = BQ76940_RT_FAULT_NONE;
        fault_stage = BQ76940_RT_STAGE_NONE;
        enter_fault = 0U;
        recovered = 0U;

#if (BMS_TEST_FORCE_RUNTIME_FAULT != 0U)
        if (test_cycle_count < 255U)
        {
            test_cycle_count++;
        }

        if (test_cycle_count == BMS_TEST_FORCE_FAIL_START_CYCLE)
        {
            test_fail_left = BMS_TEST_FORCE_FAIL_TIMES;
            BMS_LOG_TEST_HW_FAULT("[TEST] runtime inject\r\n");
        }
#endif

        /*
         * 1. 复制 ADC 校准参数快照
         */
        if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
        {
            calib_snapshot = app->calib;
            xSemaphoreGive(g_bms_ctx_mutex);
        }
        else
        {
            ret = 1U;
            fault_code = BQ76940_RT_FAULT_CTX_LOCK;
            fault_stage = BQ76940_RT_STAGE_CALIB_SNAPSHOT;
        }

        /*
         * 2. I2C 硬件读取
         */
        if (ret == 0U)
        {
            if (xSemaphoreTake(g_i2c_bus_mutex,
                               pdMS_TO_TICKS(BMS_I2C_MUTEX_TIMEOUT_MS)) == pdTRUE)
            {
                ret = BQ76940_AppSampleReadHw(&calib_snapshot, &sample);

                xSemaphoreGive(g_i2c_bus_mutex);

                if (ret != 0U)
                {
                    fault_code = BQ76940_RT_FAULT_SAMPLE_READ;
                    fault_stage = BQ76940_RT_STAGE_SAMPLE_READ_HW;
                }
            }
            else
            {
                ret = BMS_TASK_RET_I2C_LOCK_TIMEOUT;
                fault_code = BQ76940_RT_FAULT_I2C_LOCK_TIMEOUT;
                fault_stage = BQ76940_RT_STAGE_I2C_LOCK;
            }
        }
#if (BMS_TEST_FORCE_RUNTIME_FAULT != 0U)
        /*
         * 测试用：在任务正常跑起来后，连续制造几次采样失败。
         * 目的：触发 RuntimeDiag 连续失败计数，最终进入 runtime fault。
         */
        if (test_fail_left != 0U)
        {
            test_fail_left--;

            ret = 0xEEU;
            fault_code = BQ76940_RT_FAULT_SAMPLE_READ;
            fault_stage = BQ76940_RT_STAGE_SAMPLE_READ_HW;

            BMS_LOG_TEST_HW_FAULT("[TEST] sample fail:%d\r\n", test_fail_left);
        }
#endif

        /*
         * 3. 采样数据处理
         */
        if (ret == 0U)
        {
            ret = BQ76940_AppSampleProcess(&sample);
            if (ret != 0U)
            {
                fault_code = BQ76940_RT_FAULT_SAMPLE_PROCESS;
                fault_stage = BQ76940_RT_STAGE_SAMPLE_PROCESS;
            }
        }

        /*
         * 4. 提交采样结果 + RuntimeDiag 成功记录
         */



        if (ret == 0U)
        {
            if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
            {
                ret = BQ76940_AppSampleCommit(app, &sample);

                if (ret == 0U)
                {
                    BQ76940_AppRuntimeDiagRecordSampleOk(app, &recovered);

                    /*
                     * 未处于 runtime fault 时，才继续主业务链。
                     * 如果刚刚自动恢复，也允许继续。
                     */
                    if (BQ76940_AppRuntimeDiagIsFaultActive(app) == 0U)
                    {
                        notify_protect = 1U;
                    }
                }
                else
                {
                    fault_code = BQ76940_RT_FAULT_SAMPLE_COMMIT;
                    fault_stage = BQ76940_RT_STAGE_SAMPLE_COMMIT;
                }

                xSemaphoreGive(g_bms_ctx_mutex);

                if (notify_protect != 0)
                {
                    xSemaphoreGive(g_protect_sem);
                }
            }
            else
            {
                ret = 2U;
                fault_code = BQ76940_RT_FAULT_CTX_LOCK;
                fault_stage = BQ76940_RT_STAGE_SAMPLE_COMMIT;
            }
        }

        /*
         * 5. 失败路径：记录 RuntimeDiag，必要时通知 RuntimeTask
         */
        if (ret != 0U)
        {
            if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
            {
                BQ76940_AppRuntimeDiagRecordSampleFail(app,
                                                       fault_code,
                                                       fault_stage,
                                                       ret,
                                                       &enter_fault);

                if (enter_fault != 0)
                {
                    BMS_AfeWriteInhibitSet();
                }

                xSemaphoreGive(g_bms_ctx_mutex);
            }

            /*
             * 第一次进入 runtime fault 时，通知 RuntimeTask 执行 Safe-Off。
             */
            if (enter_fault != 0U)
            {
                /*
                 * 再唤醒 RuntimeTask
                 */
                xSemaphoreGive(g_runtime_sem);
            }

            BMS_LOG_PERIODIC("[RT] sample fail:%d/%d/%d\r\n",
                   ret,
                   fault_code,
                   fault_stage);
        }

        vTaskDelay(pdMS_TO_TICKS(BMS_SAMPLE_TASK_PERIOD_MS));
    }
}

static void BMS_RuntimeTask(void *argument)
{
    BQ76940_AppCtx_t *app = (BQ76940_AppCtx_t *)argument;

    for (;;)
    {
        /*
         * RuntimeTask 平时阻塞等待。
         * SampleTask 在首次进入 runtime fault 时，会 give g_runtime_sem。
         */
        if (xSemaphoreTake(g_runtime_sem, portMAX_DELAY) == pdTRUE)
        {
            uint8_t need_safe_off = 0U;
            uint8_t safe_off_result = SAFE_OFF_FAIL_NONE;
            uint8_t retry_allowed = 0U;

            /*
             * 1. 读取并消费 Safe-Off 请求
             *
             * 这里只访问 runtime_diag 状态，所以只需要 ctx mutex。
             */
            if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
            {
                BQ76940_AppRuntimeDiagTakeSafeOffRequest(app, &need_safe_off);

                xSemaphoreGive(g_bms_ctx_mutex);
            }

            /*
             * 没有 Safe-Off 请求，说明可能是误唤醒，直接继续等待。
             */
            if (need_safe_off == 0U)
            {
                BMS_LOG_RUNTIME("[RT] no safe-off req\r\n");
                continue;
            }

            /*
             * 防御性锁存 AFE 写禁止状态。
             *
             * 正常情况下 SampleTask 已经提前设置；
             * 此处再次设置是为了防止未来其他路径发起 Safe-Off 时遗漏。
             */
            BMS_AfeWriteInhibitSet();

            /*
             * 2. 立即关闭外部 BQ76200 执行层
             *
             * 这一步不需要 I2C，必须优先执行。
             * 即使 I2C 总线异常，外部执行层也能先进入 OFF。
             */
            if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
            {
                (void)BQ76940_AppForceExternalOff(app);
                BMS_LOG_RUNTIME("[RT] ext off\r\n");

                xSemaphoreGive(g_bms_ctx_mutex);
            }

            /*
             * 立刻通知 ControlTask。
             *
             * 作用：
             * 1. RuntimeTask 已经抢救式关闭 BQ76200。
             * 2. ControlTask 再根据 runtime_fault_active 刷新执行层状态机。
             */
            xSemaphoreGive(g_control_sem);

            /*
             * 3. 尝试关闭 BQ76940 AFE
             *
             * 这里采用 do-while：
             * - 第一次一定执行
             * - 如果失败且 RuntimeDiag 允许快速重试，则延时后继续执行
             */
            do
            {
                retry_allowed = 0U;
                safe_off_result = SAFE_OFF_FAIL_NONE;

                /*
                 * 3.1 只拿 I2C 锁，执行 BQ76940 硬件关断
                 *
                 * BQ76940_AppForceAfeOffHw() 只负责硬件动作：
                 * - CELLBAL = 0
                 * - BQ76940 CHG/DSG = OFF
                 *
                 * 注意：
                 * 这个函数不应该修改 app 状态。
                 */
                if (xSemaphoreTake(g_i2c_bus_mutex,
                                   pdMS_TO_TICKS(BMS_I2C_MUTEX_TIMEOUT_MS)) == pdTRUE)
                {
                    safe_off_result = BQ76940_AppForceAfeOffHw();

                    xSemaphoreGive(g_i2c_bus_mutex);
                }
                else
                {
                    /*
                     * I2C 锁超时，说明当前无法访问 BQ76940。
                     * 但是 BQ76200 已经提前关闭，所以外部执行层已进入安全态。
                     */
                    safe_off_result = SAFE_OFF_FAIL_I2C_LOCK;
                }

                /*
                 * 3.2 再拿 ctx 锁，提交软件状态和 RuntimeDiag 结果
                 *
                 * 注意：
                 * 这里不访问 I2C。
                 */
                if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
                {
                    /*
                     * 根据 safe_off_result 提交软件状态。
                     *
                     * 例如：
                     * - CELLBAL 写成功，才清 bal_active / bal_target_label
                     * - FET 写成功，才提交相关执行状态
                     */
                    BQ76940_AppForceAfeOffCommit(app, safe_off_result);

                    /*
                     * 提交 Safe-Off 结果。
                     *
                     * RuntimeDiag 内部根据失败次数决定：
                     * - retry_allowed = 1：允许继续快速重试
                     * - retry_allowed = 0：达到上限，进入故障保持
                     */
                    BQ76940_AppRuntimeDiagCommitSafeOffResult(app,
                                                              safe_off_result,
                                                              &retry_allowed);

                    xSemaphoreGive(g_bms_ctx_mutex);
                }
                else
                {
                    /*
                     * 理论上 portMAX_DELAY 基本不会失败。
                     * 如果真的失败，不再快速重试，避免状态不可控。
                     */
                    safe_off_result = SAFE_OFF_FAIL_CTX_LOCK;
                    retry_allowed = 0U;
                }

                BMS_LOG_RUNTIME("[RT] AFE off:%02X retry:%d\r\n",
                       safe_off_result,
                       retry_allowed);

                /*
                 * Safe-Off 失败且允许快速重试，则延迟一小段时间再试。
                 */
                if ((safe_off_result != SAFE_OFF_FAIL_NONE) &&
                    (retry_allowed != 0U))
                {
                    vTaskDelay(pdMS_TO_TICKS(BQ76940_RT_SAFE_OFF_RETRY_DELAY_MS));
                }

            } while ((safe_off_result != SAFE_OFF_FAIL_NONE) &&
                     (retry_allowed != 0U));

#if (BMS_TEST_SAFE_OFF_READBACK_ENABLE != 0U)
            BMS_RuntimeSafeOffReadback(app);
#endif

            /*
             * 4. 再通知一次 ControlTask
             *
             * 作用：
             * - 确保 runtime_fault_active 下 BQ76200 保持 OFF
             * - Safe-Off 成功或失败达到上限后，都让执行层重新同步状态
             */
            xSemaphoreGive(g_control_sem);
        }
    }
}

static void BMS_CANTask(void *argument)
{
    BQ76940_AppCtx_t *app = (BQ76940_AppCtx_t *)argument;
    BQ76940_AppCtx_t snapshot;

    for (;;)
    {
        /*
         * CANTask 不访问 I2C。
         *
         * 正确做法：
         *   1. 短时间拿 ctx mutex
         *   2. 复制 app 快照
         *   3. 释放 ctx mutex
         *   4. 锁外发送 CAN
         *
         * 这样可以避免 CAN 发送过程长时间占用全局状态锁。
         */
        if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
        {
            snapshot = *app;
            xSemaphoreGive(g_bms_ctx_mutex);

            BQ76940_AppSendCanTelemetry(&snapshot);

            if (snapshot.runtime_diag.fault_active != 0)
            {
                BQ76940_AppSendRuntimeFaultCan(&snapshot);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(BMS_CAN_TASK_PERIOD_MS));
    }
}

static void BMS_ProtectTask(void *argument)
{
    BQ76940_AppCtx_t *app = (BQ76940_AppCtx_t *)argument;

    for (;;)
    {
        if (xSemaphoreTake(g_protect_sem, portMAX_DELAY) == pdTRUE)
        {
            uint8_t ret = 0U;
            uint8_t runtime_fault = 0U;

            BQ76940_OtProtectRequest_t ot_req;
            BQ76940_UtProtectRequest_t ut_req;

            /*
             * 0. Runtime fault 门控
             *
             * 作用：
             *   防止旧的 protect_sem 被消费后，
             *   ProtectTask 继续基于旧采样数据执行保护/均衡链路。
             *
             * 注意：
             *   这里不做 Safe-Off。
             *   Safe-Off 由 RuntimeTask 统一处理。
             */
            if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
            {
                runtime_fault = BQ76940_AppRuntimeDiagIsFaultActive(app);
                xSemaphoreGive(g_bms_ctx_mutex);
            }
            else
            {
                ret = 1U;
            }

            if ((ret == 0U) && (runtime_fault != 0U))
            {
                /*
                 * 已经进入 Runtime fault：
                 * - 不更新软件保护
                 * - 不继续 give g_balance_sem
                 */
                continue;
            }

            /*
             * 1. Base + Decide 阶段
             *
             * Base:
             *   - 更新 UV / OV / DIFF / OT / UT 软件告警
             *
             * OT Decide:
             *   - 判断是否需要 CHG/DSG OFF 或 ON
             *
             * UT Decide:
             *   - 判断是否需要 CHG OFF 或 ON
             *
             * 这一步只读取/更新 app 状态，不主动访问 I2C。
             */
            if (ret == 0U)
            {
                if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
                {
                    ret = BQ76940_AppProtectUpdateBase(app);

                    if (ret == 0U)
                    {
                        ret = BQ76940_AppOtProtectDecide(app, &ot_req);
                    }

                    if (ret == 0U)
                    {
                        ret = BQ76940_AppUtProtectDecide(app, &ut_req);
                    }
                    xSemaphoreGive(g_bms_ctx_mutex);
                }
                else
                {
                    ret = 1U;
                }
            }

            /*
             * 2. ApplyHw 阶段
             *
             * 只有真正需要写 BQ76940 FET / CHG / DSG 时，
             * 才申请 I2C mutex。
             *
             * 执行顺序：
             *   1. OT
             *   2. UT
             *
             * 这样更安全：
             *   - OT 可同时关 CHG/DSG
             *   - UT 可进一步确保 CHG 关闭
             */
            if (ret == 0U)
            {
                if ((ot_req.action != BQ76940_OT_ACTION_NONE) ||
                    (ut_req.action != BQ76940_UT_ACTION_NONE) )
                {
                    if (xSemaphoreTake(g_i2c_bus_mutex,pdMS_TO_TICKS(BMS_I2C_MUTEX_TIMEOUT_MS)) == pdTRUE)
                    {

                        if (BMS_AfeWriteIsInhibited() != 0U)
                        {

                            xSemaphoreGive(g_i2c_bus_mutex);
                            continue;
                        }

                        if (ot_req.action != BQ76940_OT_ACTION_NONE)
                        {
                            ret = BQ76940_AppOtProtectApplyHw(&ot_req);
                        }

                        if ((ret == 0U) &&
                            (ut_req.action != BQ76940_UT_ACTION_NONE))
                        {
                            ret = BQ76940_AppUtProtectApplyHw(&ut_req);
                        }
                        xSemaphoreGive(g_i2c_bus_mutex);
                    }
                    else
                    {
                        ret = BMS_TASK_RET_I2C_LOCK_TIMEOUT;
                    }
                }
            }

            /*
             * 3. Commit 阶段
             *
             * 将 OT / UT / OCDSCD 的执行结果提交回 app。
             *
             * 注意：
             *   Commit 不访问 I2C，只修改 app 状态。
             */
            if (ret == 0U)
            {
                if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
                {
                    if (BMS_AfeWriteIsInhibited() != 0U)
                    {
                        xSemaphoreGive(g_bms_ctx_mutex);

                        /*
                         * 不提交旧保护动作。
                         * 通知 ControlTask 维持 BQ76200 OFF。
                         */
                        xSemaphoreGive(g_control_sem);
                        continue;
                    }

                    ret = BQ76940_AppOtProtectCommit(app, &ot_req);

                    if (ret == 0U)
                    {
                        ret = BQ76940_AppUtProtectCommit(app, &ut_req);
                    }

                    /*
                     * 保护阶段完成后，继续通知 BalanceTask。
                     *
                     * 注意：
                     *   如果 runtime fault 已经 active，
                     *   前面第 0 步已经 continue 了，
                     *   所以这里不会在故障状态下继续接力。
                     */
                    if (ret == 0U)
                    {
                        xSemaphoreGive(g_balance_sem);
                    }

                    xSemaphoreGive(g_bms_ctx_mutex);
                }
                else
                {
                    ret = 2U;
                }
            }

            if (ret != 0U)
            {
                BMS_LOG_ERROR("[PROT] fail:%d\r\n", ret);
            }
        }
    }
}

static void BMS_BalanceTask(void *argument)
{
    BQ76940_AppCtx_t *app = (BQ76940_AppCtx_t *)argument;

    for (;;)
    {
        if (xSemaphoreTake(g_balance_sem, portMAX_DELAY) == pdTRUE)
        {
            uint8_t ret = 0U;
            uint8_t runtime_fault = 0U;

            BQ76940_BalanceRequest_t bal_req;

            /*
             * 0. Runtime fault 门控
             *
             * 作用：
             *   防止旧的 balance_sem 被消费后，
             *   BalanceTask 继续基于旧采样数据执行均衡。
             *
             * 注意：
             *   如果故障发生前已经在均衡，
             *   RuntimeTask 的 Safe-Off 会负责清 CELLBAL。
             */
            if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
            {
                runtime_fault = BQ76940_AppRuntimeDiagIsFaultActive(app);
                xSemaphoreGive(g_bms_ctx_mutex);
            }
            else
            {
                ret = 1U;
            }

            if ((ret == 0U) && (runtime_fault != 0U))
            {
                /*
                 * 已经进入 Runtime fault：
                 * - 不再执行均衡 Decide
                 * - 不再写 CELLBAL
                 * - 主动通知 ControlTask，让 BQ76200 保持 OFF
                 */
                xSemaphoreGive(g_control_sem);
                continue;
            }

            /*
             * 1. Decide 阶段
             *
             * 只读取 app 状态，生成本轮均衡请求。
             * 不访问 I2C。
             */
            if (ret == 0U)
            {
                if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
                {
                    ret = BQ76940_AppBalanceDecide(app, &bal_req);
                    xSemaphoreGive(g_bms_ctx_mutex);
                }
                else
                {
                    ret = 2U;
                }
            }

            /*
             * 2. ApplyHw 阶段
             *
             * 只有 START / STOP 需要写 CELLBAL 时才访问 I2C。
             */
            if (ret == 0U)
            {
                if (bal_req.action != BQ76940_BAL_ACTION_NONE)
                {
                    if (xSemaphoreTake(g_i2c_bus_mutex,
                                       pdMS_TO_TICKS(BMS_I2C_MUTEX_TIMEOUT_MS)) == pdTRUE)
                    {
                        if (BMS_AfeWriteIsInhibited() != 0U)
                        {
                            /*
                             * Runtime fault 已锁存。
                             * 禁止旧 START 请求重新开启 CELLBAL。
                             */
                            xSemaphoreGive(g_i2c_bus_mutex);
                            xSemaphoreGive(g_control_sem);
                            continue;
                        }

                        ret = BQ76940_AppBalanceApplyHw(&bal_req);

                        xSemaphoreGive(g_i2c_bus_mutex);
                    }
                    else
                    {
                        ret = BMS_TASK_RET_I2C_LOCK_TIMEOUT;
                    }
                }
            }

            /*
             * 3. Commit 阶段
             *
             * 将均衡执行结果提交回 app。
             */
            if (ret == 0U)
            {
                if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
                {
                    if (BMS_AfeWriteIsInhibited() != 0U)
                    {
                        xSemaphoreGive(g_bms_ctx_mutex);
                        xSemaphoreGive(g_control_sem);
                        continue;
                    }

                    ret = BQ76940_AppBalanceCommit(app, &bal_req);

                    /*
                     * 均衡阶段结束后，继续通知 ControlTask。
                     */
                    if (ret == 0U)
                    {
                        xSemaphoreGive(g_control_sem);
                    }

                    xSemaphoreGive(g_bms_ctx_mutex);
                }
                else
                {
                    ret = 3U;
                }
            }

            if (ret != 0U)
            {
                BMS_LOG_ERROR("[BAL] fail:%d\r\n", ret);
            }
        }
    }
}

static void BMS_ControlTask(void *argument)
{
    BQ76940_AppCtx_t *app = (BQ76940_AppCtx_t *)argument;

    for (;;)
    {
        if (xSemaphoreTake(g_control_sem, portMAX_DELAY) == pdTRUE)
        {
            uint8_t ret = 0U;

            /*
             * ControlTask 当前只负责 BQ76200 执行层控制。
             *
             * BQ76200 通过 GPIO 控制：
             *   CHG_EN / DSG_EN / CP_EN / PCHG_EN
             *
             * 不访问 BQ76940 I2C，因此不需要 i2c_mutex。
             *
             * 但这里会读取保护状态，并更新 ctx->bq76200_exec，
             * 所以需要 ctx_mutex。
             */
            if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
            {
                ret = BQ76940_AppControlUpdate(app);

                xSemaphoreGive(g_bms_ctx_mutex);
            }
            else
            {
                ret = 1U;
            }

            if (ret != 0U)
            {
                BMS_LOG_ERROR("[CTRL] fail:%d\r\n", ret);
            }
        }
    }
}

static void BMS_AuxTask(void *argument)
{
    BQ76940_AppCtx_t *app = (BQ76940_AppCtx_t *)argument;

#if (BMS_LOG_PERIODIC_ENABLE == 0U)
    (void)app;
#endif

    for (;;)
    {
        led1_toggle();

#if (BMS_LOG_PERIODIC_ENABLE != 0U)
        {
            BQ76940_AppCtx_t snapshot;

            /* Copy under the mutex, print after releasing it. */
            if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
            {
                snapshot = *app;
                xSemaphoreGive(g_bms_ctx_mutex);
                BQ76940_AppPrintRuntime(&snapshot);
            }
        }
#endif

        vTaskDelay(pdMS_TO_TICKS(BMS_AUX_TASK_PERIOD_MS));
    }
}

#if (BMS_ENABLE_GAUGE_TASK != 0U)
static void BMS_GaugeTask(void *argument)
{
    (void)argument;

    for (;;)
    {
        uint8_t ret = 0U;

        /*
         * BQ34Z100 访问 I2C / SMBus，
         * 和 BQ76940 共用 SoftI2C1 总线，
         * 所以必须拿 i2c mutex。
         */
        if (xSemaphoreTake(g_i2c_bus_mutex,
                           pdMS_TO_TICKS(BMS_I2C_MUTEX_TIMEOUT_MS)) == pdTRUE)
        {
            ret = BQ34Z100_AppRunCycle(&g_bq34z100_ctx);

            xSemaphoreGive(g_i2c_bus_mutex);
        }
        else
        {
            ret = BMS_TASK_RET_I2C_LOCK_TIMEOUT;
        }

        if (ret == 0U)
        {
            BQ34Z100_AppPrint(&g_bq34z100_ctx);
        }
        else
        {
            BMS_LOG_ERROR("[GAUGE] fail:%d/%d\r\n",
                   ret,
                   g_bq34z100_ctx.last_error);
        }

        vTaskDelay(pdMS_TO_TICKS(BMS_GAUGE_TASK_PERIOD_MS));
    }
}
#endif

static void BMS_AfeWriteInhibitSet(void)
{
    taskENTER_CRITICAL();
    g_afe_write_inhibit = 1U;
    taskEXIT_CRITICAL();
}

static uint8_t BMS_AfeWriteIsInhibited(void)
{
    uint8_t inhibited;

    taskENTER_CRITICAL();
    inhibited = g_afe_write_inhibit;
    taskEXIT_CRITICAL();

    return inhibited;
}

#if (BMS_TEST_SAFE_OFF_READBACK_ENABLE != 0U)
static void BMS_RuntimeSafeOffReadback(BQ76940_AppCtx_t *app)
{
    BQ76940_CellBalRegs_t cellbal = {0xFFU, 0xFFU, 0xFFU};
    uint8_t sys_ctrl2 = 0xFFU;
    uint8_t cellbal_ret = BMS_TASK_RET_I2C_LOCK_TIMEOUT;
    uint8_t sys_ctrl2_ret = BMS_TASK_RET_I2C_LOCK_TIMEOUT;
    uint8_t chg_en;
    uint8_t dsg_en;
    uint8_t cp_en;
    uint8_t pchg_en;
    uint8_t fault_active = 0U;
    uint8_t inhibited;
    uint8_t readback_pass;

    /*
     * I2C 读回与 ctx 状态快照分开加锁，避免锁嵌套。
     * 本函数只验证并打印，不修改 Safe-Off 正式结果。
     */
    if (xSemaphoreTake(g_i2c_bus_mutex,
                       pdMS_TO_TICKS(BMS_I2C_MUTEX_TIMEOUT_MS)) == pdTRUE)
    {
        cellbal_ret = BQ76940_ReadCellBalRegs(&cellbal);
        sys_ctrl2_ret = BQ76940_ReadSysCtrl2(&sys_ctrl2);
        xSemaphoreGive(g_i2c_bus_mutex);
    }

    if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
    {
        fault_active = app->runtime_diag.fault_active;
        xSemaphoreGive(g_bms_ctx_mutex);
    }

    chg_en = BQ76200_CHG_EN_ReadBack();
    dsg_en = BQ76200_DSG_EN_ReadBack();
    cp_en = BQ76200_CP_EN_ReadBack();
    pchg_en = BQ76200_PCHG_EN_ReadBack();
    inhibited = BMS_AfeWriteIsInhibited();

    readback_pass =
        ((cellbal_ret == BQ76940_OK) &&
         (sys_ctrl2_ret == BQ76940_OK) &&
         (cellbal.cellbal1 == 0U) &&
         (cellbal.cellbal2 == 0U) &&
         (cellbal.cellbal3 == 0U) &&
         ((sys_ctrl2 & (BQ76940_SYS_CTRL2_CHG_ON |
                        BQ76940_SYS_CTRL2_DSG_ON)) == 0U) &&
         (chg_en == 0U) &&
         (dsg_en == 0U) &&
         (cp_en == 0U) &&
         (pchg_en == 0U) &&
         (fault_active != 0U) &&
         (inhibited != 0U))
            ? 1U
            : 0U;

    BMS_LOG_TEST_HW_FAULT("[TEST] RB I2C:%u/%u %02X/%02X/%02X/%02X\r\n",
           cellbal_ret,
           sys_ctrl2_ret,
           cellbal.cellbal1,
           cellbal.cellbal2,
           cellbal.cellbal3,
           sys_ctrl2);
    BMS_LOG_TEST_HW_FAULT("[TEST] RB GPIO:%u/%u/%u/%u F:%u I:%u\r\n",
           chg_en,
           dsg_en,
           cp_en,
           pchg_en,
           fault_active,
           inhibited);
    BMS_LOG_TEST_HW_FAULT("[TEST] RB:%s\r\n", (readback_pass != 0U) ? "PASS" : "FAIL");
}
#endif

void BMS_HwFaultNotifyFromISR(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (g_hw_fault_sem != NULL)
    {
        xSemaphoreGiveFromISR(g_hw_fault_sem,
                              &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void BMS_HwFaultTask(void *argument)
{
    BQ76940_AppCtx_t *app = (BQ76940_AppCtx_t*)argument;

    for (;;)
    {
        if (xSemaphoreTake(g_hw_fault_sem, portMAX_DELAY) == pdTRUE)
        {
            uint8_t ret = 0U;
            uint8_t apply_ret = 0U;
            uint8_t commit_ret = 0U;
            uint8_t notify_control = 0U;
            uint8_t sys_stat = 0U;

            BQ76940_OcdScdRequest_t req;

            BQ76940_AppOcdScdRequestClear(&req);

            BMS_LOG_HW_FAULT("[HW] alert\r\n");

      
						ret = BMS_HwFaultReadSysStat(&sys_stat);

						if (ret != 0U)
						{
								BMS_LOG_ERROR("[HW] SYS read fail:%d\r\n", ret);
								continue;
						}
						
            if ((sys_stat & BQ76940_SYS_STAT_CURRENT_FAULT_MASK) != 0U)
            {
                BMS_LOG_HW_FAULT("[HW] current:%02X\r\n",
                       (uint8_t)(sys_stat & BQ76940_SYS_STAT_CURRENT_FAULT_MASK));
            }

            if ((sys_stat & BQ76940_SYS_STAT_VOLTAGE_FAULT_MASK) != 0U)
            {
                BMS_LOG_HW_FAULT("[HW] voltage:%02X\r\n",
                       (uint8_t)(sys_stat & BQ76940_SYS_STAT_VOLTAGE_FAULT_MASK));
            }

            if ((sys_stat & BQ76940_SYS_STAT_DEVICE_XREADY) != 0U)
            {
                BMS_LOG_HW_FAULT("[HW] XREADY\r\n");
            }

            if ((sys_stat & BQ76940_SYS_STAT_OVRD_ALERT) != 0U)
            {
                BMS_LOG_HW_FAULT("[HW] OVRD\r\n");
            }

            if ((sys_stat & BQ76940_SYS_STAT_CC_READY) != 0U)
            {
                BMS_LOG_HW_FAULT("[HW] CC_READY\r\n");
            }

            /*
             * 当前 V1 只处理 OCD/SCD。
             * 如果本次 ALERT 不是 OCD/SCD，先返回等待下次事件。
             */
            if ((sys_stat & BQ76940_SYS_STAT_CURRENT_FAULT_MASK) == 0U)
            {
                BMS_LOG_TEST_HW_FAULT("[HW] no OCD/SCD\r\n");
                continue;
            }

            /*
             * 将本次 SYS_STAT 快照提交到 app，
             * 然后复用 OCD/SCD Decide 逻辑。
             */
            if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
            {
                app->sys_stat = sys_stat;

                ret = BQ76940_AppOcdScdDecide(app, &req);

                xSemaphoreGive(g_bms_ctx_mutex);
            }
            else
            {
                ret = 1U;
            }

            if (ret != 0U)
            {
                BMS_LOG_ERROR("[HW] decide fail:%d\r\n", ret);
                continue;
            }

            BMS_LOG_TEST_HW_FAULT("[HW] act:%d O:%d S:%d F:%02X\r\n",
                   req.action,
                   req.ocd_now,
                   req.scd_now,
                   req.hw_fault_now);

            /*
             * ApplyHw 阶段：
             * 尝试补写 BQ76940 DSG OFF。
             *
             * 注意：
             *   对 OCD/SCD 来说，真实硬件中 BQ76940 已经可能自动关断。
             *   这里的 ApplyHw 是 best-effort 补充动作。
             *   即使 I2C 锁失败，也不能阻止 Commit 锁存故障。
             */
            if (req.action != BQ76940_OCDSCD_ACTION_NONE)
            {
                if (xSemaphoreTake(g_i2c_bus_mutex,
                                   pdMS_TO_TICKS(BMS_I2C_MUTEX_TIMEOUT_MS)) == pdTRUE)
                {
                    apply_ret = BQ76940_AppOcdScdApplyHw(&req);

                    xSemaphoreGive(g_i2c_bus_mutex);
                }
                else
                {
                    apply_ret = BMS_TASK_RET_I2C_LOCK_TIMEOUT;
                }

                if (apply_ret != 0U)
                {
                    BMS_LOG_ERROR("[HW] apply fail:%d\r\n",
                           apply_ret);
                }
            }

            /*
             * Commit 阶段：
             * 只要 SYS_STAT 确认 OCD/SCD，就必须锁存软件故障状态。
             * 不能因为 ApplyHw 失败就不锁存。
             */
            if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
            {
                commit_ret = BQ76940_AppOcdScdCommit(app, &req);

                if (commit_ret == 0U)
                {
                    notify_control = 1U;
                }

                xSemaphoreGive(g_bms_ctx_mutex);
            }
            else
            {
                commit_ret = 2U;
            }

            if (commit_ret != 0U)
            {
                BMS_LOG_ERROR("[HW] commit fail:%d\r\n",
                       commit_ret);
                continue;
            }

            if (notify_control != 0U)
            {
                xSemaphoreGive(g_control_sem);
            }

            BMS_LOG_HW_FAULT("[HW] latched,DSG block:%d\r\n",
                   apply_ret);
        }
    }
}

#if (BMS_TEST_FAKE_ALERT_EXTI != 0U)
static void BMS_AlertSimTestTask(void *argument)
{
    (void)argument;

    vTaskDelay(pdMS_TO_TICKS(5000));

    BMS_LOG_TEST_ALERT("[TEST] alert trigger\r\n");

    BQ76940_AlertSimSoftwareTrigger();

    vTaskDelete(NULL);
}
#endif

static uint8_t BMS_HwFaultReadSysStat(uint8_t *sys_stat)
{
    uint8_t ret = 0U;

    if (sys_stat == 0)
    {
        return 1U;
    }

#if (BMS_TEST_FAKE_HW_FAULT != 0U)

    *sys_stat = BMS_TEST_FAKE_HW_FAULT_SYS_STAT;

    BMS_LOG_TEST_HW_FAULT("[HW] fake SYS:%02X\r\n", *sys_stat);

#else

    if (xSemaphoreTake(g_i2c_bus_mutex,pdMS_TO_TICKS(BMS_I2C_MUTEX_TIMEOUT_MS)) == pdTRUE)
    {
        ret = BQ76940_ReadSysStat(sys_stat);

        xSemaphoreGive(g_i2c_bus_mutex);
    }
    else
    {
        ret = BMS_TASK_RET_I2C_LOCK_TIMEOUT;
    }

    if (ret == 0U)
    {
        BMS_LOG_HW_FAULT("[HW] real SYS:%02X\r\n", *sys_stat);
    }

#endif

    return ret;
}
