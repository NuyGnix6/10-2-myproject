#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "APPData.h"
#include "UartTask.h"
#include "uart.h"
#include "ESP.h"
#include "stdio.h"
#include "string.h"

#define AT_Line_MAX 128U

#define AT_OK 0U
#define AT_TIMEOUT 2U

static uint8_t AT_Line[AT_Line_MAX];
static uint16_t AT_Len = 0U;

static void     AT_ClearLine(void);
static uint8_t  AT_GetLine(void);
static uint8_t  AT_Response(const char *cmd, const char *resp, uint32_t timeout);
static uint8_t  ESP_WaitPrompt(uint32_t timeout);
static uint16_t MQTT_Publish(uint8_t *buf, const char *topic, const char *payload);
static uint8_t  MQTT_SendPublish(const char *topic, const char *payload);
// static void     ESP_DumpHex(uint32_t timeout);



static const uint8_t MqttConn[] =
{
    0x10, 0x17,                                     /* CONNECT, 剩余长度 23 */
    0x00, 0x04, 'M',  'Q',  'T',  'T',              /* 协议名 "MQTT" */
    0x04,                                           /* 协议级别 3.1.1 */
    0x02,                                           /* Clean Session */
    0x00, 0x3C,                                     /* Keep Alive 60s */
    0x00, 0x0B, 'w',  'w',  '_',  'e',  'n',  'v',
    '_',  '7',  'f',  '3',  'a'                     /* ClientID "ww_env_7f3a" */
};

static uint8_t  MqttBuf[128];
static uint8_t  MqttPacketId = 1U;          /* 每条 PUBLISH 递增 */


// static void ESP_DumpHex(uint32_t timeout)
// {
//     static const char HexTab[] = "0123456789ABCDEF";
//     uint32_t start;
//     uint8_t byte;
//     char out[4];

//     start = HAL_GetTick();
//     while((HAL_GetTick() - start) < timeout)
//     {
//         while(ESP_GetByte(&byte) != 0)
//         {
//             out[0] = HexTab[byte >> 4];
//             out[1] = HexTab[byte & 0x0F];
//             out[2] = ' ';
//             out[3] = '\0';
//             UART_SendString(out);
//         }
//         vTaskDelay(pdMS_TO_TICKS(5U));
//     }
//     UART_SendString("\r\n");
// }

static uint8_t MQTT_SendPublish(const char *topic, const char *payload)
{
    char cmd[32];
    uint16_t len;

    MqttPacketId++;
    if(MqttPacketId == 0U)
    {
        MqttPacketId = 1U;
    }

    len = MQTT_Publish(MqttBuf, topic, payload);
    sprintf(cmd, "AT+CIPSEND=%u\r\n", (unsigned)len);

    AT_ClearLine();
    ESP_SendString(cmd);

    if(ESP_WaitPrompt(2000U) == 0U)
    {
        return 0U;
    }

    ESP_SendBuf(MqttBuf, len);
    return 1U;
}



static uint8_t ESP_WaitPrompt(uint32_t timeout)
{
    uint32_t start;
    uint8_t byte;

    start = HAL_GetTick();
    while((HAL_GetTick() - start) < timeout)
    {
        while(ESP_GetByte(&byte) != 0)
        {
            if(byte == '>')
            {
                return 1U;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5U));
    }
    return 0U;
}



static uint16_t MQTT_Publish(uint8_t *buf, const char *topic, const char *payload)
{
    uint16_t tlen  = (uint16_t)strlen(topic);
    uint16_t plen  = (uint16_t)strlen(payload);
    uint16_t rem   = (uint16_t)(2U + tlen + 2U + plen);
    uint16_t pos   = 0U;

    buf[pos++] = 0x32;                      /* PUBLISH + QoS1 */
    buf[pos++] = (uint8_t)rem;              /* 剩余长度，<128 用单字节 */
    buf[pos++] = (uint8_t)(tlen >> 8);      /* topic 长度高字节 */
    buf[pos++] = (uint8_t)(tlen & 0xFFU);   /* topic 长度低字节 */
    memcpy(&buf[pos], topic, tlen);
    pos = (uint16_t)(pos + tlen);
    buf[pos++] = 0x00;                      /* Packet ID */
    buf[pos++] = MqttPacketId;
    memcpy(&buf[pos], payload, plen);
    pos = (uint16_t)(pos + plen);

    return pos;
}


// static void PrintStack(void)
// {
//     static TaskStatus_t st[12];
//     UBaseType_t n;
//     UBaseType_t i;
//     char buf[32];

//     n = uxTaskGetSystemState(st, 12U, NULL);

//     for(i = 0U; i < n; i++)
//     {
//         sprintf(buf, "%-10s %3u\r\n", st[i].pcTaskName,
//                 (unsigned)st[i].usStackHighWaterMark);
//         UART_SendString(buf);
//     }
//     UART_SendString("-----------\r\n");
// }


static uint8_t ESP_Connect(void)
{
    char cmd[32];
    AT_ClearLine();
    ESP_SendString("AT+RST\r\n");
    vTaskDelay(pdMS_TO_TICKS(2000U));
    AT_ClearLine();
  
    if(AT_Response("AT\r\n", "OK", 300U) != AT_OK)  
    {
        UART_SendString("ESP no response\r\n");
        return 0U;
    }


   
    if(AT_Response("AT+CWMODE=1\r\n", "OK", 2000U) != AT_OK)
    {
        UART_SendString("CWMODE fail\r\n");
        return 0U;
    }

    if(AT_Response("AT+CWJAP=\"www\",\"www123456\"\r\n", "OK", 20000U) != AT_OK)
    {
        UART_SendString("CWJAP fail\r\n");
        return 0U;
    }
    //UART_SendString("WiFi OK\r\n");

    if(AT_Response("AT+CIPSTART=\"TCP\",\"broker.emqx.io\",1883\r\n", "OK", 10000U) != AT_OK)
    {
        UART_SendString("TCP fail\r\n");
        return 0U;
    }
    

    /* 阶段 5：MQTT CONNECT */
    sprintf(cmd, "AT+CIPSEND=%u\r\n", (unsigned)sizeof(MqttConn));
    AT_ClearLine();
    ESP_SendString(cmd);

    if(ESP_WaitPrompt(2000U) == 0U)
    {
        UART_SendString("CONNECT no prompt\r\n");
        return 0U;
    }


    ESP_SendBuf(MqttConn, (uint16_t)sizeof(MqttConn));
    vTaskDelay(pdMS_TO_TICKS(500U));
    return 1U;
    
}


static void AT_ClearLine(void)
{
    uint8_t byte;
    AT_Len = 0U;
    while(ESP_GetByte(&byte) != 0) ;
}

static uint8_t AT_GetLine(void)
{
    uint8_t byte;
    while( ESP_GetByte(&byte) != 0)
    {
        if(byte == '\r')
        {
            continue; // 跳过\r
        }
        if(byte == '\n')
        {
            AT_Line[AT_Len] = '\0';
            AT_Len = 0U; // 清空缓存
            return 1U; // 成功
        }

        if(AT_Len < AT_Line_MAX - 1)
        {
            AT_Line[AT_Len++] = byte;
        }

    }
    return 0U; // 失败
}

// cmd 是发送的命令例如 AT/r/n 
// resp 是响应的字符串例如 OK/r/n
// timeout 是超时时间，单位毫秒
static uint8_t AT_Response(const char *cmd, const char *resp, uint32_t timeout)
{
    uint32_t start;

    AT_ClearLine();
    ESP_SendString(cmd);

    start = HAL_GetTick();
    while((HAL_GetTick() - start) < timeout)
    {
        if(AT_GetLine() == 1U)
        {

            if(strstr((char *)AT_Line, resp) != NULL)
            {
                return AT_OK;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5U));
    }
    return AT_TIMEOUT;
}



void UartTask(void *pvParameters)
{
    char cmd[32];
    DHT11Data_t d;
    LightData_t l;
    char payload[64];
    uint8_t fail_count;
    
    for(;;)
    {
        while(ESP_Connect() == 0U)
        {
            UART_SendString("ESP_Connect fail\r\n");
            vTaskDelay(pdMS_TO_TICKS(5000U));
        }
        UART_SendString("ESP_Connect success\r\n");
        fail_count = 0U;

        for(;;)
        {
             if((xQueuePeek(DHT11Queue, &d, 0) == pdPASS) &&
               (xQueuePeek(LightQueue, &l, 0) == pdPASS) &&
               ((xTaskGetTickCount() - d.tick) <= DATA_TIMEOUT_TICK) &&
               ((xTaskGetTickCount() - l.tick) <= DATA_TIMEOUT_TICK))
            {
                sprintf(payload, "{\"t\":%u.%u,\"h\":%u.%u,\"l\":%u,\"tmax\":%u,\"hmax\":%u,\"lmin\":%u}",
                        d.temp_int, d.temp_dec,
                        d.humi_int, d.humi_dec,
                        l.percent,
                        (unsigned)Temp_HLimit,
                        (unsigned)Humi_HLimit,
                        (unsigned)Flame_HLimit);


                if(MQTT_SendPublish("env/data", payload) != 0U)
                {
                    UART_SendString(payload);
                    UART_SendString("\r\n");
                    fail_count = 0U;
                }
                else
                {
                    UART_SendString("PUBLISH fail\r\n");
                    fail_count++;
                }
            }
            if(fail_count >= 3U)
            {
                UART_SendString("Connecting\r\n");
                break;
            }
            //  PrintStack();

            AT_ClearLine();                    /* 丢掉 PUBACK，防止缓冲堆满 */
            vTaskDelay(pdMS_TO_TICKS(5000U));
        }

    }

}


