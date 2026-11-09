#include "Buzzer.h"

void Buzzer_Init(void)
{
    Buzzer_Off();
}

void Buzzer_On(void)
{
    HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_RESET);
}
void Buzzer_Off(void)
{
    HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_SET);
}

