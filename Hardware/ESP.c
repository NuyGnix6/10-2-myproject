#include "ESP.h"
#include "main.h"
#include "string.h"
#include "uart.h"


static uint8_t Esp_Byte;
static uint8_t Esp_BUF[UART_RX_BUF_SIZE];
static volatile uint16_t Esp_Head = 0U;
static volatile uint16_t Esp_Tail = 0U;

void ESP_Init(void)
{
    Esp_Head = 0U;
    Esp_Tail = 0U;
    HAL_UART_Receive_IT(&huart2, &Esp_Byte, 1U);
}

void ESP_SendString(const char *str)
{
    uint16_t len = (uint16_t)strlen(str);
    if(len > 0U)
    {
        HAL_UART_Transmit(&huart2, (uint8_t *)str, len, 100U);
    }
}

void ESP_SendBuf(const uint8_t *buf, uint16_t len)
{
    if(len > 0U)
    {
        HAL_UART_Transmit(&huart2, (uint8_t *)buf, len, 100U);
    }
}

uint8_t ESP_GetByte(uint8_t *byte)
{
    if(Esp_Head == Esp_Tail)
    {
        return 0U;
    }
    *byte = Esp_BUF[Esp_Tail];
    Esp_Tail = (Esp_Tail + 1U) % UART_RX_BUF_SIZE;
    return 1U;
}

uint16_t ESP_RxCount(void)
{
    return (uint16_t)((Esp_Head + UART_RX_BUF_SIZE - Esp_Tail) % UART_RX_BUF_SIZE);
}

void ESP_RxHandler(void)
{
    uint16_t next;

    next = (Esp_Head + 1U) % UART_RX_BUF_SIZE;
    if(next != Esp_Tail)
    {
        Esp_BUF[Esp_Head] = Esp_Byte;
        Esp_Head = next;
    }

    HAL_UART_Receive_IT(&huart2, &Esp_Byte, 1U);
}

void ESP_ErrorHandler(void)
{
    __HAL_UART_CLEAR_OREFLAG(&huart2);
    huart2.RxState = HAL_UART_STATE_READY;
    HAL_UART_Receive_IT(&huart2, &Esp_Byte, 1U);
}
