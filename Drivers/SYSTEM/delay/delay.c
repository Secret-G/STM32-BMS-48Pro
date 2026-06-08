#include "delay.h"
#include "FreeRTOS.h"
#include "task.h"

static void delay_dwt_init(void)
{
    if ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) == 0U)
    {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    }

    if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0U)
    {
        DWT->CYCCNT = 0U;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }
}

/**
  * @brief  微秒级延时
  * @param  nus 延时时长，范围：0~233015
  * @retval 无
  */
void delay_us(uint32_t nus)
{
    uint32_t start;
    uint32_t cycles;

    if (nus == 0U)
    {
        return;
    }

    delay_dwt_init();
    cycles = (SystemCoreClock / 1000000U) * nus;
    start = DWT->CYCCNT;

    while ((uint32_t)(DWT->CYCCNT - start) < cycles)
    {
    }
}

/**
  * @brief  毫秒级延时
  * @param  nms 延时时长，范围：0~4294967295
  * @retval 无
  */
void delay_ms(uint32_t nms)
{
    if (nms == 0U)
    {
        return;
    }

    if ((xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) &&
        (__get_IPSR() == 0U))
    {
        vTaskDelay(pdMS_TO_TICKS(nms));
    }
    else
    {
        while (nms-- != 0U)
        {
            delay_us(1000U);
        }
    }
}
 
/**
  * @brief  秒级延时
  * @param  ns 延时时长，范围：0~4294967295
  * @retval 无
  */
void delay_s(uint32_t ns)
{
    while (ns-- != 0U)
    {
        delay_ms(1000U);
    }
}

/**
  * @brief  重写HAL_Delay函数
  * @param  nms 延时时长，范围：0~4294967295
  * @retval 无
  */
void HAL_Delay(uint32_t nms)
{
    delay_ms(nms);
}
