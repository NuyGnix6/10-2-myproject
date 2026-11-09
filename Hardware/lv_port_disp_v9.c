/**
 ******************************************************************************
 * @file    lv_port_disp_v9.c
 * @brief   LVGL 9.x 显示移植层（STM32F407VET6 + ILI9341 320x240 横屏）
 *
 * 与 8.x 的差异：
 *   8.x: lv_disp_drv_t / lv_disp_drv_register / buffer 按【像素数】
 *   9.x: lv_display_create / lv_display_set_buffers / buffer 按【字节数】
 *        flush 回调第三个参数是 uint8_t*，不是 lv_color_t*
 *
 * 依赖：bsp_ili9341.c 提供 LCD_SetWindow / LCD_SPI / LCD_DC_DATA 等
 ******************************************************************************
 */

#include "lvgl.h"
#include "bsp_ili9341.h"

/* ===================== 可调参数 ===================== */

#define MY_DISP_HOR_RES     240         /* 与 bsp_ili9341.h 的 LCD_ROTATION=0 对应（竖屏） */
#define MY_DISP_VER_RES     320

/* 发送方式：1 = DMA（快），0 = 阻塞（稳，调试时用） */
#define DISP_USE_DMA        0

/* 缓冲行数：240 * 40 * 2 = 19200 字节 */
#define DRAW_BUF_LINES      40
#define DRAW_BUF_BYTES      (MY_DISP_HOR_RES * DRAW_BUF_LINES * 2U)

/* ===================== 静态变量 ===================== */

static lv_display_t *s_disp = NULL;

/* 必须显式对齐：LV_ATTRIBUTE_MEM_ALIGN 在 lv_conf_internal.h 里展开为空 */
static uint8_t s_buf1[DRAW_BUF_BYTES] __attribute__((aligned(32)));

/* 调试用：等于 0xDEAD 说明 buffer 落进 CCM（DMA 访问不到） */
volatile uint32_t g_disp_buf_check = 0U;

/* ===================== flush 回调 ===================== */

static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint32_t w = (uint32_t)lv_area_get_width(area);
    uint32_t h = (uint32_t)lv_area_get_height(area);
    uint32_t n = w * h;

    LCD_SetWindow((uint16_t)area->x1, (uint16_t)area->y1,
                  (uint16_t)area->x2, (uint16_t)area->y2);

    /* LVGL 的 lv_color_t 是小端：[0x00,0xF8] 表示 0xF800
     * ILI9341 要大端：先收 0xF8 再收 0x00
     * 这里逐像素交换高低字节，走 8bit 发送（和 LCD_FillScreen 完全同一条路径） */
    {
        uint16_t *p = (uint16_t *)px_map;
        uint32_t  i;

        for (i = 0; i < n; i++) {
            uint16_t v = p[i];
            p[i] = (uint16_t)((v >> 8) | (v << 8));
        }

#if DISP_USE_DMA
        LCD_DMA_Start(px_map, n * 2U);
        LCD_DMA_WaitDone(100U);
#else
        LCD_DC_DATA();
        LCD_CS_LOW();
        (void)HAL_SPI_Transmit(&LCD_SPI, px_map, (uint16_t)(n * 2U), 1000U);
        LCD_CS_HIGH();
#endif
    }

    lv_display_flush_ready(disp);
}

/* ===================== 初始化 ===================== */

void lv_port_disp_init(void)
{
    /* CCM 检查：F407 的 64KB CCM 在 0x10000000，DMA 访问不到。
     * 这里只记录标志，不再 while(1) 卡死（卡死会掩盖真正的问题）。 */
    {
        uint32_t addr = (uint32_t)s_buf1;
        if ((addr >= 0x10000000UL) && (addr < 0x10010000UL)) {
            g_disp_buf_check = 0xDEADUL;
        }
    }

    s_disp = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);

    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(s_disp, disp_flush_cb);

    /* 第 4 个参数是【字节数】。8.x 是像素数，这里改了 */
    lv_display_set_buffers(s_disp, s_buf1, NULL, sizeof(s_buf1),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
}
