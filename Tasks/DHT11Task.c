#include "FreeRTOS.h"
#include "task.h"
#include "DHT11Task.h"
#include "DHT11.h"
#include "AppData.h"
#include "main.h"
#include "event_groups.h"
#include "UART.h"


uint8_t dht_fault = 0;
void DHT11Task(void *pvParameters)
{
    TickType_t last_wake = xTaskGetTickCount();
    DHT11Data_t DHT11Data;
    uint8_t ret;
    uint8_t dht_fault_cnt = 0;
    for(;;)
    {
        DHT11_Request();
        taskENTER_CRITICAL();
        ret = DHT11_GetData(&DHT11Data.temp_int, &DHT11Data.humi_int, &DHT11Data.temp_dec, &DHT11Data.humi_dec);
        taskEXIT_CRITICAL();

        if(ret == DHT11_OK)
        {
            DHT11Data.tick = xTaskGetTickCount();
            xQueueOverwrite(DHT11Queue, &DHT11Data);
            dht_fault = 0;
            dht_fault_cnt = 0;
        }
        else
        {
            if(dht_fault_cnt < 5)
            {
                dht_fault_cnt++;
            }
            if(dht_fault_cnt >= 3 && dht_fault == 0)
            {
                dht_fault = 1;
                UART_SendString("DHT11 Fault!\r\n");
            }

        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(2000));
    }

}
