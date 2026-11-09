#ifndef __APP_DATA_H
#define __APP_DATA_H

#include "FreeRTOS.h"
#include "event_groups.h"
#include "queue.h"
#include "main.h"
#include "MyFlash.h"

/* data_ready 的位定义 */
#define DATA_TEMP      0x01U
#define DATA_HUMI      0x02U
#define DATA_LIGHT     0x04U

/* alarm_flag 的位定义 */
#define ALARM_TEMP     0x01U
#define ALARM_HUMI     0x02U
#define ALARM_FLAME    0x04U
#define ALARM_SENSOR   0x08U

#define DATA_TIMEOUT_TICK pdMS_TO_TICKS(10000)

/* 传感器数据 */

typedef struct
{
    uint16_t raw, mv, percent;
    TickType_t tick;
} LightData_t;

typedef struct
{
    uint8_t temp_int, temp_dec;
    uint8_t humi_int, humi_dec;
    TickType_t tick;
} DHT11Data_t;

/* 报警阈值 */
#define Temp_HLimit   (Store_Data[1])
#define Humi_HLimit   (Store_Data[2])
#define Flame_HLimit  (Store_Data[3])

/* 界面与报警状态 */
extern uint8_t  page;
extern uint8_t  alarm_flag;
extern uint8_t  dht_fault;

extern QueueHandle_t LightQueue;
extern QueueHandle_t DHT11Queue;

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
#endif
