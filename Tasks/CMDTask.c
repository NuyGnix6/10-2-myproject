#include "FreeRTOS.h"
#include "task.h"
#include "CMDTask.h"
#include "AppData.h"
#include "uart.h"
#include "MyFlash.h"

static uint8_t CMDBuffer[8];
static uint8_t CMDBufferLen ;

static void StrTranslateNum(void)
{
    uint16_t num = 0;

    if(CMDBufferLen == 0)
    {
        return;
    }

    for(uint8_t i = 1; i < CMDBufferLen; i++)
    {
        if(CMDBuffer[i] >= '0' && CMDBuffer[i] <= '9')
        {
            num = num * 10 + (CMDBuffer[i] - '0');
        }
        else
        {
            return;
        }
    }
    
    if(CMDBuffer[0] == 'T')
    {
        if( num <= 50)
        {
            Temp_HLimit = num;
            UART_SendString("Temp_HLimit Set OK\r\n");
        }
        else
        {
            UART_SendString("Temp_HLimit Set Error\r\n");
        }

    }
    else if(CMDBuffer[0] == 'H')
    {
        if( num <= 90 && num >= 20)
        {
            Humi_HLimit = num;
            UART_SendString("Humi_HLimit Set OK\r\n");
        }
        else
        {
            UART_SendString("Humi_HLimit Set Error\r\n");
        }
    }
    else if(CMDBuffer[0] == 'F')
    {
        if( num <= 90)
        {
            Flame_HLimit = num;
            UART_SendString("Flame_HLimit Set OK\r\n");
        }
        else
        {
            UART_SendString("Flame_HLimit Set Error\r\n");
        }
    }
    else if(CMDBuffer[0] == 'S')
    {
        MyFlash_Save();
        UART_SendString("Save OK\r\n");
    }
    else if(CMDBuffer[0] == '?')
    {
        char buf[128];
        sprintf(buf, "Temp_HLimit: %d, Humi_HLimit: %d, Flame_HLimit: %d\r\n", Temp_HLimit, Humi_HLimit, Flame_HLimit);
        UART_SendString(buf);
    }
    else
    {
        /* 把首字节打出来，若还有 Unknown 能一眼看出收到了什么 */
        static const char HEX[] = "0123456789ABCDEF";
        UART_SendString("Unknown [");
        UART_SendByte((uint8_t)HEX[(CMDBuffer[0] >> 4) & 0x0FU]);
        UART_SendByte((uint8_t)HEX[CMDBuffer[0] & 0x0FU]);
        UART_SendString("]\r\n");
    }

}


void CMDTask(void *pvParameters)
{
    uint8_t byte;
    uint8_t overflow;

    overflow = 0U;

    for(;;)
    {
        /* 一次把环形缓冲取空，别一个 tick 只吃一个字节 */
        while(UART_GetByte(&byte)!=0)
        {
            if(byte == '\n' || byte == '\r')
            {
                if(overflow == 0U)
                {
                    StrTranslateNum();
                }
                overflow = 0U;      /* 换行了，超长状态解除 */
                CMDBufferLen = 0;
            }
            else //if(byte >= 0x20U && byte <= 0x7EU)
            {
                /* 只收可见 ASCII，0x00、帧错误之类的噪声直接丢掉 */
                if(overflow != 0U)
                {
                    /* 这条已经超长，整条丢到下一个换行为止，
                       不能清零后接着存，否则后半截会被当成新命令 */
                }
                else if(CMDBufferLen < 8U)
                {
                    CMDBuffer[CMDBufferLen++] = byte;
                }
                else
                {
                    overflow = 1U;
                    CMDBufferLen = 0U;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
}
