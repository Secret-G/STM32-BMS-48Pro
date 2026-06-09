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

static SemaphoreHandle_t g_bms_ctx_mutex = NULL;
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

    g_bms_ctx_mutex = xSemaphoreCreateMutex();
    if (g_bms_ctx_mutex == NULL)
    {
        printf("[FreeRTOS] create ctx mutex fail\r\n");
        return pdFAIL;
    }
		
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

    for (;;)
    {
        uint8_t ret = 0U;

        if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
        {
            ret = BQ76940_AppSampleUpdate(app);

            /*
             * 采样成功后，在释放 mutex 之前先通知 ProtectTask。
             * 这样可以减少 CAN / Aux 在“采样已更新、保护未更新”中间插队读取 app 的概率。
             */
            if (ret == 0U)
            {
                xSemaphoreGive(g_protect_sem);
            }

            xSemaphoreGive(g_bms_ctx_mutex);
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

            if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
            {
                ret = BQ76940_AppProtectUpdate(app);

                /*
                 * 保护判断成功后，在释放 mutex 之前通知 BalanceTask。
                 * 这样主链会更紧凑：
                 * Sample -> Protect -> Balance -> Control
                 */
                if (ret == 0U)
                {
                    xSemaphoreGive(g_balance_sem);
                }

                xSemaphoreGive(g_bms_ctx_mutex);
            }

            if (ret != 0U)
            {
                printf("[BMS_ProtectTask] protect update fail, ret = %d\r\n", ret);
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

            if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
            {
                ret = BQ76940_AppBalanceUpdate(app);

                /*
                 * 均衡处理成功后，在释放 mutex 之前通知 ControlTask。
                 * ControlTask 会根据最新的保护/均衡/故障状态刷新执行层。
                 */
                if (ret == 0U)
                {
                    xSemaphoreGive(g_control_sem);
                }

                xSemaphoreGive(g_bms_ctx_mutex);
            }

            if (ret != 0U)
            {
                printf("[BMS_BalanceTask] balance update fail, ret = %d\r\n", ret);
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

            if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
            {
                ret = BQ76940_AppControlUpdate(app);
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

    for (;;)
    {
        led1_toggle();

        if (xSemaphoreTake(g_bms_ctx_mutex, portMAX_DELAY) == pdTRUE)
        {
            BQ76940_AppPrintRuntime(app);
            xSemaphoreGive(g_bms_ctx_mutex);
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
