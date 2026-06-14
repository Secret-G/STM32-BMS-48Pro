#include "bq76940_alert_sim.h"
#include "stdio.h"
#include "bms_tasks.h"

void BQ76940_AlertSimInit(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    BQ76940_ALERT_SIM_GPIO_CLK_EN();

    /*
     * STM32F1 使用 EXTI 需要 AFIO 时钟。
     */
    __HAL_RCC_AFIO_CLK_ENABLE();

    /*
     * PB14 配置为外部中断输入。
     * 当前用于模拟 BQ76940 ALERT。
     */
    GPIO_InitStruct.Pin  = BQ76940_ALERT_SIM_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;

    HAL_GPIO_Init(BQ76940_ALERT_SIM_GPIO_PORT, &GPIO_InitStruct);

    /*
     * PB14 属于 EXTI15_10_IRQn。
     */
    HAL_NVIC_SetPriority(BQ76940_ALERT_SIM_EXTI_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(BQ76940_ALERT_SIM_EXTI_IRQn);
}

uint8_t BQ76940_AlertSimRead(void)
{
    return (HAL_GPIO_ReadPin(BQ76940_ALERT_SIM_GPIO_PORT,
                             BQ76940_ALERT_SIM_GPIO_PIN) == GPIO_PIN_SET) ? 1U : 0U;
}

/*软件测试触发，后续接入真是硬件可删除*/
void BQ76940_AlertSimSoftwareTrigger(void)
{
    /*
     * 软件触发 EXTI14。
     * 这一步模拟 ALERT 引脚产生中断。
     */
    EXTI->SWIER |= (1U << 14);
}



void BQ76940_AlertSimOnExti(void)
{
    BMS_HwFaultNotifyFromISR();
}
