#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "LVGLTask.h"
#include "lvgl.h"
#include "bsp_ili9341.h"
#include "uart.h"
#include "myui_rewriter.h"
#include "appdata.h"
/* 在各自移植层里，没有单独的头文件 */
void    lv_port_disp_init(void);
uint8_t lv_port_indev_init(void);

/* LVGL 不是线程安全的：所有 lv_* 调用都必须在这个任务里发生。
   业务任务要更新界面，只能通过共享变量/队列传值，由这里统一刷 */
void LVGLTask(void *pvParameters)
{
    (void)pvParameters;

    lv_init();

    /* LVGL 9 的 tick 不再用 lv_tick_inc，直接给一个取毫秒的函数 */
    lv_tick_set_cb(HAL_GetTick);

    LCD_Init();                 /* 屏复位 + ILI9341 初始化序列，必须在注册显示前 */
    lv_port_disp_init();

    if(lv_port_indev_init() != 0U)
    {
        UART_SendString("Touch fail\r\n");      /* I2C 读不到芯片，查 PC2/PC3/PC0 */
    }
    else
    {
        UART_SendString("Touch OK\r\n");
    }

    myui_rewriter_init();

    TickType_t   last_ui = xTaskGetTickCount();
    DHT11Data_t  d;
    LightData_t  l;

    for(;;)
    {
        lv_timer_handler();

        /* 200ms 刷一次数据，比 LVGL 自己的 5ms 节奏慢很多，够用又不占 CPU */
        if((xTaskGetTickCount() - last_ui) >= pdMS_TO_TICKS(200))
        {
            last_ui = xTaskGetTickCount();

            if(dht_fault != 0U)
            {
                /* 传真实的 alarm_flag，不是写死 ALARM_SENSOR：
                   拔掉 DHT11 时如果同时有明火，报警页要优先显示明火 */
                myui_update(0U, 0U, 0U, 0U, 0U, alarm_flag);
            }
            else if((xQueuePeek(DHT11Queue, &d, 0) == pdPASS) &&
                    (xQueuePeek(LightQueue, &l, 0) == pdPASS) &&
                    ((xTaskGetTickCount() - d.tick) <= DATA_TIMEOUT_TICK) &&
                    ((xTaskGetTickCount() - l.tick) <= DATA_TIMEOUT_TICK))
            {
                myui_update(d.temp_int, d.temp_dec,
                            d.humi_int, d.humi_dec,
                            (uint8_t)l.percent,
                            alarm_flag);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }

}
