#include "bms_tasks.h"

#include "led.h"
#include "task.h"
#include "stdio.h"
#include "bq34z100_app.h"
#include "can_drv.h"

#define BMS_CORE_TASK_STACK_WORDS      512U
#define LED_TASK_STACK_WORDS           128U

#define BMS_CORE_TASK_PRIORITY         ( tskIDLE_PRIORITY + 2U )
#define LED_TASK_PRIORITY              ( tskIDLE_PRIORITY + 1U )


static BQ34Z100_AppCtx_t g_bq34z100_ctx;

/*
 * BQ34Z100-G1 电量计模块运行开关
 * 0：暂不运行，只保留代码入口
 * 1：在 BMS_CoreTask 中周期刷新
 */
#define BMS_CORE_RUN_BQ34Z100          0U

/*
 * BQ34Z100 刷新周期计数
 * 当前 BMS_CoreTask 周期约 100ms，
 * 10 次约等于 1s。
 */
#define BMS_CORE_BQ34Z100_PERIOD_CNT   10U


static void BMS_CoreTask(void *argument);
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
	
		BQ34Z100_AppInit(&g_bq34z100_ctx);
	

    result = xTaskCreate(BMS_CoreTask,
                         "BMS_Core",
                         BMS_CORE_TASK_STACK_WORDS,
                         app,
                         BMS_CORE_TASK_PRIORITY,
                         NULL);
    if (result != pdPASS)
    {
        return pdFAIL;
    }

    result = xTaskCreate(LED_Task,
                         "LED",
                         LED_TASK_STACK_WORDS,
                         NULL,
                         LED_TASK_PRIORITY,
                         NULL);

    return result;
}

static void BMS_CoreTask(void *argument)
{
    BQ76940_AppCtx_t *app = (BQ76940_AppCtx_t *)argument;

#if (BMS_CORE_RUN_BQ34Z100 != 0U)
    uint8_t bq34_cnt = 0U;
#endif

    for (;;)
    {
        uint8_t ret;

        /*
         * 1. BQ76940 主流程
         */
        ret = BQ76940_AppRunCycle(app);
        if (ret != 0U)
        {
            printf("BQ76940_AppRunCycle fail, ret = %d\r\n", ret);
            vTaskDelay(pdMS_TO_TICKS(1000U));
            continue;
        }

#if (BMS_CORE_RUN_BQ34Z100 != 0U)
        /*
         * 2. BQ34Z100 电量计周期刷新
         * 当前 BMS_CoreTask 周期约 100ms，
         * BMS_CORE_BQ34Z100_PERIOD_CNT = 10 时约 1s 刷新一次。
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
        vTaskDelay(pdMS_TO_TICKS(100U));
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
