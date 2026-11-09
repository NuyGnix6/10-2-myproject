#ifndef __LIGHT_H__
#define __LIGHT_H__
#include "main.h"
#define LIGHT_VREF_MV   3300U    /* 参考电压 3.3V */
#define LIGHT_ADC_MAX   4095U    /* 12 位满量程 */

void Light_Init(void);
void Light_Get(uint16_t *raw,uint16_t *mv,uint16_t *percent);
const char *Light_GetPercent (uint8_t percent);

#endif
