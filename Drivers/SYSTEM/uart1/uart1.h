#ifndef __USART_H__
#define __USART_H__

#include "sys.h"

#define UART1_RX_BUF_SIZE 128
#define UART1_TX_BUF_SIZE 64
#define UART_EOK 	  	0
#define UART_ERROR     1
#define UART_ETIMEOUT  2
#define UART_ENAL  	3

void uart1_init(uint32_t BAUDRATE);
void uart1_receiv_test(void);
void uart1_print_hex(uint8_t *buf, uint16_t len);

#endif


