#ifndef __KEY_H__
#define __KEY_H__



#include "main.h"

#define KEY_SET     0U
#define KEY_DOWN    1U
#define KEY_UP      2U
#define KEY_NONE    0xFFU

typedef struct
{
    GPIO_TypeDef* GPIOx;
    uint16_t GPIO_Pin;
    
    GPIO_PinState Previous;
}Key_t;

void Key_Init(void);
uint8_t Key_Scan(void);


#endif
