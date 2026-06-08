#include "sys.h"
#include "delay.h"
#include "led.h"
#include "uart1.h"
#include "io_ctrl.h"
#include "soft_i2c1.h"
#include "can_drv.h"
#include "bq76940_app.h"
#include "bms_tasks.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stdio.h"

int main(void)
{
    uint8_t ret = 0;
    static BQ76940_AppCtx_t app;

    HAL_Init();
    stm32_clock_init(RCC_PLL_MUL9);

    led_init();
    uart1_init(115200);
    IO_CTRL_Init();
    SoftI2C1_Init();
	
		ret = CAN_DrvInit();
		if (ret != 0)
		{
				printf("[MAIN] CAN_DrvInit fail, ret = %d\r\n", ret);
		}
		else
		{
				printf("[MAIN] CAN_DrvInit ok\r\n");
		}

    /* 1. 初始化默认配置 */
    BQ76940_AppInitDefaultConfig(&app);

    /* 2. 上电 bring-up + 自检 */
    ret = BQ76940_AppBringUpAndSelfTest(&app);
    if (ret != 0)
    {
        printf("BQ76940_AppBringUpAndSelfTest fail, ret = %d\r\n", ret);
        while (1)
        {
            led1_on();
            delay_ms(100);
            led1_off();
            delay_ms(100);
        }
    }

    /* 3. 创建最小任务框架 */
    if (BMS_TasksCreate(&app) != pdPASS)
    {
        printf("[MAIN] BMS_TasksCreate fail\r\n");
        while (1)
        {
        }
    }

    /* 4. 启动 FreeRTOS 调度器，正常情况下不会返回 */
    vTaskStartScheduler();

    /* 调度器启动失败（通常是空闲任务内存申请失败） */
    while (1)
    {
    }
}
