#ifndef __BQ76200_EXEC_PORT_H
#define __BQ76200_EXEC_PORT_H

#include "sys.h"

/* =========================
 * Òý½ÅÓ³Éä
 * PB10 -> CHG_EN
 * PB11 -> DSG_EN
 * PB12 -> CP_EN
 * PB13 -> PCHG_EN
 * ========================= */
#define BQ76200_CHG_EN_GPIO_PORT      GPIOB
#define BQ76200_CHG_EN_GPIO_PIN       GPIO_PIN_10
#define BQ76200_CHG_EN_GPIO_CLK_EN()  __HAL_RCC_GPIOB_CLK_ENABLE()

#define BQ76200_DSG_EN_GPIO_PORT      GPIOB
#define BQ76200_DSG_EN_GPIO_PIN       GPIO_PIN_11
#define BQ76200_DSG_EN_GPIO_CLK_EN()  __HAL_RCC_GPIOB_CLK_ENABLE()

#define BQ76200_CP_EN_GPIO_PORT       GPIOB
#define BQ76200_CP_EN_GPIO_PIN        GPIO_PIN_12
#define BQ76200_CP_EN_GPIO_CLK_EN()   __HAL_RCC_GPIOB_CLK_ENABLE()

#define BQ76200_PCHG_EN_GPIO_PORT     GPIOB
#define BQ76200_PCHG_EN_GPIO_PIN      GPIO_PIN_13
#define BQ76200_PCHG_EN_GPIO_CLK_EN() __HAL_RCC_GPIOB_CLK_ENABLE()

void BQ76200_PortInit(void);

void BQ76200_CHG_EN_Write(uint8_t on);
void BQ76200_DSG_EN_Write(uint8_t on);
void BQ76200_CP_EN_Write(uint8_t on);
void BQ76200_PCHG_EN_Write(uint8_t on);

uint8_t BQ76200_CHG_EN_ReadBack(void);
uint8_t BQ76200_DSG_EN_ReadBack(void);
uint8_t BQ76200_CP_EN_ReadBack(void);
uint8_t BQ76200_PCHG_EN_ReadBack(void);

#endif
