/**
 ******************************************************************************
 * @file    bsp_ft6336.h
 * @brief   FT6336U 电容触摸驱动（软件 I2C）
 *
 * 这块 CL24CK229 模块的 10~14 脚是"电阻触摸和电容触摸共用"的：
 *   10  T_CLK / SCL   → I2C 时钟
 *   11  T_CS  / RST   → 触摸复位
 *   12  T_SDI / SDA   → I2C 数据
 *   13  T_SDO         → 空脚，不用接
 *   14  T_IRQ / INT   → 中断（轮询方式不用）
 *
 * 所以丝印看着像 SPI，实际是 I2C。本工程用 PC2/PC3 软件模拟 I2C，
 * 不占硬件 I2C 外设，也不影响 SPI1 刷屏。
 ******************************************************************************
 */

#ifndef __BSP_FT6336_H
#define __BSP_FT6336_H

#include "main.h"

/* ---- 引脚：沿用现有接线 ---- */
#define FT_SCL_PORT         GPIOC
#define FT_SCL_PIN          GPIO_PIN_2      /* 原 T_CLK */
#define FT_SDA_PORT         GPIOC
#define FT_SDA_PIN          GPIO_PIN_3      /* 原 T_SDI */
#define FT_RST_PORT         GPIOC
#define FT_RST_PIN          GPIO_PIN_0      /* 原 T_CS */

/* ---- 坐标映射方向 ----
 * 0 = 原样输出（默认，先试这个）
 * 1 = 交换 X/Y
 * 2 = X 翻转
 * 3 = 交换后 Y 翻转
 * 手指位置和光标对不上时，按 0→1→2→3 依次试
 */
#ifndef TOUCH_MAP_ROTATION
#define TOUCH_MAP_ROTATION  0
#endif

/* ---- 面板分辨率，映射时用 ---- */
#define TOUCH_PANEL_W       240U
#define TOUCH_PANEL_H       320U

/**
 * @brief  初始化触摸 IC
 * @retval 0 = 成功，1 = 失败（I2C 读不到芯片）
 */
uint8_t FT6336_Init(void);

/**
 * @brief  读一个触摸点（已按 TOUCH_MAP_ROTATION 换算）
 * @retval 0 = 有按下，1 = 无触摸
 */
uint8_t FT6336_GetPoint(uint16_t *x, uint16_t *y);

/**
 * @brief  读原始坐标，不做换算（调映射时对照用）
 * @retval 0 = 有按下，1 = 无触摸
 */
uint8_t FT6336_ReadRaw(uint16_t *rx, uint16_t *ry);

#endif /* __BSP_FT6336_H */
