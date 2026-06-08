#include "io_ctrl.h"
#include "delay.h"

/*==================== 用户根据原理图修改这里 ====================*/
#define BQ_WAKE_GPIO_PORT     GPIOA
#define BQ_WAKE_GPIO_PIN      GPIO_PIN_8
#define BQ_WAKE_GPIO_CLK_EN() __HAL_RCC_GPIOB_CLK_ENABLE()
/*==============================================================*/

void IO_CTRL_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    BQ_WAKE_GPIO_CLK_EN();

    GPIO_InitStruct.Pin   = BQ_WAKE_GPIO_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(BQ_WAKE_GPIO_PORT, &GPIO_InitStruct);

    BQ_WAKE_Low();
}

void BQ_WAKE_High(void)
{
    HAL_GPIO_WritePin(BQ_WAKE_GPIO_PORT, BQ_WAKE_GPIO_PIN, GPIO_PIN_SET);
}

void BQ_WAKE_Low(void)
{
    HAL_GPIO_WritePin(BQ_WAKE_GPIO_PORT, BQ_WAKE_GPIO_PIN, GPIO_PIN_RESET);
}

void BQ_WAKE_Pulse(void)
{
    BQ_WAKE_High();
    delay_ms(100);
    BQ_WAKE_Low();
		delay_ms(20);
}
