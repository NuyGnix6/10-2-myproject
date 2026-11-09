#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "AppData.h"
#include "Light.h"
#include "LightTask.h"
#include "event_groups.h"

void LightTask(void *pvParameters)
{
   TickType_t last_wake = xTaskGetTickCount();
   LightData_t LightData;
   for(;;)
   {
      Light_Get(&LightData.raw, &LightData.mv, &LightData.percent);
      LightData.tick = xTaskGetTickCount();
      xQueueOverwrite(LightQueue, &LightData);

      //xEventGroupSetBits(EventGroup, DATA_LIGHT);
      vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(500));
   }
}
