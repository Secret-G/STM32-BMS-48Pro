#include "led.h"
#include "sys.h"

//初始化GPIO函数
void led_init(void)
{
    GPIO_InitTypeDef gpio_initstruct;
    //打开时钟
    __HAL_RCC_GPIOA_CLK_ENABLE();                           // 使能GPIOA时钟
		__HAL_RCC_AFIO_CLK_ENABLE();
		__HAL_AFIO_REMAP_SWJ_NOJTAG();
    
    //调用GPIO初始化函数
    gpio_initstruct.Pin = GPIO_PIN_15;                       // LED1对应的引脚
    gpio_initstruct.Mode = GPIO_MODE_OUTPUT_PP;             // 推挽输出
    gpio_initstruct.Pull = GPIO_PULLUP;                     // 上拉
    gpio_initstruct.Speed = GPIO_SPEED_FREQ_HIGH;           // 高速
    HAL_GPIO_Init(GPIOA, &gpio_initstruct);
    //关闭LED
    led1_off();
}

//点亮LED1的函数
void led1_on(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);   // 拉低LED1引脚，点亮LED1
}

//熄灭LED1的函数
void led1_off(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);     // 拉高LED1引脚，熄灭LED1
}

//翻转LED1状态的函数
void led1_toggle(void)
{
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_15);
}
