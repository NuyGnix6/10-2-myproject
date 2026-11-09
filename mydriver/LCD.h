#ifndef __LCD_H__
#define __LCD_H__

#include "main.h"

/* CL24CK229-18A：ILI9341，2.4 寸 */
#define LCD_W   240
#define LCD_H   320

/* 常用颜色，RGB565 */
#define WHITE   0xFFFF
#define BLACK   0x0000
#define RED     0xF800
#define GREEN   0x07E0
#define BLUE    0x001F
#define YELLOW  0xFFE0
#define GRAY    0x8410

void LCD_Init(void);
void LCD_Clear(uint16_t color);
void LCD_Fill(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color);
void LCD_DrawRect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void LCD_ShowColorBar(void);

/* 字符显示：8x16 点阵，字库来自厂家 FONT.H */
#define LCD_FONT_W   8
#define LCD_FONT_H   16

void LCD_ShowChar(uint16_t x, uint16_t y, char ch, uint16_t fc, uint16_t bc);
void LCD_ShowString(uint16_t x, uint16_t y, const char *str, uint16_t fc, uint16_t bc);
void LCD_ShowNum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint16_t fc, uint16_t bc);

#endif
