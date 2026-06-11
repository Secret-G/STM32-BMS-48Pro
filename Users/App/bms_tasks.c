#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "bms_tasks.h"

#include "stdio.h"
#include "led.h"
#include "bq34z100_app.h"

#define BMS_SAMPLE_TASK_STACK_WORDS      512U
#define BMS_PROTECT_TASK_STACK_WORDS     512U
#define BMS_BALANCE_TASK_STACK_WORDS     384U
#define BMS_CONTROL_TASK_STACK_WORDS     384U
#define BMS_CAN_TASK_STACK_WORDS         256U
#define BMS_GAUGE_TASK_STACK_WORDS       256U
#define BMS_AUX_TASK_STACK_WORDS         256U
#define BMS_RUNTIME_TASK_STACK_WORDS     512U


#define BMS_RUNTIME_TASK_PRIORITY        ( tskIDLE_PRIORITY + 4U )
#define BMS_SAMPLE_TASK_PRIORITY         ( tskIDLE_PRIORITY + 4U )
#define BMS_PROTECT_TASK_PRIORITY        ( tskIDLE_PRIORITY + 3U )
#define BMS_BALANCE_TASK_PRIORITY        ( tskIDLE_PRIORITY + 3U )
#define BMS_CONTROL_TASK_PRIORITY        ( tskIDLE_PRIORITY + 3U )
#define BMS_CAN_TASK_PRIORITY            ( tskIDLE_PRIORITY + 2U )
#define BMS_GAUGE_TASK_PRIORITY          ( tskIDLE_PRIORITY + 1U )
#define BMS_AUX_TASK_PRIORITY            ( tskIDLE_PRIORITY + 1U )


#define BMS_SAMPLE_TASK_PERIOD_MS        500U
#define BMS_CAN_TASK_PERIOD_MS           1000U
#define BMS_GAUGE_TASK_PERIOD_MS         1000U
#define BMS_AUX_TASK_PERIOD_MS           1000U

#define BMS_ENABLE_GAUGE_TASK            0U


#if (BMS_ENABLE_GAUGE_TASK != 0U)
static BQ34Z100_AppCtx_t g_bq34z100_ctx;
#endif

/*
 * BQ34Z100 刷新周期计数
 * 当前 BMS_SampleTask 周期约 100ms，
 * 10 次约等于 1s。
 */
#define BMS_CORE_BQ34Z100_PERIOD_CNT   10U

/*
 * I2C 总线互斥锁超时时间。
 * 作用：
 *   防止某个任务长期占用 I2C 总线后，其他任务永久阻塞。
 */
#define BMS_I2C_MUTEX_TIMEOUT_MS          100U

/*
 * FreeRTOS 任务层内部错误码：
 * 表示当前任务在规定时间内没有拿到 I2C 总线锁。
 */
#define BMS_TASK_RET_I2C_LOCK_TIMEOUT     0xF0U

/*
 * BMS 上下文互斥锁：
 *   保护 BQ76940_AppCtx_t 这个全局状态结构体。
 */
static SemaphoreHandle_t g_bms_ctx_mutex = NULL;

/*
 * I2C 总线互斥锁：
 *   保护 SoftI2C1 总线，防止多个任务同时访问 BQ76940 / BQ34Z100。
 */
static SemaphoreHandle_t g_i2c_bus_mutex = NULL;

static SemaphoreHandle_t g_protect_sem = NULL;
static SemaphoreHandle_t g_balance_sem = NULL;
static SemaphoreHandle_t g_control_sem = NULL;
static SemaphoreHandle_t g_runtime_sem = NULL;



static void BMS_SampleTask(void *argument);
static void BMS_ProtectTask(void *argument);
static void BMS_BalanceTask(void *argument);
static void BMS_ControlTask(void *argument);
static void BMS_CANTask(void *argument);
static void BMS_RuntimeTask(void *argument);


#if (BMS_ENABLE_GAUGE_TASK != 0U)
static void BMS_GaugeTask(void *argument);
#endif

static void BMS_AuxTask(void *argument);



void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name)
{
    (void)task;

    printf("[FreeRTOS] Stack overflow: %s\r\n",
           (task_name != NULL) ? task_name : "unknown");

    taskDISABLE_INTERRUPTS();
    while (1)
    {
    }
}

void vApplicationMallocFailedHook(void)
{
    printf("[FreeRTOS] Malloc failed\r\n");

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
        printf("[FreeRTOS] create ctx mutex fail\r\n");
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
        printf("[FreeRTOS] create i2c mutex fail\r\n");
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

		if ((g_protect_sem == NULL) ||
				(g_balance_sem == NULL) ||
				(g_control_sem == NULL) ||
				(g_runtime_sem == NULL))
		{
				printf("[FreeRTOS] create bms sem fail\r\n");
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
        printf("[FreeRTOS] create BMS_SampleTask fail\r\n");
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
        printf("[FreeRTOS] create BMS_ProtectTask fail\r\n");
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
        printf("[FreeRTOS] create BMS_BalanceTask fail\r\n");
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
        printf("[FreeRTOS] create BMS_ControlTask fail\r\n");
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
				printf("[FreeRTOS] create BMS_RuntimeTask fail\r\n");
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
        printf("[FreeRTOS] create BMS_CANTask fail\r\n");
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
        printf("[FreeRTOS] create BMS_GaugeTask fail\r\n");
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
        printf("[FreeRTOS] create BMS_AuxTask fail\r\n");
        return result;
    }

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

    BQ76940_AdcCalib_t calib_snapshot;
    BQ76940_AppSampleData_t sample;

    for (;;)
    {
        ret = 0U;
        fault_code = BQ76940_RT_FAULT_NONE;
        fault_stage = BQ76940_RT_STAGE_NONE;
        enter_fault = 0U;
        recovered = 0U;

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
				
				
				/*
				 * RuntimeDiag 测试代码：
				 * 第 10~12 轮人为制造连续 3 次采样失败。
				 * 测完一定要删除。
				 */
				static uint8_t rt_test_cnt = 0U;
				rt_test_cnt++;

				if ((rt_test_cnt >= 10U) && (rt_test_cnt <= 12U))
				{
						ret = 99U;
						fault_code = BQ76940_RT_FAULT_SAMPLE_READ;
						fault_stage = BQ76940_RT_STAGE_SAMPLE_READ_HW;
				}

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
                        xSemaphoreGive(g_protect_sem);
                    }
                }
                else
                {
                    fault_code = BQ76940_RT_FAULT_SAMPLE_COMMIT;
                    fault_stage = BQ76940_RT_STAGE_SAMPLE_COMMIT;
                }

                xSemaphoreGive(g_bms_ctx_mutex);
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

                xSemaphoreGive(g_bms_ctx_mutex);
            }

            /*
             * 第一次进入 runtime fault 时，通知 RuntimeTask 执行 Safe-Off。
             */
            if (enter_fault != 0U)
            {
                xSemaphoreGive(g_runtime_sem);
            }

            printf("[BMS_SampleTask] sample fail, ret=%d code=%d stage=%d\r\n",
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
        if (xSemaphoreTake(g_runtime_sem, portMAX_DELAY) == pdTRUE)
        {
            uint8_t need_safe_off = 0U;
            uint8_t safe_off_result = 0U;
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

            if (need_safe_off == 0U)
            {
                continue;
            }

            /*
             * 2. 立即关闭外部 BQ76200 执行层
             *
             * 这一步不需要 I2C，必须优先执行。
             * 即使 I2C 总线被占用或异常，外部驱动也能先进入 OFF。
             */
            if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
            {
                (void)BQ76940_AppForceExternalOff(app);
                xSemaphoreGive(g_bms_ctx_mutex);
            }

            /*
             * 立刻通知 ControlTask，让 BQ76200 执行层刷新一次。
             */
            xSemaphoreGive(g_control_sem);

            /*
             * 3. 尝试关闭 BQ76940 AFE
             *
             * 如果失败，则根据 RuntimeDiag 的 retry_allowed 快速重试。
             */
            do
            {
                retry_allowed = 0U;
                safe_off_result = 0U;

                if (xSemaphoreTake(g_i2c_bus_mutex,
                                   pdMS_TO_TICKS(BMS_I2C_MUTEX_TIMEOUT_MS)) == pdTRUE)
                {
                    if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
                    {
                        /*
                         * ForceAfeOff 会：
                         * - 清 CELLBAL
                         * - 关闭 BQ76940 CHG/DSG
                         */
                        safe_off_result = BQ76940_AppForceAfeOff(app);

                        BQ76940_AppRuntimeDiagCommitSafeOffResult(app,
                                                                  safe_off_result,
                                                                  &retry_allowed);

                        xSemaphoreGive(g_bms_ctx_mutex);
                    }
                    else
                    {
                        /*
                         * ctx 锁失败，记录为 Safe-Off 失败。
                         */
                        safe_off_result = BQ76940_RT_FAULT_CTX_LOCK;
                    }

                    xSemaphoreGive(g_i2c_bus_mutex);
                }
                else
                {
                    /*
                     * I2C 锁超时，说明当前无法访问 BQ76940。
                     * 但是 BQ76200 已经提前关闭，所以外部执行层是安全的。
                     */
                    safe_off_result = BMS_TASK_RET_I2C_LOCK_TIMEOUT;

                    if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
                    {
                        BQ76940_AppRuntimeDiagCommitSafeOffResult(app,
                                                                  safe_off_result,
                                                                  &retry_allowed);
                        xSemaphoreGive(g_bms_ctx_mutex);
                    }
                }

                /*
                 * Safe-Off 失败且允许快速重试，则延迟一小段时间再试。
                 */
                if (retry_allowed != 0U)
                {
                    vTaskDelay(pdMS_TO_TICKS(BQ76940_RT_SAFE_OFF_RETRY_DELAY_MS));
                }

            } while ((safe_off_result != 0U) &&
                     (retry_allowed != 0U));

            /*
             * 4. 再通知一次 ControlTask
             *
             * 作用：
             * - 确保 runtime_fault_active 下 BQ76200 保持 OFF
             * - 后续如果 control 里增加更多动作，也能被触发
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
            BQ76940_OcdScdRequest_t ocdscd_req;

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
                 * - 不执行 OT/UT/OCDSCD 动作
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
             * OCD/SCD Decide:
             *   - 根据 SYS_STAT 判断是否需要 DSG OFF 或 ON
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

                    if (ret == 0U)
                    {
                        ret = BQ76940_AppOcdScdDecide(app, &ocdscd_req);
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
             *   3. OCD/SCD
             *
             * 这样更安全：
             *   - OT 可同时关 CHG/DSG
             *   - UT 可进一步确保 CHG 关闭
             *   - OCD/SCD 可进一步确保 DSG 关闭
             */
            if (ret == 0U)
            {
                if ((ot_req.action != BQ76940_OT_ACTION_NONE) ||
                    (ut_req.action != BQ76940_UT_ACTION_NONE) ||
                    (ocdscd_req.action != BQ76940_OCDSCD_ACTION_NONE))
                {
                    if (xSemaphoreTake(g_i2c_bus_mutex,
                                       pdMS_TO_TICKS(BMS_I2C_MUTEX_TIMEOUT_MS)) == pdTRUE)
                    {
                        if (ot_req.action != BQ76940_OT_ACTION_NONE)
                        {
                            ret = BQ76940_AppOtProtectApplyHw(&ot_req);
                        }

                        if ((ret == 0U) &&
                            (ut_req.action != BQ76940_UT_ACTION_NONE))
                        {
                            ret = BQ76940_AppUtProtectApplyHw(&ut_req);
                        }

                        if ((ret == 0U) &&
                            (ocdscd_req.action != BQ76940_OCDSCD_ACTION_NONE))
                        {
                            ret = BQ76940_AppOcdScdApplyHw(&ocdscd_req);
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
                    ret = BQ76940_AppOtProtectCommit(app, &ot_req);

                    if (ret == 0U)
                    {
                        ret = BQ76940_AppUtProtectCommit(app, &ut_req);
                    }

                    if (ret == 0U)
                    {
                        ret = BQ76940_AppOcdScdCommit(app, &ocdscd_req);
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
                printf("[BMS_ProtectTask] protect fail, ret = %d\r\n", ret);
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
                printf("[BMS_BalanceTask] balance fail, ret = %d\r\n", ret);
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
                printf("[BMS_ControlTask] control fail, ret = %d\r\n", ret);
            }
        }
    }
}

static void BMS_AuxTask(void *argument)
{
    BQ76940_AppCtx_t *app = (BQ76940_AppCtx_t *)argument;
    BQ76940_AppCtx_t snapshot;

    for (;;)
    {
        led1_toggle();

        /*
         * printf 很慢，所以不能拿着 ctx mutex 打印。
         * 这里只复制快照，锁外打印。
         */
        if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
        {
            snapshot = *app;
            xSemaphoreGive(g_bms_ctx_mutex);

            BQ76940_AppPrintRuntime(&snapshot);
        }

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
            printf("[BMS_GaugeTask] BQ34Z100 run fail, ret = %d, last_error = %d\r\n",
                   ret,
                   g_bq34z100_ctx.last_error);
        }

        vTaskDelay(pdMS_TO_TICKS(BMS_GAUGE_TASK_PERIOD_MS));
    }
}
#endif
