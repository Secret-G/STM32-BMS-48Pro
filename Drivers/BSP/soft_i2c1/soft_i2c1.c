#include "soft_i2c1.h"
#include "delay.h"

/* PB8 -> SCL
 * PB9 -> SDA
 */
#define SOFT_I2C1_SCL_PORT         GPIOB
#define SOFT_I2C1_SCL_PIN          GPIO_PIN_8
#define SOFT_I2C1_SDA_PORT         GPIOB
#define SOFT_I2C1_SDA_PIN          GPIO_PIN_9
#define SOFT_I2C1_GPIO_CLK_EN()    __HAL_RCC_GPIOB_CLK_ENABLE()

/* I2C时序延时，软件I2C速度不要太快 */
#define SOFT_I2C1_DELAY_US(x)      delay_us(x)

/*-------------------- SDA模式切换 --------------------*/
static void SoftI2C1_SDA_Out(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin   = SOFT_I2C1_SDA_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(SOFT_I2C1_SDA_PORT, &GPIO_InitStruct);
}

static void SoftI2C1_SDA_In(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin  = SOFT_I2C1_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(SOFT_I2C1_SDA_PORT, &GPIO_InitStruct);
}

/*-------------------- 引脚操作 --------------------*/
static void SoftI2C1_SCL_High(void)
{
    HAL_GPIO_WritePin(SOFT_I2C1_SCL_PORT, SOFT_I2C1_SCL_PIN, GPIO_PIN_SET);
}

static void SoftI2C1_SCL_Low(void)
{
    HAL_GPIO_WritePin(SOFT_I2C1_SCL_PORT, SOFT_I2C1_SCL_PIN, GPIO_PIN_RESET);
}

static void SoftI2C1_SDA_High(void)
{
    HAL_GPIO_WritePin(SOFT_I2C1_SDA_PORT, SOFT_I2C1_SDA_PIN, GPIO_PIN_SET);
}

static void SoftI2C1_SDA_Low(void)
{
    HAL_GPIO_WritePin(SOFT_I2C1_SDA_PORT, SOFT_I2C1_SDA_PIN, GPIO_PIN_RESET);
}

static uint8_t SoftI2C1_ReadSDA(void)
{
    return (uint8_t)HAL_GPIO_ReadPin(SOFT_I2C1_SDA_PORT, SOFT_I2C1_SDA_PIN);
}

/*-------------------- 初始化 --------------------*/
void SoftI2C1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    SOFT_I2C1_GPIO_CLK_EN();

    /* SCL: 开漏输出 */
    GPIO_InitStruct.Pin   = SOFT_I2C1_SCL_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(SOFT_I2C1_SCL_PORT, &GPIO_InitStruct);

    /* SDA: 开漏输出 */
    GPIO_InitStruct.Pin   = SOFT_I2C1_SDA_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(SOFT_I2C1_SDA_PORT, &GPIO_InitStruct);

    /* 总线空闲状态：SCL=1, SDA=1 */
    SoftI2C1_SCL_High();
    SoftI2C1_SDA_High();
}

/*-------------------- I2C基础时序 --------------------*/
void SoftI2C1_Start(void)
{
    SoftI2C1_SDA_Out();

    SoftI2C1_SDA_High();
    SoftI2C1_SCL_High();
    SOFT_I2C1_DELAY_US(5);

    SoftI2C1_SDA_Low();
    SOFT_I2C1_DELAY_US(5);

    SoftI2C1_SCL_Low();
    SOFT_I2C1_DELAY_US(5);
}

void SoftI2C1_Stop(void)
{
    SoftI2C1_SDA_Out();

    SoftI2C1_SCL_Low();
    SoftI2C1_SDA_Low();
    SOFT_I2C1_DELAY_US(5);

    SoftI2C1_SCL_High();
    SOFT_I2C1_DELAY_US(5);

    SoftI2C1_SDA_High();
    SOFT_I2C1_DELAY_US(5);
}

uint8_t SoftI2C1_WaitAck(void)
{
    uint8_t wait_time = 0;

    SoftI2C1_SDA_In();
    SoftI2C1_SDA_High();      /* 释放SDA */
    SOFT_I2C1_DELAY_US(2);

    SoftI2C1_SCL_High();
    SOFT_I2C1_DELAY_US(5);

    while (SoftI2C1_ReadSDA())
    {
        wait_time++;
        if (wait_time > 100)
        {
            SoftI2C1_Stop();
            return 1;   /* 无应答 */
        }
        SOFT_I2C1_DELAY_US(1);
    }

    SoftI2C1_SCL_Low();
    return 0;           /* 收到ACK */
}

void SoftI2C1_Ack(void)
{
    SoftI2C1_SCL_Low();
    SoftI2C1_SDA_Out();
    SoftI2C1_SDA_Low();
    SOFT_I2C1_DELAY_US(2);

    SoftI2C1_SCL_High();
    SOFT_I2C1_DELAY_US(5);

    SoftI2C1_SCL_Low();
}

void SoftI2C1_NAck(void)
{
    SoftI2C1_SCL_Low();
    SoftI2C1_SDA_Out();
    SoftI2C1_SDA_High();
    SOFT_I2C1_DELAY_US(2);

    SoftI2C1_SCL_High();
    SOFT_I2C1_DELAY_US(5);

    SoftI2C1_SCL_Low();
}

void SoftI2C1_SendByte(uint8_t byte)
{
    uint8_t i;

    SoftI2C1_SDA_Out();
    SoftI2C1_SCL_Low();

    for (i = 0; i < 8; i++)
    {
        if (byte & 0x80)
            SoftI2C1_SDA_High();
        else
            SoftI2C1_SDA_Low();

        byte <<= 1;
        SOFT_I2C1_DELAY_US(2);

        SoftI2C1_SCL_High();
        SOFT_I2C1_DELAY_US(5);

        SoftI2C1_SCL_Low();
        SOFT_I2C1_DELAY_US(2);
    }
}

uint8_t SoftI2C1_ReadByte(uint8_t ack)
{
    uint8_t i;
    uint8_t receive = 0;

    SoftI2C1_SDA_In();

    for (i = 0; i < 8; i++)
    {
        SoftI2C1_SCL_Low();
        SOFT_I2C1_DELAY_US(2);

        SoftI2C1_SCL_High();
        receive <<= 1;
        if (SoftI2C1_ReadSDA())
            receive++;

        SOFT_I2C1_DELAY_US(5);
    }

    SoftI2C1_SCL_Low();

    if (ack)
        SoftI2C1_Ack();
    else
        SoftI2C1_NAck();

    return receive;
}

uint8_t SoftI2C1_ReadRegs(uint8_t dev_addr,
                          uint8_t reg_addr,
                          uint8_t *buf,
                          uint8_t len)
{
    uint8_t i;

    if ((buf == 0) || (len == 0))
    {
        return 1;
    }

    SoftI2C1_Start();

    SoftI2C1_SendByte((dev_addr << 1) | 0);
    if (SoftI2C1_WaitAck())
    {
        return 2;
    }

    SoftI2C1_SendByte(reg_addr);
    if (SoftI2C1_WaitAck())
    {
        return 3;
    }

    SoftI2C1_Start();

    SoftI2C1_SendByte((dev_addr << 1) | 1);
    if (SoftI2C1_WaitAck())
    {
        return 4;
    }

    for (i = 0; i < len; i++)
    {
        /*
         * 不是最后一个字节就 ACK，
         * 最后一个字节 NACK，然后 Stop。
         */
        if (i < (len - 1U))
        {
            buf[i] = SoftI2C1_ReadByte(1);
        }
        else
        {
            buf[i] = SoftI2C1_ReadByte(0);
        }
    }

    SoftI2C1_Stop();

    return 0;
}


/*-------------------- 寄存器接口 --------------------*/
uint8_t SoftI2C1_WriteReg(uint8_t dev_addr, uint8_t reg_addr, uint8_t data)
{
    SoftI2C1_Start();

    SoftI2C1_SendByte((dev_addr << 1) | 0); //[7位设备地址][写位0]
    if (SoftI2C1_WaitAck())
        return 1;

    SoftI2C1_SendByte(reg_addr);
    if (SoftI2C1_WaitAck())
        return 2;

    SoftI2C1_SendByte(data);
    if (SoftI2C1_WaitAck())
        return 3;

    SoftI2C1_Stop();
    return 0;
}

uint8_t SoftI2C1_ReadReg(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data)
{
    SoftI2C1_Start();

    SoftI2C1_SendByte((dev_addr << 1) | 0);
    if (SoftI2C1_WaitAck())
        return 1;

    SoftI2C1_SendByte(reg_addr);
    if (SoftI2C1_WaitAck())
        return 2;

    SoftI2C1_Start();

    SoftI2C1_SendByte((dev_addr << 1) | 1);
    if (SoftI2C1_WaitAck())
        return 3;

    *data = SoftI2C1_ReadByte(0);
    SoftI2C1_Stop();

    return 0;
}
