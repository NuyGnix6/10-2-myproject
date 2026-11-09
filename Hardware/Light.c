#include "Light.h"
#include "bsp.h"


extern ADC_HandleTypeDef hadc1;

/* F407 的校准位是 ADC_CR2 的第 14 位，这版 CMSIS 头文件没给宏 */
#define ADC_CAL_BIT    (1UL << 14)

void Light_Init(void)
{
    /* F4 这版 HAL 没有 HAL_ADCEx_Calibration_Start，直接写 CR2 的 CAL 位。
       校准由硬件完成，完成后 CAL 位自动清 0 */
    if((ADC1->CR2 & ADC_CR2_ADON) == 0U)
    {
        __HAL_ADC_ENABLE(&hadc1);
    }

    ADC1->CR2 |= ADC_CAL_BIT;

    while((ADC1->CR2 & ADC_CAL_BIT) != 0U)
    {
    }
}

void Light_Get(uint16_t *raw,uint16_t *mv,uint16_t *percent)
{
    static uint16_t last = 0U;      /* 上一次有效值，转换失败时沿用 */
    uint32_t sum = 0U;
    uint16_t temp;
    uint8_t  i, ok = 0U;

    for (i = 0U; i < 10U; i++)
    {
        HAL_ADC_Start(&hadc1);
        if(HAL_ADC_PollForConversion(&hadc1, 10U) == HAL_OK)
        {
            sum += HAL_ADC_GetValue(&hadc1);
            ok++;
        }
        HAL_ADC_Stop(&hadc1);
    }

    if(ok == 0U)                    /* 一个都没采到，别算，沿用上次 */
    {
        *raw     = last;
        *mv      = (uint16_t)(((uint32_t)last * LIGHT_VREF_MV) / LIGHT_ADC_MAX);
        *percent = (uint8_t)(((uint32_t)last * 100U) / LIGHT_ADC_MAX);
        return;
    }

    temp = (uint16_t)(sum / ok);    /* 用成功次数除，不是固定 10 */
    temp = (uint16_t)(LIGHT_ADC_MAX - temp);
    last = temp;

    *raw = temp;
    *mv      = (uint16_t)(((uint32_t)temp * LIGHT_VREF_MV) / LIGHT_ADC_MAX);
    *percent = (uint8_t)(((uint32_t)temp * 100U) / LIGHT_ADC_MAX);

}

const char *Light_GetPercent (uint8_t percent)
{
    if (percent < 10U)  { return "CLEAR   "; }
    if (percent < 30U)  { return "NORMAL  "; }
    if (percent < 50U)  { return "WARM    "; }
    if (percent < 70U)  { return "HOT     "; }

    return "FIRE!!!";
}
