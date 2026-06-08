#include "bq76200_exec_port.h"

void BQ76200_PortInit(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 4 个口都在 GPIOB，所以开一次时钟就够了 */
    BQ76200_CHG_EN_GPIO_CLK_EN();

    GPIO_InitStruct.Pin = BQ76200_CHG_EN_GPIO_PIN |
                          BQ76200_DSG_EN_GPIO_PIN |
                          BQ76200_CP_EN_GPIO_PIN  |
                          BQ76200_PCHG_EN_GPIO_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;   /* 这里 LOW 就够了 */

    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* 默认安全关闭态 */
    HAL_GPIO_WritePin(BQ76200_CHG_EN_GPIO_PORT,  BQ76200_CHG_EN_GPIO_PIN,  GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BQ76200_DSG_EN_GPIO_PORT,  BQ76200_DSG_EN_GPIO_PIN,  GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BQ76200_CP_EN_GPIO_PORT,   BQ76200_CP_EN_GPIO_PIN,   GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BQ76200_PCHG_EN_GPIO_PORT, BQ76200_PCHG_EN_GPIO_PIN, GPIO_PIN_RESET);
}

void BQ76200_CHG_EN_Write(uint8_t on)
{
    HAL_GPIO_WritePin(BQ76200_CHG_EN_GPIO_PORT,
                      BQ76200_CHG_EN_GPIO_PIN,
                      (on != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void BQ76200_DSG_EN_Write(uint8_t on)
{
    HAL_GPIO_WritePin(BQ76200_DSG_EN_GPIO_PORT,
                      BQ76200_DSG_EN_GPIO_PIN,
                      (on != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void BQ76200_CP_EN_Write(uint8_t on)
{
    HAL_GPIO_WritePin(BQ76200_CP_EN_GPIO_PORT,
                      BQ76200_CP_EN_GPIO_PIN,
                      (on != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void BQ76200_PCHG_EN_Write(uint8_t on)
{
    HAL_GPIO_WritePin(BQ76200_PCHG_EN_GPIO_PORT,
                      BQ76200_PCHG_EN_GPIO_PIN,
                      (on != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

uint8_t BQ76200_CHG_EN_ReadBack(void)
{
    return (HAL_GPIO_ReadPin(BQ76200_CHG_EN_GPIO_PORT,
                             BQ76200_CHG_EN_GPIO_PIN) == GPIO_PIN_SET) ? 1U : 0U;
}

uint8_t BQ76200_DSG_EN_ReadBack(void)
{
    return (HAL_GPIO_ReadPin(BQ76200_DSG_EN_GPIO_PORT,
                             BQ76200_DSG_EN_GPIO_PIN) == GPIO_PIN_SET) ? 1U : 0U;
}

uint8_t BQ76200_CP_EN_ReadBack(void)
{
    return (HAL_GPIO_ReadPin(BQ76200_CP_EN_GPIO_PORT,
                             BQ76200_CP_EN_GPIO_PIN) == GPIO_PIN_SET) ? 1U : 0U;
}

uint8_t BQ76200_PCHG_EN_ReadBack(void)
{
    return (HAL_GPIO_ReadPin(BQ76200_PCHG_EN_GPIO_PORT,
                             BQ76200_PCHG_EN_GPIO_PIN) == GPIO_PIN_SET) ? 1U : 0U;
}
