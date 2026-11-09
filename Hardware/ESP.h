#ifndef __ESP_H__
#define __ESP_H__
#include "main.h"
extern UART_HandleTypeDef huart2;

void ESP_Init(void);
void ESP_SendString(const char *str);
void ESP_SendBuf(const uint8_t *buf, uint16_t len);
uint8_t ESP_GetByte(uint8_t *byte);
uint16_t ESP_RxCount(void);
void ESP_RxHandler(void);
void ESP_ErrorHandler(void);


#endif