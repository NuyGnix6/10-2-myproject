#include "FreeRTOS.h"
#include "task.h"
#include "DisplayTask.h"
#include "LCD.h"
#include "AppData.h"
#include "Light.h"
#include "DHT11.h"
#include <string.h>

/* 240x320，字 8x16。行距 40px，一行最多 28 个字符(从 x=8 起) */

#define ROW_TITLE   8U
#define ROW_TEMP    48U
#define ROW_HUMI    88U
#define ROW_FLAME   128U
#define ROW_STATE   168U
#define ROW_THR     208U
#define ROW_HINT    288U

#define COL_VAL     64U

/* 画一行空格清残影。只在长度会变的地方用 */
static void Clear_Row(uint16_t y)
{
    LCD_ShowString(8, y, "                            ", GRAY, BLACK);
}

static void Show_DHT11(void)
{
    DHT11Data_t DHT11Data;

    if(xQueuePeek(DHT11Queue, &DHT11Data, 0) != pdPASS)
    {
        return;
    }

    if((dht_fault != 0U) || ((xTaskGetTickCount() - DHT11Data.tick) > DATA_TIMEOUT_TICK))
    {
        LCD_ShowString(COL_VAL, ROW_TEMP, "--.-", RED, BLACK);
        LCD_ShowString(COL_VAL, ROW_HUMI, "--.-", RED, BLACK);
        return;
    }

    LCD_ShowNum(COL_VAL, ROW_TEMP, DHT11Data.temp_int, 2, WHITE, BLACK);
    LCD_ShowNum(COL_VAL + 24U, ROW_TEMP, DHT11Data.temp_dec, 2, WHITE, BLACK);
    LCD_ShowNum(COL_VAL, ROW_HUMI, DHT11Data.humi_int, 2, WHITE, BLACK);
    LCD_ShowNum(COL_VAL + 24U, ROW_HUMI, DHT11Data.humi_dec, 2, WHITE, BLACK);
}

static void Show_Light(void)
{
    LightData_t LightData;

    if(xQueuePeek(LightQueue, &LightData, 0) != pdPASS)
    {
        return;
    }

    if((xTaskGetTickCount() - LightData.tick) > DATA_TIMEOUT_TICK)
    {
        LCD_ShowString(COL_VAL, ROW_FLAME, "---", RED, BLACK);
        return;
    }

    LCD_ShowNum(COL_VAL, ROW_FLAME, LightData.percent, 3, WHITE, BLACK);
}

/* 状态行缓存：内容没变就不重画。
   原来每 200ms 都"擦整行再画字"，擦和画之间有几毫秒空白，
   看起来就是文字在闪。改成只在内容真的变化时才动屏幕 */
static char State_Last[20];

/* 状态行：火焰强度描述 / 传感器故障 */
static void Show_State(void)
{
    char now[20];
    LightData_t LightData;

    if(dht_fault != 0U)
    {
        strcpy(now, "DHT11 Fault!");
    }
    else
    {
        if(xQueuePeek(LightQueue, &LightData, 0) != pdPASS)
        {
            return;
        }

        if((xTaskGetTickCount() - LightData.tick) > DATA_TIMEOUT_TICK)
        {
            strcpy(now, "FLAME TIMEOUT");
        }
        else
        {
            strcpy(now, Light_GetPercent((uint8_t)LightData.percent));
        }
    }

    if(strcmp(now, State_Last) != 0)
    {
        strcpy(State_Last, now);
        Clear_Row(ROW_STATE);
        LCD_ShowString(8, ROW_STATE, now, YELLOW, BLACK);
    }
}

/* 三个阈值固定显示，当前选中的那个用黄色 */
static void Show_Thr(void)
{
    LCD_ShowString(8, ROW_THR, "THR", GRAY, BLACK);

    LCD_ShowString(56, ROW_THR, "T:", GRAY, BLACK);
    LCD_ShowNum(72, ROW_THR, Temp_HLimit, 2,
                (page == 0U) ? YELLOW : WHITE, BLACK);

    LCD_ShowString(112, ROW_THR, "H:", GRAY, BLACK);
    LCD_ShowNum(128, ROW_THR, Humi_HLimit, 2,
                (page == 1U) ? YELLOW : WHITE, BLACK);

    LCD_ShowString(168, ROW_THR, "F:", GRAY, BLACK);
    LCD_ShowNum(184, ROW_THR, Flame_HLimit, 2,
                (page == 2U) ? YELLOW : WHITE, BLACK);
}

static void Show_Normal(void)
{
    LCD_Clear(BLACK);

    LCD_ShowString(8, ROW_TITLE, "ENV MONITOR  F407", GRAY, BLACK);

    LCD_ShowString(8, ROW_TEMP, "TEMP", GRAY, BLACK);
    LCD_ShowString(80, ROW_TEMP, ".", WHITE, BLACK);
    LCD_ShowString(112, ROW_TEMP, "C", WHITE, BLACK);

    LCD_ShowString(8, ROW_HUMI, "HUMI", GRAY, BLACK);
    LCD_ShowString(80, ROW_HUMI, ".", WHITE, BLACK);
    LCD_ShowString(112, ROW_HUMI, "%RH", WHITE, BLACK);

    LCD_ShowString(8, ROW_FLAME, "FLAME", GRAY, BLACK);
    LCD_ShowString(88, ROW_FLAME, "%", WHITE, BLACK);

    LCD_ShowString(8, ROW_HINT, "SET:SELECT  UP/DOWN:ADJ", GRAY, BLACK);

    State_Last[0] = '\0';       /* 整屏刚清过，强制重画状态行 */

    Show_DHT11();
    Show_Light();
    Show_State();
    Show_Thr();
}

/* 报警页：位置和正常页对齐，覆盖式切换 */
static void Show_Alarm(void)
{
    LightData_t LightData;
    DHT11Data_t DHT11Data;

    if(xQueuePeek(LightQueue, &LightData, 0) != pdPASS)
    {
        LightData.percent = 0U;
    }
    if(xQueuePeek(DHT11Queue, &DHT11Data, 0) != pdPASS)
    {
        DHT11Data.temp_int  = 0U;
        DHT11Data.temp_dec  = 0U;
        DHT11Data.humi_int  = 0U;
        DHT11Data.humi_dec  = 0U;
    }

    if((alarm_flag & ALARM_SENSOR) != 0U)
    {
        LCD_ShowString(8, ROW_TITLE, "!!SENSOR ERR!!", RED, BLACK);
        LCD_ShowString(8, ROW_TEMP,  "DHT11 NO RESP", RED, BLACK);
        LCD_ShowString(8, ROW_HUMI,  "CHECK WIRING", RED, BLACK);
    }
    else if((alarm_flag & ALARM_TEMP) != 0U)
    {
        LCD_ShowString(8, ROW_TITLE, "!! TEMP HIGH !!", RED, BLACK);
        LCD_ShowString(8, ROW_TEMP,  "NOW:", GRAY, BLACK);
        LCD_ShowNum(56, ROW_TEMP, DHT11Data.temp_int, 2, WHITE, BLACK);
        LCD_ShowString(72, ROW_TEMP, ".", WHITE, BLACK);
        LCD_ShowNum(80, ROW_TEMP, DHT11Data.temp_dec, 1, WHITE, BLACK);
        LCD_ShowString(96, ROW_TEMP, " C", WHITE, BLACK);
        LCD_ShowString(8, ROW_HUMI, "LIM:", GRAY, BLACK);
        LCD_ShowNum(56, ROW_HUMI, Temp_HLimit, 2, YELLOW, BLACK);
        LCD_ShowString(72, ROW_HUMI, " C", YELLOW, BLACK);
    }
    else if((alarm_flag & ALARM_HUMI) != 0U)
    {
        LCD_ShowString(8, ROW_TITLE, "!! HUMI HIGH !!", RED, BLACK);
        LCD_ShowString(8, ROW_TEMP,  "NOW:", GRAY, BLACK);
        LCD_ShowNum(56, ROW_TEMP, DHT11Data.humi_int, 2, WHITE, BLACK);
        LCD_ShowString(72, ROW_TEMP, " %", WHITE, BLACK);
        LCD_ShowString(8, ROW_HUMI, "LIM:", GRAY, BLACK);
        LCD_ShowNum(56, ROW_HUMI, Humi_HLimit, 2, YELLOW, BLACK);
        LCD_ShowString(72, ROW_HUMI, " %", YELLOW, BLACK);
    }
    else if((alarm_flag & ALARM_FLAME) != 0U)
    {
        LCD_ShowString(8, ROW_TITLE, "!! FLAME !!", RED, BLACK);
        LCD_ShowString(8, ROW_TEMP,  "NOW:", GRAY, BLACK);
        LCD_ShowNum(56, ROW_TEMP, LightData.percent, 3, WHITE, BLACK);
        LCD_ShowString(80, ROW_TEMP, " %", WHITE, BLACK);
        LCD_ShowString(8, ROW_HUMI, "LIM:", GRAY, BLACK);
        LCD_ShowNum(56, ROW_HUMI, Flame_HLimit, 3, YELLOW, BLACK);
        LCD_ShowString(80, ROW_HUMI, " %", YELLOW, BLACK);
    }
    else
    {
        LCD_ShowString(8, ROW_TITLE, "!! UNKNOWN !!", RED, BLACK);
        LCD_ShowString(8, ROW_TEMP,  "FLAG=", GRAY, BLACK);
        LCD_ShowNum(56, ROW_TEMP, alarm_flag, 3, WHITE, BLACK);
        LCD_ShowString(8, ROW_HUMI, "REPORT TO AUTHOR", GRAY, BLACK);
    }

    /* 清掉正常页下面四行的残留 */
    Clear_Row(ROW_FLAME);
    Clear_Row(ROW_STATE);
    Clear_Row(ROW_THR);
    Clear_Row(ROW_HINT);
}

void DisplayTask(void *pvParameters)
{
    TickType_t last_tick = xTaskGetTickCount();
    uint8_t last_alarm = 0xFFU;      /* 初值给个不可能的值，保证第一轮一定画框架 */

    for(;;)
    {
        uint8_t now_alarm = (alarm_flag != 0U) ? 1U : 0U;

        if(now_alarm != 0U)
        {
            Show_Alarm();
        }
        else if(last_alarm != 0U)    /* 刚从报警页回来，重画框架 */
        {
            Show_Normal();
        }
        else
        {
            Show_DHT11();
            Show_Light();
            Show_State();
            Show_Thr();
        }

        last_alarm = now_alarm;

        vTaskDelayUntil(&last_tick, pdMS_TO_TICKS(200));
    }
}
