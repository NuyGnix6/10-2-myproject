/**
 ******************************************************************************
 * @file    bsp_ili9341.h
 * @brief   ILI9341V 240x320 4线SPI 屏驱动（硬件SPI + DMA）
 * @note    屏幕型号 CL24CK229-18A / 转接板 24003
 *          接线见 README.md，本文件只依赖 HAL 库
 ******************************************************************************
 */

#ifndef __BSP_ILI9341_H
#define __BSP_ILI9341_H

#include "main.h"
#include "tim.h"        /* htim2：PA1 背光走 TIM2_CH2 PWM */

/* ==========================================================================
 * 1. 屏参数
 * ========================================================================== */

#define LCD_NATIVE_W        240U        /* 面板物理宽 */
#define LCD_NATIVE_H        320U        /* 面板物理高 */

/* 旋转：0 = 竖屏 240x320，1 = 横屏 320x240 */
#ifndef LCD_ROTATION
#define LCD_ROTATION        0
#endif

/* 把 SPI 句柄抽出来，方便换成 SPI2/SPI3 */
#ifndef LCD_SPI
#define LCD_SPI             hspi1
#endif
extern SPI_HandleTypeDef    LCD_SPI;

/* ==========================================================================
 * 2. 引脚宏
 *    如果 CubeMX 里加了 User Label，下面这些 _Pin / _GPIO_Port 宏
 *    已经在 main.h 里自动生成了，直接用；否则按实际接线改。
 * ========================================================================== */

#ifdef LCD_CS_Pin
/* ---- 用 CubeMX User Label 的路径 ---- */
#define LCD_BL_GPIO_PORT    LCD_BL_GPIO_Port
#define LCD_BL_GPIO_PIN     LCD_BL_Pin
#define LCD_DC_GPIO_PORT    LCD_DC_GPIO_Port
#define LCD_DC_GPIO_PIN     LCD_DC_Pin
#define LCD_RST_GPIO_PORT   LCD_RST_GPIO_Port
#define LCD_RST_GPIO_PIN    LCD_RST_Pin
#define LCD_CS_GPIO_PORT    LCD_CS_GPIO_Port
#define LCD_CS_GPIO_PIN     LCD_CS_Pin
#else
/* ---- 没加 Label 时手填 ---- */
#define LCD_CS_GPIO_PORT    GPIOA
#define LCD_CS_GPIO_PIN     GPIO_PIN_4
#define LCD_RST_GPIO_PORT   GPIOA
#define LCD_RST_GPIO_PIN    GPIO_PIN_3
#define LCD_DC_GPIO_PORT    GPIOA
#define LCD_DC_GPIO_PIN     GPIO_PIN_2
#define LCD_BL_GPIO_PORT    GPIOA
#define LCD_BL_GPIO_PIN     GPIO_PIN_1
#endif

#define LCD_CS_LOW()        HAL_GPIO_WritePin(LCD_CS_GPIO_PORT,  LCD_CS_GPIO_PIN,  GPIO_PIN_RESET)
#define LCD_CS_HIGH()       HAL_GPIO_WritePin(LCD_CS_GPIO_PORT,  LCD_CS_GPIO_PIN,  GPIO_PIN_SET)
#define LCD_DC_CMD()        HAL_GPIO_WritePin(LCD_DC_GPIO_PORT,  LCD_DC_GPIO_PIN,  GPIO_PIN_RESET)
#define LCD_DC_DATA()       HAL_GPIO_WritePin(LCD_DC_GPIO_PORT,  LCD_DC_GPIO_PIN,  GPIO_PIN_SET)
#define LCD_RST_LOW()       HAL_GPIO_WritePin(LCD_RST_GPIO_PORT, LCD_RST_GPIO_PIN, GPIO_PIN_RESET)
#define LCD_RST_HIGH()      HAL_GPIO_WritePin(LCD_RST_GPIO_PORT, LCD_RST_GPIO_PIN, GPIO_PIN_SET)
/* 背光：主工程 PA1 是 TIM2_CH2 复用推挽，HAL_GPIO_WritePin 写它不起作用，
   必须走 PWM 比较值控制。80% 亮度 */
#define LCD_BL_ON()         do { \
                                HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2); \
                                __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 80); \
                            } while(0)
#define LCD_BL_OFF()        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 0)

/* ==========================================================================
 * 3. 颜色（RGB565）
 * ========================================================================== */

#define LCD_COLOR_WHITE     0xFFFFU
#define LCD_COLOR_BLACK     0x0000U
#define LCD_COLOR_BLUE      0x001FU
#define LCD_COLOR_RED       0xF800U
#define LCD_COLOR_GREEN     0x07E0U
#define LCD_COLOR_YELLOW    0xFFE0U
#define LCD_COLOR_CYAN      0x07FFU
#define LCD_COLOR_MAGENTA   0xF81FU
#define LCD_COLOR_GRAY      0x8410U
#define LCD_COLOR_ORANGE    0xFD20U

/* ==========================================================================
 * 4. 接口
 * ========================================================================== */

void     LCD_Init(void);
void     LCD_SetRotation(uint8_t rotation);      /* 0=竖屏 1=横屏 */
uint16_t LCD_GetWidth(void);
uint16_t LCD_GetHeight(void);

void     LCD_SetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void     LCD_Fill(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void     LCD_FillScreen(uint16_t color);
void     LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color);

/* DMA 刷屏：给 LVGL 的 flush_cb 用。发送完成会在 SPI 的 TxCplt 回调里拉高 CS */
void     LCD_DMA_Start(const uint8_t *buf, uint32_t len);
uint8_t  LCD_DMA_IsBusy(void);
void     LCD_DMA_WaitDone(uint32_t timeout_ms);

void     LCD_BackLight(uint8_t on);

#endif /* __BSP_ILI9341_H */
