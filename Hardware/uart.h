#ifndef __UART_H__
#define __UART_H__

#include "main.h"

#define UART_RX_BUF_SIZE 128U
extern UART_HandleTypeDef huart1;

void UART_Init(void);
void UART_SendByte(uint8_t byte);
void UART_SendString(const char *str);
void UART_SendBuf(const uint8_t *buf, uint16_t len);
uint16_t Uart_RxCount(void);
uint8_t UART_GetByte(uint8_t *byte);
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart);

#endif
