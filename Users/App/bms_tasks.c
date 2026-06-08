#include "bms_tasks.h"

#include "led.h"
#include "task.h"
#include "stdio.h"

#define BMS_CORE_TASK_STACK_WORDS      512U
#define LED_TASK_STACK_WORDS           128U

#define BMS_CORE_TASK_PRIORITY         ( tskIDLE_PRIORITY + 2U )
#define LED_TASK_PRIORITY              ( tskIDLE_PRIORITY + 1U )

/*
 * Keep this disabled for the minimum framework migration.
 * Set it to 1 when BQ76940_AppRunCycle() should run inside BMS_CoreTask.
 */
#define BMS_CORE_RUN_BQ76940           1U

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

    for (;;)
    {
#if (BMS_CORE_RUN_BQ76940 != 0U)
        uint8_t ret;

        ret = BQ76940_AppRunCycle(app);
        if (ret != 0U)
        {
            printf("BQ76940_AppRunCycle fail, ret = %d\r\n", ret);
            vTaskDelay(pdMS_TO_TICKS(1000U));
            continue;
        }
#else
        (void)app;
        /* Reserved: call BQ76940_AppRunCycle(app) here when enabled. */
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
