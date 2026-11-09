/**
 ******************************************************************************
 * @file    bsp_ft6336.c
 * @brief   FT6336U 电容触摸驱动（软件 I2C，PC2=SCL / PC3=SDA / PC0=RST）
 *
 * 寄存器（FT5206 / FT6236 / FT6336 通用）：
 *   0x02      触摸点数，低 4bit 有效，0 = 没摸
 *   0x03~0x06 点1：XH XL YH YL
 *             XH 高 4bit 是事件类型，0x80 = 真实按下
 *   0x09~0x0C 点2 ... 每点 6 字节
 *
 * I2C 地址 0x38（7 位），写命令 0x70、读命令 0x71
 ******************************************************************************
 */

#include "bsp_ft6336.h"
#include "bsp.h"

/* ---- 命令与寄存器 ---- */
#define FT_CMD_WR           0x70U
#define FT_CMD_RD           0x71U

#define FT_REG_NUM_FINGER   0x02U
#define FT_REG_POINT1       0x03U

#define FT_EVENT_CONTACT    0x80U

/* ---- 引脚操作 ---- */
#define SCL_H()     HAL_GPIO_WritePin(FT_SCL_PORT, FT_SCL_PIN, GPIO_PIN_SET)
#define SCL_L()     HAL_GPIO_WritePin(FT_SCL_PORT, FT_SCL_PIN, GPIO_PIN_RESET)
#define SDA_H()     HAL_GPIO_WritePin(FT_SDA_PORT, FT_SDA_PIN, GPIO_PIN_SET)
#define SDA_L()     HAL_GPIO_WritePin(FT_SDA_PORT, FT_SDA_PIN, GPIO_PIN_RESET)
#define SDA_READ()  (HAL_GPIO_ReadPin(FT_SDA_PORT, FT_SDA_PIN) == GPIO_PIN_SET)

/* 100kHz 左右，2us 半周期足够 */
#define I2C_DELAY() BSP_DelayUs(2)

/* SDA 方向切换：写的时候推挽/开漏输出，读的时候切输入上拉 */
static void sda_out(void)
{
    GPIO_InitTypeDef g = {0};

    g.Pin   = FT_SDA_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_OD;      /* 开漏，避免和从机抢总线 */
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(FT_SDA_PORT, &g);
}

static void sda_in(void)
{
    GPIO_InitTypeDef g = {0};

    g.Pin   = FT_SDA_PIN;
    g.Mode  = GPIO_MODE_INPUT;
    g.Pull  = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(FT_SDA_PORT, &g);
}

/* ---------------- 基础时序 ---------------- */

static void i2c_start(void)
{
    sda_out();
    SDA_H();
    SCL_H();
    I2C_DELAY();
    SDA_L();
    I2C_DELAY();
    SCL_L();
    I2C_DELAY();
}

static void i2c_stop(void)
{
    sda_out();
    SCL_L();
    SDA_L();
    I2C_DELAY();
    SCL_H();
    I2C_DELAY();
    SDA_H();
    I2C_DELAY();
}

static void i2c_write_byte(uint8_t b)
{
    uint8_t i;

    sda_out();

    for(i = 0U; i < 8U; i++)
    {
        if((b & 0x80U) != 0U)
        {
            SDA_H();
        }
        else
        {
            SDA_L();
        }
        b = (uint8_t)(b << 1);

        I2C_DELAY();
        SCL_H();
        I2C_DELAY();
        SCL_L();
        I2C_DELAY();
    }
}

static uint8_t i2c_read_byte(uint8_t ack)
{
    uint8_t i;
    uint8_t b = 0U;

    sda_in();

    for(i = 0U; i < 8U; i++)
    {
        b = (uint8_t)(b << 1);
        SCL_H();
        I2C_DELAY();
        if(SDA_READ())
        {
            b |= 1U;
        }
        SCL_L();
        I2C_DELAY();
    }

    /* 主机回 ACK / NACK */
    sda_out();
    if(ack != 0U)
    {
        SDA_L();
    }
    else
    {
        SDA_H();
    }
    I2C_DELAY();
    SCL_H();
    I2C_DELAY();
    SCL_L();
    I2C_DELAY();
    SDA_H();

    return b;
}

/* 等 ACK，返回 0 = 有应答 */
static uint8_t i2c_wait_ack(void)
{
    uint16_t t = 0U;

    sda_in();
    SCL_H();
    I2C_DELAY();

    while(SDA_READ())
    {
        t++;
        if(t > 500U)
        {
            SCL_L();
            return 1U;              /* 超时，从机没应答 */
        }
        I2C_DELAY();
    }

    SCL_L();
    I2C_DELAY();
    return 0U;
}

/* ---------------- FT6336 寄存器读写 ---------------- */

static uint8_t ft_read_reg(uint8_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i;

    i2c_start();
    i2c_write_byte(FT_CMD_WR);
    if(i2c_wait_ack() != 0U) { i2c_stop(); return 1U; }

    i2c_write_byte(reg);
    if(i2c_wait_ack() != 0U) { i2c_stop(); return 1U; }

    i2c_start();
    i2c_write_byte(FT_CMD_RD);
    if(i2c_wait_ack() != 0U) { i2c_stop(); return 1U; }

    for(i = 0U; i < len; i++)
    {
        buf[i] = i2c_read_byte((i == (uint8_t)(len - 1U)) ? 0U : 1U);
    }

    i2c_stop();
    return 0U;
}

/* ---------------- 对外接口 ---------------- */

uint8_t FT6336_Init(void)
{
    GPIO_InitTypeDef g = {0};
    uint8_t n = 0U;

    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* SCL 推挽输出 */
    g.Pin   = FT_SCL_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(FT_SCL_PORT, &g);

    /* SDA 开漏输出（读的时候再切输入） */
    g.Pin  = FT_SDA_PIN;
    g.Mode = GPIO_MODE_OUTPUT_OD;
    HAL_GPIO_Init(FT_SDA_PORT, &g);

    /* RST 推挽输出 */
    g.Pin  = FT_RST_PIN;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(FT_RST_PORT, &g);

    SCL_H();
    SDA_H();

    /* 厂家时序：复位拉低 20ms，释放后等 300ms 让它起振 */
    HAL_GPIO_WritePin(FT_RST_PORT, FT_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(FT_RST_PORT, FT_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(300);

    /* 读一次点数寄存器确认芯片在 */
    if(ft_read_reg(FT_REG_NUM_FINGER, &n, 1U) != 0U)
    {
        return 1U;
    }

    return 0U;
}

uint8_t FT6336_ReadRaw(uint16_t *rx, uint16_t *ry)
{
    uint8_t buf[4];
    uint8_t num;

    if(ft_read_reg(FT_REG_NUM_FINGER, &num, 1U) != 0U)
    {
        return 1U;
    }

    num &= 0x0FU;
    if((num == 0U) || (num > 2U))
    {
        return 1U;                  /* 没摸 */
    }

    if(ft_read_reg(FT_REG_POINT1, buf, 4U) != 0U)
    {
        return 1U;
    }

    /* buf[0] 高 4bit 是事件类型，0x80 才是"按下"；
       0x00/0x40 分别是抬起和接触，都不能当成有效点 */
    if((buf[0] & 0xF0U) != FT_EVENT_CONTACT)
    {
        return 1U;
    }

    *rx = (uint16_t)((((uint16_t)buf[0] & 0x0FU) << 8) | buf[1]);
    *ry = (uint16_t)((((uint16_t)buf[2] & 0x0FU) << 8) | buf[3]);

    return 0U;
}

uint8_t FT6336_GetPoint(uint16_t *x, uint16_t *y)
{
    uint16_t rx = 0U;
    uint16_t ry = 0U;

    if(FT6336_ReadRaw(&rx, &ry) != 0U)
    {
        return 1U;
    }

#if   TOUCH_MAP_ROTATION == 1
    *x = (uint16_t)((ry * TOUCH_PANEL_W) / TOUCH_PANEL_H);
    *y = (uint16_t)((rx * TOUCH_PANEL_H) / TOUCH_PANEL_W);
#elif TOUCH_MAP_ROTATION == 2
    *x = (uint16_t)((TOUCH_PANEL_W - 1U) - rx);
    *y = ry;
#elif TOUCH_MAP_ROTATION == 3
    *x = (uint16_t)((TOUCH_PANEL_W - 1U) - ry);
    *y = (uint16_t)((TOUCH_PANEL_H - 1U) - rx);
#else
    *x = rx;
    *y = ry;
#endif

    return 0U;
}
