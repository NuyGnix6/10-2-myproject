#include "FreeRTOS.h"
#include "task.h"
#include "KeyTask.h"
#include "Key.h"
#include "AppData.h"


void KeyHandle(uint8_t key_index)
{
    if(key_index == KEY_SET)
    {
        page++;
        if(page > 2U)
        {
            page = 0U;
            MyFlash_Save();     /* 转一圈回到 T，顺手存盘 */
        }
    }
    else if(key_index == KEY_UP)
    {
        if(page == 0U && Temp_HLimit  < 50U)    Temp_HLimit++;
        if(page == 1U && Humi_HLimit  < 95U)    Humi_HLimit++;
        if(page == 2U && Flame_HLimit < 99U)    Flame_HLimit++;
    }
    else if(key_index == KEY_DOWN)
    {
        if(page == 0U && Temp_HLimit  > 0U)     Temp_HLimit--;
        if(page == 1U && Humi_HLimit  > 20U)    Humi_HLimit--;
        if(page == 2U && Flame_HLimit > 0U)     Flame_HLimit--;
    }
}

void KeyTask(void *pvParameters)
{
    TickType_t last_tick = xTaskGetTickCount();
    uint8_t key_index = KEY_NONE;
    for(;;)
    {
        key_index = Key_Scan();
        KeyHandle(key_index);
          
        vTaskDelayUntil(&last_tick, pdMS_TO_TICKS(20));
    }


}