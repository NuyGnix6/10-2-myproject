#include "bsp.h"


void BSP_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

void BSP_DelayUs(uint32_t us)
{
    uint32_t start;
    uint32_t ticks;

    if (us == 0U)
    {
        return;
    }

    start = DWT->CYCCNT;
    ticks = us * (SystemCoreClock / 1000000U);

    while ((DWT->CYCCNT - start) < ticks)
    {
        ;
    }
}

void BSP_DelayMs(uint32_t ms)
{
    while (ms > 0U)
    {
        BSP_DelayUs(1000U);
        ms--;
    }
}