#ifndef __BQ76940_ALERT_SIM_H
#define __BQ76940_ALERT_SIM_H

#include "sys.h"
#include <stdint.h>

#define BQ76940_ALERT_SIM_GPIO_PORT        GPIOB
#define BQ76940_ALERT_SIM_GPIO_PIN         GPIO_PIN_14
#define BQ76940_ALERT_SIM_GPIO_CLK_EN()    __HAL_RCC_GPIOB_CLK_ENABLE()

#define BQ76940_ALERT_SIM_EXTI_IRQn        EXTI15_10_IRQn

void BQ76940_AlertSimInit(void);
void BQ76940_AlertSimSoftwareTrigger(void);
uint8_t BQ76940_AlertSimRead(void);
void BQ76940_AlertSimOnExti(void);

#endif
