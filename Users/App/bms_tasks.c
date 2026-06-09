#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "bms_tasks.h"

#include "stdio.h"
#include "led.h"
#include "bq34z100_app.h"

#define BMS_SAMPLE_TASK_STACK_WORDS      512U
#define BMS_CAN_TASK_STACK_WORDS         256U
#define LED_TASK_STACK_WORDS             128U

#define BMS_SAMPLE_TASK_PRIORITY         ( tskIDLE_PRIORITY + 3U )
#define BMS_CAN_TASK_PRIORITY            ( tskIDLE_PRIORITY + 2U )
#define LED_TASK_PRIORITY                ( tskIDLE_PRIORITY + 1U )

#define BMS_SAMPLE_TASK_PERIOD_MS        500U
#define BMS_CAN_TASK_PERIOD_MS           1000U


static SemaphoreHandle_t g_bms_ctx_mutex = NULL;

/*
 * BQ34Z100-G1 电量计模块运行开关
 * 0：暂不运行，只保留代码入口
 * 1：在 BMS_SampleTask 中周期刷新
 */
#define BMS_CORE_RUN_BQ34Z100          0U

#if (BMS_CORE_RUN_BQ34Z100 != 0U)
static BQ34Z100_AppCtx_t g_bq34z100_ctx;
#endif

/*
 * BQ34Z100 刷新周期计数
 * 当前 BMS_SampleTask 周期约 100ms，
 * 10 次约等于 1s。
 */
#define BMS_CORE_BQ34Z100_PERIOD_CNT   10U


static void BMS_SampleTask(void *argument);
static void BMS_CANTask(void *argument);
static void LED_Task(void *argument);


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

    g_bms_ctx_mutex = xSemaphoreCreateMutex();
    if (g_bms_ctx_mutex == NULL)
    {
        printf("[FreeRTOS] create ctx mutex fail\r\n");
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

    result = xTaskCreate(LED_Task,
                         "LED",
                         LED_TASK_STACK_WORDS,
                         NULL,
                         LED_TASK_PRIORITY,
                         NULL);

    return result;
}

static void BMS_SampleTask(void *argument)
{
    BQ76940_AppCtx_t *app = (BQ76940_AppCtx_t *)argument;

#if (BMS_CORE_RUN_BQ34Z100 != 0U)
    uint8_t bq34_cnt = 0U;
#endif

    for (;;)
    {
        uint8_t ret = 0U;

        /*
         * 1. BQ76940 主流程
         * 负责采样、告警、保护、均衡、执行层状态更新。
         * 注意：CAN 上报已经拆到 BMS_CANTask 中。
         */
        if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
        {
            ret = BQ76940_AppRunCycle(app);

            xSemaphoreGive(g_bms_ctx_mutex);
        }

        if (ret != 0U)
        {
            printf("[BMS_SampleTask] BQ76940_AppRunCycle fail, ret = %d\r\n", ret);
            vTaskDelay(pdMS_TO_TICKS(1000U));
            continue;
        }

#if (BMS_CORE_RUN_BQ34Z100 != 0U)
        /*
         * 2. BQ34Z100 电量计周期刷新
         * 当前暂时保留在 SampleTask 中。
         * 后续可拆分为 BMS_GaugeTask。
         */
        bq34_cnt++;
        if (bq34_cnt >= BMS_CORE_BQ34Z100_PERIOD_CNT)
        {
            uint8_t bq34_ret;

            bq34_cnt = 0U;

            bq34_ret = BQ34Z100_AppRunCycle(&g_bq34z100_ctx);
            if (bq34_ret == 0U)
            {
                BQ34Z100_AppPrint(&g_bq34z100_ctx);
            }
            else
            {
                printf("[BQ34Z100] run fail, ret = %d, last_error = %d\r\n",
                       bq34_ret,
                       g_bq34z100_ctx.last_error);
            }
        }
#endif

        vTaskDelay(pdMS_TO_TICKS(BMS_SAMPLE_TASK_PERIOD_MS));
    }
}


static void LED_Task(void *argument)
{
    (void)argument;

    for (;;)
    {
        led1_toggle();
        vTaskDelay(pdMS_TO_TICKS(500U));
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
