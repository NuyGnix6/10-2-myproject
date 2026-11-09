#ifndef MYUI_REWRITER_H
#define MYUI_REWRITER_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "lvgl.h"

#define COL_BG      lv_color_hex(0x0D1117)   /* 页面底色 */
#define COL_CARD    lv_color_hex(0x1C2128)   /* 卡片底色 */
#define COL_TEXT    lv_color_hex(0xE6EDF3)   /* 主文字 */
#define COL_DIM     lv_color_hex(0x8B949E)   /* 次要文字 */
#define COL_TEMP    lv_color_hex(0xFF8C42)   /* 温度橙 */
#define COL_HUMI    lv_color_hex(0x39D0E0)   /* 湿度青 */
#define COL_OK      lv_color_hex(0x3FB950)   /* 正常绿 */
#define COL_WARN    lv_color_hex(0xF85149)   /* 超限红（暂未使用，接传感器后用） */
#define COL_ALARM   lv_color_hex(0xC62828)
#define COL_LIGHT   lv_color_hex(0x2A3436)
void myui_rewriter_init(void);
void myui_update(uint8_t temp_int, uint8_t temp_dec,
                 uint8_t humi_int, uint8_t humi_dec,
                 uint8_t fire_percent, uint8_t alarm_flag);

#endif
