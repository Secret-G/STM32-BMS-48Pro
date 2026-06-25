#include "sys.h"
#include "bms_log.h"
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
#include "bq76940_alert_sim.h"

/*
 * 功能：
 *   进入故障低功耗保持态。
 *
 * 使用场景：
 *   BQ76940 上电 bring-up / self-test 连续失败后，
 *   系统已经不能进入正常 BMS 运行流程。
 *
 * 进入 STOP 前应保证：
 *   1. BQ76200 驱动引脚已经全部关闭
 *      CHG_EN / DSG_EN / CP_EN / PCHG_EN = 0
 *   2. BQ76940 FET / 均衡已经尽力关闭
 *   3. CAN 故障报文已经发送
 *   4. LED 已经完成故障提示
 *
 * 注意：
 *   进入 STOP 后，CPU 停止运行，大部分外设时钟停止。
 *   因此不能指望 LED 继续闪烁，也不能指望 CAN 继续周期发送。
 *   当前 V1 版本进入 STOP 后等待人工复位 / 重新上电。
 */
static void BMS_EnterFaultStopMode(void)
{

    /*
     * 调试用：允许 MCU 进入 STOP 后仍保持调试连接。
     * 注意：这会增加低功耗下的功耗，正式版本不要依赖它。
     */
    HAL_DBGMCU_EnableDBGStopMode();

    /*
     * 进入低功耗前先关闭 LED，避免故障保持态下持续耗电。
     */
    led1_off();
    /*
     * 给最后一次串口打印 / CAN 发送留一点时间。
     * 这里不能太长，只是为了让外设完成最后动作。
     */
    delay_ms(50);

    /*
     * 进入 STOP 前暂停 SysTick。
     * 如果不暂停，SysTick 中断可能会把 MCU 很快唤醒。
     */
    HAL_SuspendTick();

    /*
     * 使能 PWR 外设时钟。
     * HAL_PWR_EnterSTOPMode() 依赖 PWR 模块。
     */
    __HAL_RCC_PWR_CLK_ENABLE();

    /*
     * 进入 STOP 模式：
     *
     * PWR_LOWPOWERREGULATOR_ON：
     *   使用低功耗稳压器，进一步降低功耗。
     *
     * PWR_STOPENTRY_WFI：
     *   执行 WFI 指令进入等待中断状态。
     *
     * 当前没有主动配置唤醒源，
     * 设计目标是故障后等待人工复位 / 重新上电。
     */

    BMS_LOG_RUNTIME("[MAIN] stop\r\n");
    HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);

    /*
     * 理论上，如果没有外部唤醒源，不会运行到这里。
     * 如果被意外唤醒，也不要继续执行正常流程。
     * 直接停在这里，等待人工复位。
     */
    while (1)
    {
    }
}

static uint8_t BMS_StartupSafeOff(BQ76940_AppCtx_t *app)
{
     uint8_t safe_off_result;

    if (app != 0)
    {
        (void)BQ76940_AppForceExternalOff(app);
    }
    
    safe_off_result = BQ76940_AppForceAfeOffHw();

    if (app != 0)
    {
        BQ76940_AppForceAfeOffCommit(app, safe_off_result);
    }
    return safe_off_result;
}


static void BMS_BringUpFaultHandler(BQ76940_AppCtx_t *app, uint8_t err)
{
    uint8_t safe_off_result;
    uint8_t i;

    BMS_LOG_ERROR("[MAIN] bringup:%d\r\n", err);

   safe_off_result = BMS_StartupSafeOff(app);

    /*
     * 4. 进入 STOP 前发送几次 CAN 故障帧。
     *
     *    0x305 用于表示 BQ76940 初始化 / 自检失败。
     */
    for (i = 0U; i < 3U; i++)
    {
        BQ76940_AppSendBringUpFaultCan(app, err, safe_off_result);
        delay_ms(100);
    }

    for (i = 0U; i < 5U; i++)
    {
        led1_on();
        delay_ms(100);
        led1_off();
        delay_ms(100);
    }

    /*
     * 4. 进入故障低功耗保持态。
     *    系统不创建 FreeRTOS 任务，不进入正常 BMS 运行流程。
     */
    BMS_EnterFaultStopMode();
}

static void BMS_RtosCreateFaultHandler(BQ76940_AppCtx_t *app, BaseType_t err)
{
    uint8_t i;
    uint8_t safe_off_result;

    BMS_LOG_ERROR("[MAIN] rtos init:%d\r\n", (int)err);

    safe_off_result = BMS_StartupSafeOff(app);


    for (i = 0U; i < 3U; i++)
    {
        BQ76940_AppSendRtosInitFaultCan(app, err, safe_off_result);
        delay_ms(100);
    }

    for (i = 0U; i < 5U; i++)
    {
        led1_on();
        delay_ms(100);
        led1_off();
        delay_ms(100);
    }

    BMS_EnterFaultStopMode();
}

int main(void)
{
    uint8_t ret = 0;
    static BQ76940_AppCtx_t app;
    BaseType_t result;

    HAL_Init();
    stm32_clock_init(RCC_PLL_MUL9);

    led_init();
    uart1_init(115200);
    IO_CTRL_Init();
    SoftI2C1_Init();
    BQ76940_AlertSimInit();

    ret = CAN_DrvInit();
    if (ret != 0)
    {
        BMS_LOG_ERROR("[MAIN] CAN:%d\r\n", ret);
    }
    else
    {
        BMS_LOG_CAN("[MAIN] CAN ok\r\n");
    }

    /* 1. 初始化默认配置 */
    BQ76940_AppInitDefaultConfig(&app);

    /* 2. 上电 bring-up + 自检 */
    ret = BQ76940_AppBringUpAndSelfTest(&app);
    if (ret != 0U)
    {
        BMS_LOG_ERROR("[MAIN] selftest:%d\r\n", ret);

        /*
         * BQ76940 初始化失败：
         * 不创建 FreeRTOS 任务，
         * 直接进入故障处理流程。
         */
        BMS_BringUpFaultHandler(&app, ret);

        BMS_LOG_RUNTIME("[MAIN] low power\r\n");
    }

    result = BMS_TasksCreate(&app);
    /* 3. 创建最小任务框架 */
    if (result != pdPASS)
    {
        BMS_LOG_ERROR("[MAIN] tasks fail\r\n");
        BMS_RtosCreateFaultHandler(&app,result);
    }

    /* 4. 启动 FreeRTOS 调度器，正常情况下不会返回 */
    vTaskStartScheduler();

    /* 调度器启动失败（通常是空闲任务内存申请失败） */
    while (1)
    {
    }
}
