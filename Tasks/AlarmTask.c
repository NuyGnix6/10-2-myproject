#include "FreeRTOS.h"
#include "task.h"
#include "AlarmTask.h"
#include "main.h"
#include "AppData.h"
#include "Buzzer.h"


void AlarmTask(void *pvParameters)
{
    TickType_t last_tick = xTaskGetTickCount();
    EventBits_t ready;
    LightData_t LightData;
    DHT11Data_t DHT11Data;
    uint8_t dht_ok;
    uint8_t light_ok;
    uint8_t flag;

    // 上一轮的报警状态，用来做回差。static 才能跨循环保持
    static uint8_t temp_alarm  = 0U;
    static uint8_t humi_alarm  = 0U;
    static uint8_t light_alarm = 0U;

    for(;;)
    {
        flag = 0U;

        dht_ok   = (uint8_t)(((xQueuePeek(DHT11Queue, &DHT11Data, 0) == pdPASS) &&
                              ((xTaskGetTickCount() - DHT11Data.tick) <= DATA_TIMEOUT_TICK)) ? 1U : 0U);
        light_ok = (uint8_t)(((xQueuePeek(LightQueue, &LightData, 0) == pdPASS) &&
                              ((xTaskGetTickCount() - LightData.tick) <= DATA_TIMEOUT_TICK)) ? 1U : 0U);

        if(dht_fault != 0U)
        {
            flag |= ALARM_SENSOR;               // 传感器坏了，温湿度判不了
        }
        else if(dht_ok != 0U)
        {
            // 温度：高于阈值才报警，低于阈值才解除，等于时保持
            if(DHT11Data.temp_int > Temp_HLimit)
            {
                temp_alarm = 1U;
            }
            else if(DHT11Data.temp_int < Temp_HLimit)
            {
                temp_alarm = 0U;
            }

            // 湿度同上
            if(DHT11Data.humi_int > Humi_HLimit)
            {
                humi_alarm = 1U;
            }
            else if(DHT11Data.humi_int < Humi_HLimit)
            {
                humi_alarm = 0U;
            }

            if(temp_alarm != 0U)
            {
                flag |= ALARM_TEMP;
            }
            if(humi_alarm != 0U)
            {
                flag |= ALARM_HUMI;
            }
        }

        if(light_ok != 0U)
        {
            // 火焰是上限报警，和温湿度同方向：高于阈值才报，低于才解
            if(LightData.percent > Flame_HLimit)
            {
                light_alarm = 1U;
            }
            else if(LightData.percent < Flame_HLimit)
            {
                light_alarm = 0U;
            }
        }

        if(light_alarm != 0U)
        {
            flag |= ALARM_FLAME;
        }

        alarm_flag = flag;                      // 一次写入，避免读到半截

        if(alarm_flag != 0U)
        {
            Buzzer_On();
        }
        else
        {
            Buzzer_Off();
        }

        vTaskDelayUntil(&last_tick, pdMS_TO_TICKS(100));
    }
}
