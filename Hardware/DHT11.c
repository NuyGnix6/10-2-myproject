#include "DHT11.h"
#include "bsp.h"

#define DHT11_PIN()     (((DHT11_GPIO_Port)->IDR & DHT11_Pin) ? 1U : 0U)
#define DHT11_TIMEOUT   0xFFFFU

static void DHt11_ModeOutput(void)
{
    GPIO_InitTypeDef Temp;
    Temp.Pin = DHT11_Pin;
    Temp.Mode = GPIO_MODE_OUTPUT_PP;
    Temp.Pull = GPIO_NOPULL;
    Temp.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT11_GPIO_Port, &Temp);

}

static void DHt11_ModeInput(void)
{
    GPIO_InitTypeDef Temp;
    Temp.Pin = DHT11_Pin;
    Temp.Mode = GPIO_MODE_INPUT;
    Temp.Pull = GPIO_PULLUP;
    Temp.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT11_GPIO_Port, &Temp);
}



void DHT11_Init(void)
{
    DHt11_ModeOutput();
    HAL_GPIO_WritePin(DHT11_GPIO_Port, DHT11_Pin, GPIO_PIN_SET);

    HAL_Delay(1000); // 等待 1ms稳定
}

static uint8_t DHT11_Read(void)
{
    uint8_t i,byte=0U;
    uint32_t timeout;

    for (i = 0U; i < 8U; i++)
    {
        timeout = 0U;
        while (DHT11_PIN() == 0U)
        {
            if (timeout++ > DHT11_TIMEOUT)
            {
                return 0U;
            }
        }// 等待低电平结束，每次都以低电平开始
        
        BSP_DelayUs(40U);// 等待 40us 采样
        byte <<= 1U;
        if (DHT11_PIN() == 1U)
        {
            byte |= 1U;
        }

        //等待高电平结束
        timeout = 0U;
        while (DHT11_PIN() == 1U)
        {
            if (timeout++ > DHT11_TIMEOUT)
            {
                return 0U;
            }
        }
        

    }
    return byte;
}

void DHT11_Request(void)
{
    DHt11_ModeOutput();
    HAL_GPIO_WritePin(DHT11_GPIO_Port, DHT11_Pin, GPIO_PIN_RESET);
    BSP_DelayMs(20U);
}   

/*
 * 返回值：0 成功；1 从机不应答；2 应答低电平不结束；3 应答高电平不结束；
 *         4 读位超时；5 校验和不匹配
 */
uint8_t DHT11_GetData(uint8_t *temp_int, uint8_t *humi_int, uint8_t *temp_dec, uint8_t *humi_dec)
{
    uint8_t buf[5U];
    uint8_t i;
    uint32_t timeout;
    /* 1. 起始信号：主机拉低 20ms，释放 30us */
    HAL_GPIO_WritePin(DHT11_GPIO_Port, DHT11_Pin, GPIO_PIN_SET);
    BSP_DelayUs(30U);

    //切换输入等待响应
    DHt11_ModeInput();
    //响应信号拉低再拉高

    timeout = 0U;
    while (DHT11_PIN() == 1U) //这是等待起始信号的高电平结束
    {
        if (timeout++ > DHT11_TIMEOUT)
        {
            return DHT11_ERR_NO_RESP;
        }
    }

    timeout = 0U;
    while (DHT11_PIN() == 0U) //这是等待起始信号的低电平结束
    {
        if (timeout++ > DHT11_TIMEOUT)
        {
            return DHT11_ERR_NO_RESP;
        }
    }

    timeout = 0U;
    while (DHT11_PIN() == 1U) //这是等待起始信号的高电平结束
    {
        if (timeout++ > DHT11_TIMEOUT)
        {
            return DHT11_ERR_NO_RESP;
        }
    }

    //读5个字节
    for (i = 0U; i < 5U; i++)
    {
        buf[i] = DHT11_Read();
    }

    //检验校验和
    if (buf[4U] != (buf[0U] + buf[1U] + buf[2U] + buf[3U]))
    {
        return DHT11_ERR_CRC;
    }

    //赋值
    *humi_int = buf[0U];
    *humi_dec = buf[1U];
    *temp_int = buf[2U];
    *temp_dec = buf[3U];

    return 0U;
}