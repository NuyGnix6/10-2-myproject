#include "uart.h"
#include "ESP.h"
#include "string.h"

/* USART1 = 调试/命令口，USART2 = ESP8266。
   两个口各自一条环形缓冲，靠 Instance 分发 */

static uint8_t Rx_Byte;                    // 接收字节
static uint8_t Rx_BUF[UART_RX_BUF_SIZE];   // 接收缓冲区

static volatile uint16_t Rx_Head = 0U;     // 头指针，中断里推
static volatile uint16_t Rx_Tail = 0U;     // 尾指针，任务里拉

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    uint16_t next;

    if(huart->Instance == USART2)
    {
        ESP_RxHandler();        /* ESP 自己重新武装接收 */
        return;
    }

    if(huart->Instance == USART1)
    {
        next = (Rx_Head + 1U) % UART_RX_BUF_SIZE;
        if(next != Rx_Tail)
        {
            Rx_BUF[Rx_Head] = Rx_Byte;
            Rx_Head = next;
        }

        /* 重开接收失败（RxState 异常）时必须强制复位再开，
           否则接收链就此断掉，表现是"发好几次才有一次响应" */
            if(HAL_UART_Receive_IT(&huart1, &Rx_Byte, 1U) != HAL_OK)
        {
            huart1.RxState = HAL_UART_STATE_READY;
            HAL_UART_Receive_IT(&huart1, &Rx_Byte, 1U);
        }
    }
}

void UART_Init(void)
{
    Rx_Tail = 0U;
    Rx_Head = 0U;
    HAL_UART_Receive_IT(&huart1, &Rx_Byte, 1U);
}

void UART_SendByte(uint8_t byte)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&byte, 1U, 100U);
}

void UART_SendString(const char *str)
{
    uint16_t len = (uint16_t)strlen(str);

    if(len > 0U)
    {
        HAL_UART_Transmit(&huart1, (uint8_t *)str, len, 100U);
    }
}

void UART_SendBuf(const uint8_t *buf, uint16_t len)
{
    if(len > 0U)
    {
        HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, 100U);
    }
}

uint16_t Uart_RxCount(void)
{
    return (uint16_t)((Rx_Head + UART_RX_BUF_SIZE - Rx_Tail) % UART_RX_BUF_SIZE);
}

uint8_t UART_GetByte(uint8_t *byte)
{
    if(Rx_Head == Rx_Tail)
    {
        return 0U;
    }

    *byte = Rx_BUF[Rx_Tail];
    Rx_Tail = (Rx_Tail + 1U) % UART_RX_BUF_SIZE;
    return 1U;
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART2)
    {
        ESP_ErrorHandler();
        return;
    }

    if(huart->Instance != USART1)
    {
        return;
    }

    __HAL_UART_CLEAR_OREFLAG(huart);

    if(HAL_UART_Receive_IT(&huart1, &Rx_Byte, 1U) != HAL_OK)
    {
        huart1.RxState = HAL_UART_STATE_READY;
        HAL_UART_Receive_IT(&huart1, &Rx_Byte, 1U);
    }
}
