#include "can_drv.h"
#include "bms_log.h"
#include "stdio.h"

static CAN_HandleTypeDef hcan;

static uint8_t can_ready = 0U;

static QueueHandle_t g_can_rx_queue = NULL;
static volatile uint16_t g_can_rx_drop_count = 0U;



uint8_t CAN_DrvInit(void)
{
    CAN_FilterTypeDef filter;

    can_ready = 0U;

    /*
     * 1. 绑定 CAN1 外设
     */
    hcan.Instance = CAN1;

    /*
     * 2. 正常模式
     */
    hcan.Init.Mode = CAN_MODE_NORMAL;

    /*
     * 3. 位时序配置：500kbps
     * APB1 = 36MHz
     * Baud = 36MHz / 4 / (1 + 13 + 4) = 500kbps
     */
    hcan.Init.Prescaler = 4;
    hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
    hcan.Init.TimeSeg1 = CAN_BS1_13TQ;
    hcan.Init.TimeSeg2 = CAN_BS2_4TQ;

    /*
     * 4. CAN 工作特性配置
     */
    hcan.Init.TimeTriggeredMode = DISABLE;
    hcan.Init.AutoBusOff = DISABLE;
    hcan.Init.AutoWakeUp = DISABLE;
    hcan.Init.AutoRetransmission = ENABLE;
    hcan.Init.ReceiveFifoLocked = DISABLE;
    hcan.Init.TransmitFifoPriority = DISABLE;

    /*
     * 5. 初始化 CAN
     * 这里内部会自动调用 HAL_CAN_MspInit()
     */
    if (HAL_CAN_Init(&hcan) != HAL_OK)
    {
        BMS_LOG_ERROR("[CAN] init fail\r\n");
        return 1;
    }

    /*
     * 6. 配置过滤器：当前接收所有 ID
     */
    filter.FilterBank = 0;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;

    filter.FilterIdHigh = 0x0000;
    filter.FilterIdLow = 0x0000;
    filter.FilterMaskIdHigh = 0x0000;
    filter.FilterMaskIdLow = 0x0000;

    filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterActivation = ENABLE;
    filter.SlaveStartFilterBank = 14;

    if (HAL_CAN_ConfigFilter(&hcan, &filter) != HAL_OK)
    {
        BMS_LOG_ERROR("[CAN] filter fail\r\n");
        return 2;
    }

    /*
     * 7. 启动 CAN
     */
    if (HAL_CAN_Start(&hcan) != HAL_OK)
    {
        BMS_LOG_ERROR("[CAN] start fail\r\n");
        return 3;
    }
		
		if(HAL_CAN_ActivateNotification(&hcan,CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
		{
				BMS_LOG_ERROR("[CAN] rx int fail\r\n");
				return 4;
		}

    can_ready = 1U;

    BMS_LOG_CAN("[CAN] 500k ok\r\n");

    return 0;
}

void HAL_CAN_MspInit(CAN_HandleTypeDef *hcan)
{
	
	
	
	

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (hcan->Instance == CAN1)
    {
        /*
         * 1. 打开 GPIOA 时钟
         * CAN1 默认引脚：
         * PA11 -> CAN_RX
         * PA12 -> CAN_TX
         */
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /*
         * 2. 打开 CAN1 外设时钟
         */
        __HAL_RCC_CAN1_CLK_ENABLE();

        /*
         * 3. PA12 -> CAN_TX
         * 复用推挽输出，由 CAN 外设控制输出
         */
        GPIO_InitStruct.Pin   = GPIO_PIN_12;
        GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        /*
         * 4. PA11 -> CAN_RX
         * 输入模式，用于接收 TJA1050 输出的 RXD 信号
         */
        GPIO_InitStruct.Pin  = GPIO_PIN_11;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_INPUT;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
				
				
				/*
				 * 5. 打开 CAN RX0中断
				 */
				HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn,5,0);
				HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
    }
}



void CAN_DrvSetRxQueue(QueueHandle_t queue)
{
    g_can_rx_queue = queue;
}

uint16_t CAN_DrvGetRxDropCount(void)
{
    return g_can_rx_drop_count;
}


uint8_t CAN_DrvIsReady(void)
{
	
	 return can_ready;
}

uint8_t CAN_DrvSendStd(uint16_t std_id, const uint8_t *data, uint8_t dlc)
{
    CAN_TxHeaderTypeDef tx_header;
    uint32_t tx_mailbox;
    uint32_t tick_start;

    if (can_ready == 0U)
    {
        return 1;
    }

    if (data == 0)
    {
        return 2;
    }

    if (std_id > 0x7FFU)
    {
        return 3;
    }

    if (dlc > 8U)
    {
        return 4;
    }

    /*
     * 构造 CAN 标准数据帧头
     */
    tx_header.StdId = std_id;              /* 11位标准帧 ID */
    tx_header.ExtId = 0U;                  /* 不使用扩展帧 ID */
    tx_header.IDE = CAN_ID_STD;            /* 标准帧 */
    tx_header.RTR = CAN_RTR_DATA;          /* 数据帧 */
    tx_header.DLC = dlc;                   /* 数据长度：0~8 */
    tx_header.TransmitGlobalTime = DISABLE;

    /*
     * 等待至少有一个发送邮箱空闲。
     * 避免连续发送多帧时，邮箱满导致发送失败。
     */
    tick_start = HAL_GetTick();

    while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0U)
    {
        if ((HAL_GetTick() - tick_start) > 10U)
        {
            return 5;
        }
    }

    /*
     * 把 CAN 帧加入发送邮箱。
     * 加入成功后，CAN 外设会自动等待总线空闲并发送。
     */
    if (HAL_CAN_AddTxMessage(&hcan,
                             &tx_header,
                             (uint8_t *)data,
                             &tx_mailbox) != HAL_OK)
    {
        return 6;
    }

    return 0;
}

void CAN_DrvRxIrqHandler(void)
{
    HAL_CAN_IRQHandler(&hcan);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan_arg)
{
    CAN_RxHeaderTypeDef rx_header;
    CAN_DrvRxFrame_t frame;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if ((hcan_arg == NULL) || (hcan_arg->Instance != CAN1))
    {
        return;
    }

    /*
     * 一次回调里尽量把 FIFO0 中已有帧读完，
     * 避免连续收到多帧时只处理一帧。
     */
    while (HAL_CAN_GetRxFifoFillLevel(hcan_arg, CAN_RX_FIFO0) > 0U)
    {
        frame.std_id = 0U;
        frame.dlc = 0U;
        frame.data[0] = 0U;
        frame.data[1] = 0U;
        frame.data[2] = 0U;
        frame.data[3] = 0U;
        frame.data[4] = 0U;
        frame.data[5] = 0U;
        frame.data[6] = 0U;
        frame.data[7] = 0U;

        if (HAL_CAN_GetRxMessage(hcan_arg, CAN_RX_FIFO0, &rx_header, frame.data) != HAL_OK)
        {
            break;
        }

        /*
         * V1 只接收标准数据帧。
         * 扩展帧 / 远程帧直接丢弃。
         */
        if ((rx_header.IDE != CAN_ID_STD) || (rx_header.RTR != CAN_RTR_DATA))
        {
            continue;
        }

        if (rx_header.DLC > 8U)
        {
            continue;
        }

        frame.std_id = (uint16_t)rx_header.StdId;
        frame.dlc = (uint8_t)rx_header.DLC;

        if (g_can_rx_queue != NULL)
        {
            if (xQueueSendFromISR(g_can_rx_queue,
                                  &frame,
                                  &xHigherPriorityTaskWoken) != pdTRUE)
            {
                if (g_can_rx_drop_count < 0xFFFFU)
                {
                    g_can_rx_drop_count++;
                }
            }
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
