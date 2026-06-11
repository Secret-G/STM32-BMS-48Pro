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




static void BMS_SampleTask(void *argument);
static void BMS_ProtectTask(void *argument);
static void BMS_BalanceTask(void *argument);
static void BMS_ControlTask(void *argument);
static void BMS_CANTask(void *argument);

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

    if ((g_protect_sem == NULL) ||
        (g_balance_sem == NULL) ||
        (g_control_sem == NULL))
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
    BQ76940_AdcCalib_t calib_snapshot;
    BQ76940_AppSampleData_t sample;

    for (;;)
    {
        ret = 0U;

        /*
         * 1. 先复制一份 ADC 校准参数。
         *
         * calib 在 bring-up 后基本不变，
         * 但它属于 app 全局上下文，
         * 所以这里短时间拿 ctx mutex 复制一份快照。
         */
        if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
        {
            calib_snapshot = app->calib;
            xSemaphoreGive(g_bms_ctx_mutex);
        }
        else
        {
            ret = 1U;
        }

        /*
         * 2. 申请 I2C 总线互斥锁，只保护硬件读取过程。
         *
         * 注意：
         *   这里不持有 ctx mutex。
         *   这样不会出现“拿着全局数据锁慢慢读 I2C”的问题。
         */
        if (ret == 0U)
        {
            if (xSemaphoreTake(g_i2c_bus_mutex,
                               pdMS_TO_TICKS(BMS_I2C_MUTEX_TIMEOUT_MS)) == pdTRUE)
            {
                ret = BQ76940_AppSampleReadHw(&calib_snapshot, &sample);

                xSemaphoreGive(g_i2c_bus_mutex);
            }
            else
            {
                ret = BMS_TASK_RET_I2C_LOCK_TIMEOUT;
            }
        }

        /*
         * 3. 采样数据处理。
         *
         * 这一步不访问 I2C，也不修改全局 app，
         * 所以不需要任何 mutex。
         */
        if (ret == 0U)
        {
            ret = BQ76940_AppSampleProcess(&sample);
        }

        /*
         * 4. 将采样结果提交到 app 全局上下文。
         *
         * 这一步会修改 app，
         * 所以只在这里短时间持有 ctx mutex。
         */
        if (ret == 0U)
        {
            if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
            {
                ret = BQ76940_AppSampleCommit(app, &sample);

                /*
                 * 采样提交成功后，再通知 ProtectTask。
                 * 这样 ProtectTask 一定能看到最新采样结果。
                 */
                if (ret == 0U)
                {
                    xSemaphoreGive(g_protect_sem);
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
            printf("[BMS_SampleTask] sample update fail, ret = %d\r\n", ret);
        }

        vTaskDelay(pdMS_TO_TICKS(BMS_SAMPLE_TASK_PERIOD_MS));
    }
}


static void BMS_CANTask(void *argument)
{
    BQ76940_AppCtx_t *app = (BQ76940_AppCtx_t *)argument;

    for (;;)
    {
        if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
        {
            BQ76940_AppSendCanTelemetry(app);
            xSemaphoreGive(g_bms_ctx_mutex);
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
            BQ76940_OtProtectRequest_t ot_req;
            BQ76940_UtProtectRequest_t ut_req;
            BQ76940_OcdScdRequest_t ocdscd_req;

            /*
             * 1. Base + Decide 阶段
             *
             * Base:
             *   - 更新 UV / OV / DIFF / OT / UT 告警
             *
             * OT Decide:
             *   - 判断是否需要 CHG/DSG OFF 或 ON
             *
             * UT Decide:
             *   - 判断是否需要 CHG OFF 或 ON
             *
             * OCD/SCD Decide:
             *   - 判断是否需要 DSG OFF 或 ON
             *
             * 这一步不主动访问 I2C。
             */
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
             * 这样较安全：
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
             * 将 OT / UT / OCDSCD 结果提交回 app。
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
            BQ76940_BalanceRequest_t req;

            /*
             * 1. Decide 阶段：
             *    只读取 app 当前状态，判断是否需要开启/关闭均衡。
             *
             *    这一步不访问 I2C。
             *    所以只需要 ctx mutex。
             */
            if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
            {
                ret = BQ76940_AppBalanceDecide(app, &req);
                xSemaphoreGive(g_bms_ctx_mutex);
            }
            else
            {
                ret = 1U;
            }

            /*
             * 2. ApplyHw 阶段：
             *    如果需要操作 CELLBAL，则拿 I2C mutex。
             *
             *    只有 BQ76940_AppBalanceApplyHw() 会访问 I2C。
             */
            if (ret == 0U)
            {
                if (req.action != BQ76940_BAL_ACTION_NONE)
                {
                    if (xSemaphoreTake(g_i2c_bus_mutex,
                                       pdMS_TO_TICKS(BMS_I2C_MUTEX_TIMEOUT_MS)) == pdTRUE)
                    {
                        ret = BQ76940_AppBalanceApplyHw(&req);

                        xSemaphoreGive(g_i2c_bus_mutex);
                    }
                    else
                    {
                        ret = BMS_TASK_RET_I2C_LOCK_TIMEOUT;
                    }
                }
            }

            /*
             * 3. Commit 阶段：
             *    将均衡结果提交回 app。
             *
             *    这一步不访问 I2C，只修改 app。
             */
            if (ret == 0U)
            {
                if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
                {
                    ret = BQ76940_AppBalanceCommit(app, &req);

                    /*
                     * 均衡阶段完成后通知 ControlTask。
                     * 注意：
                     *   即使本轮 req.action == NONE，只要 ret == 0，
                     *   也应该继续接力给 ControlTask。
                     */
                    if (ret == 0U)
                    {
                        xSemaphoreGive(g_control_sem);
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
             * ControlTask 可能会写 BQ76940 的 CHG / DSG 控制位，
             * 也可能会更新 BQ76200 执行层状态。
             *
             * 其中 BQ76940 FET 控制依赖 I2C，因此这里也要持有 I2C 锁。
             */
            if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
            {
                if (xSemaphoreTake(g_i2c_bus_mutex,
                                   pdMS_TO_TICKS(BMS_I2C_MUTEX_TIMEOUT_MS)) == pdTRUE)
                {
                    ret = BQ76940_AppControlUpdate(app);

                    xSemaphoreGive(g_i2c_bus_mutex);
                }
                else
                {
                    ret = BMS_TASK_RET_I2C_LOCK_TIMEOUT;
                }

                xSemaphoreGive(g_bms_ctx_mutex);
            }

            if (ret != 0U)
            {
                printf("[BMS_ControlTask] control update fail, ret = %d\r\n", ret);
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
        uint8_t ret;

        ret = BQ34Z100_AppRunCycle(&g_bq34z100_ctx);
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
