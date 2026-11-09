#include "Key.h"
#include "main.h"
#include "bsp.h"

static Key_t Key[3] = {
    {Key_Set_GPIO_Port, Key_Set_Pin},
    {Key_Down_GPIO_Port, Key_Down_Pin},
    {Key_Up_GPIO_Port, Key_Up_Pin},
};

void Key_Init(void)
{
    for (uint8_t i = 0; i < 3; i++)
        Key[i].Previous = HAL_GPIO_ReadPin(Key[i].GPIOx, Key[i].GPIO_Pin);
}

uint8_t Key_Scan(void)
{
    for (uint8_t i = 0; i < 3; i++)
    {
        GPIO_PinState Current = HAL_GPIO_ReadPin(Key[i].GPIOx, Key[i].GPIO_Pin);

        if (Current != Key[i].Previous)
        {
            //按键按下
            BSP_DelayMs(20); //消抖
            Current = HAL_GPIO_ReadPin(Key[i].GPIOx, Key[i].GPIO_Pin);
            if (Current != Key[i].Previous )
            {
                Key[i].Previous = Current;
                //按键按下
                if(Current == GPIO_PIN_RESET)
                    return i;
            }
            

        }
    }
    return KEY_NONE;
}


