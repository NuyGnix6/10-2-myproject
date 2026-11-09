/**
 ******************************************************************************
 * @file    bsp_ili9341.c
 * @brief   ILI9341V 240x320 4线SPI 屏驱动（硬件SPI + DMA）
 *
 * 要点：
 *   1) 命令/短数据走阻塞 HAL_SPI_Transmit，够快且简单；
 *   2) 像素块（LVGL 的一次 flush）走 HAL_SPI_Transmit_DMA，
 *      期间 CS 保持低、DC 保持高，传完在回调和轮询里拉高 CS；
 *   3) 初始化序列完全照抄厂家 CL24CK229-18A 的参数，别自己改 gamma，
 *      这块屏的面板批次和 gamma 是配好的。
 ******************************************************************************
 */

#include "bsp_ili9341.h"

/* ---- ILI9341 命令 ---- */
#define ILI9341_SWRESET     0x01U
#define ILI9341_SLPOUT      0x11U
#define ILI9341_DISPON      0x29U
#define ILI9341_CASET       0x2AU
#define ILI9341_RASET       0x2BU
#define ILI9341_RAMWR       0x2CU
#define ILI9341_MADCTL      0x36U

/* MADCTL 位定义 */
#define MADCTL_MY           0x80U
#define MADCTL_MX           0x40U
#define MADCTL_MV           0x20U
#define MADCTL_BGR          0x08U

/* 当前逻辑宽高（随旋转变化） */
static uint16_t s_width  = LCD_NATIVE_W;
static uint16_t s_height = LCD_NATIVE_H;

/* DMA 状态 */
static volatile uint8_t s_dma_done = 1U;

/* 填充用的行缓冲：按最大维度算，竖向 320 时一行 640 字节 */
static uint8_t s_line_buf[LCD_NATIVE_H * 2U];

/* ==========================================================================
 * 底层：命令 / 数据
 * ========================================================================== */

static void LCD_WriteCmd(uint8_t cmd)
{
    LCD_DC_CMD();
    LCD_CS_LOW();
    (void)HAL_SPI_Transmit(&LCD_SPI, &cmd, 1U, 100U);
    LCD_CS_HIGH();
}

static void LCD_WriteData(const uint8_t *buf, uint16_t len)
{
    if (len == 0U) {
        return;
    }
    LCD_DC_DATA();
    LCD_CS_LOW();
    (void)HAL_SPI_Transmit(&LCD_SPI, (uint8_t *)buf, len, 1000U);
    LCD_CS_HIGH();
}

static void LCD_WriteDataByte(uint8_t data)
{
    LCD_WriteData(&data, 1U);
}

/* ==========================================================================
 * 初始化序列表
 * ========================================================================== */

typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t len;
} lcd_init_cmd_t;

static const lcd_init_cmd_t s_init_cmds[] = {
    /* Power control */
    { 0xCFU, { 0x00U, 0xC1U, 0x30U },                              3U },
    { 0xEDU, { 0x64U, 0x03U, 0x12U, 0x81U },                       4U },
    { 0xE8U, { 0x85U, 0x01U, 0x7AU },                              3U },
    { 0xCBU, { 0x39U, 0x2CU, 0x00U, 0x34U, 0x02U },                5U },
    { 0xF7U, { 0x20U },                                            1U },
    { 0xEAU, { 0x00U, 0x00U },                                     2U },
    { 0xC0U, { 0x21U },                                            1U },  /* VRH[5:0] */
    { 0xC1U, { 0x11U },                                            1U },  /* SAP[2:0];BT[3:0] */
    /* VCM control */
    { 0xC5U, { 0x31U, 0x3CU },                                     2U },
    { 0xC7U, { 0x9FU },                                            1U },
    /* Memory Access Control：先给竖屏，后面按旋转重设 */
    { ILI9341_MADCTL,  { MADCTL_BGR },                             1U },
    /* 像素格式：0x55 = 16bit RGB565 */
    { 0x3AU, { 0x55U },                                            1U },
    { 0xB1U, { 0x00U, 0x1BU },                                     2U },
    { 0xB6U, { 0x0AU, 0xA2U },                                     2U },
    { 0xF2U, { 0x00U },                                            1U },  /* 3Gamma off */
    { 0x26U, { 0x01U },                                            1U },
    /* 正极性 gamma */
    { 0xE0U, { 0x0FU, 0x20U, 0x1DU, 0x0BU, 0x10U, 0x0AU, 0x49U, 0xA9U,
               0x3BU, 0x0AU, 0x15U, 0x06U, 0x0CU, 0x06U, 0x00U },   15U },
    /* 负极性 gamma */
    { 0xE1U, { 0x00U, 0x1FU, 0x22U, 0x04U, 0x0FU, 0x05U, 0x36U, 0x46U,
               0x46U, 0x05U, 0x0BU, 0x09U, 0x33U, 0x39U, 0x0FU },   15U },
};

#define INIT_CMD_COUNT  (sizeof(s_init_cmds) / sizeof(s_init_cmds[0]))

/* ==========================================================================
 * 对外：初始化
 * ========================================================================== */

void LCD_Init(void)
{
    /* 上电电平先摆好：CS/RST 高，背光灭 */
    LCD_CS_HIGH();
    LCD_RST_HIGH();
    LCD_BL_OFF();
    HAL_Delay(10U);

    /* 硬件复位：低 20ms，拉高后等 120ms */
    LCD_RST_LOW();
    HAL_Delay(20U);
    LCD_RST_HIGH();
    HAL_Delay(120U);

    /* 逐条发初始化序列 */
    for (uint32_t i = 0U; i < INIT_CMD_COUNT; i++) {
        LCD_WriteCmd(s_init_cmds[i].cmd);
        if (s_init_cmds[i].len > 0U) {
            LCD_WriteData(s_init_cmds[i].data, s_init_cmds[i].len);
        }
    }

    /* Sleep Out，必须等 120ms 才能 Display On */
    LCD_WriteCmd(ILI9341_SLPOUT);
    HAL_Delay(120U);
    LCD_WriteCmd(ILI9341_DISPON);
    HAL_Delay(20U);

    LCD_SetRotation(LCD_ROTATION);

    /* 开背光 */
    LCD_BL_ON();
}

/* ==========================================================================
 * 旋转
 * ========================================================================== */

void LCD_SetRotation(uint8_t rotation)
{
    uint8_t madctl;

    switch (rotation) {
        case 0U:    /* 竖屏 240x320 */
            madctl     = MADCTL_BGR;
            s_width    = LCD_NATIVE_W;
            s_height   = LCD_NATIVE_H;
            break;

        case 1U:    /* 横屏 320x240（MV=1 MX=1，对应厂家 dir=3） */
            madctl     = MADCTL_BGR | MADCTL_MV | MADCTL_MX;
            s_width    = LCD_NATIVE_H;
            s_height   = LCD_NATIVE_W;
            break;

        case 2U:    /* 竖屏翻转 */
            madctl     = MADCTL_BGR | MADCTL_MY | MADCTL_MX;
            s_width    = LCD_NATIVE_W;
            s_height   = LCD_NATIVE_H;
            break;

        case 3U:    /* 横屏翻转 */
            madctl     = MADCTL_BGR | MADCTL_MV | MADCTL_MY;
            s_width    = LCD_NATIVE_H;
            s_height   = LCD_NATIVE_W;
            break;

        default:
            return;
    }

    LCD_WriteCmd(ILI9341_MADCTL);
    LCD_WriteDataByte(madctl);
}

uint16_t LCD_GetWidth(void)
{
    return s_width;
}

uint16_t LCD_GetHeight(void)
{
    return s_height;
}

/* ==========================================================================
 * 窗口设置
 * ========================================================================== */

void LCD_SetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    uint8_t buf[4];

    /* 列地址 CASET */
    LCD_WriteCmd(ILI9341_CASET);
    buf[0] = (uint8_t)(x1 >> 8);
    buf[1] = (uint8_t)(x1 & 0xFFU);
    buf[2] = (uint8_t)(x2 >> 8);
    buf[3] = (uint8_t)(x2 & 0xFFU);
    LCD_WriteData(buf, 4U);

    /* 行地址 RASET */
    LCD_WriteCmd(ILI9341_RASET);
    buf[0] = (uint8_t)(y1 >> 8);
    buf[1] = (uint8_t)(y1 & 0xFFU);
    buf[2] = (uint8_t)(y2 >> 8);
    buf[3] = (uint8_t)(y2 & 0xFFU);
    LCD_WriteData(buf, 4U);

    /* 准备写显存，之后的数据都当像素 */
    LCD_WriteCmd(ILI9341_RAMWR);
}

/* ==========================================================================
 * DMA 刷像素块
 * ========================================================================== */

void LCD_DMA_Start(const uint8_t *buf, uint32_t len)
{
    if ((buf == NULL) || (len == 0U)) {
        return;
    }

    /* HAL 的 Size 是 uint16_t，一次别超过 65535 字节。
     * LVGL 的 draw buffer 一般是几十 KB，不会撞到；真撞到就把缓冲改小。 */
    if (len > 0xFFFFU) {
        len = 0xFFFFU;
    }

    while (s_dma_done == 0U) {
        /* 上一笔还没完，等它 */
    }

    s_dma_done = 0U;
    LCD_DC_DATA();
    LCD_CS_LOW();
    (void)HAL_SPI_Transmit_DMA(&LCD_SPI, (uint8_t *)buf, (uint16_t)len);
}

uint8_t LCD_DMA_IsBusy(void)
{
    return (s_dma_done == 0U) ? 1U : 0U;
}

void LCD_DMA_WaitDone(uint32_t timeout_ms)
{
    uint32_t tick = HAL_GetTick();

    while (s_dma_done == 0U) {
        if ((timeout_ms != 0U) && ((HAL_GetTick() - tick) > timeout_ms)) {
            /* 超时就强制收尾，避免 LVGL 卡死在等 flush_ready */
            LCD_CS_HIGH();
            s_dma_done = 1U;
            break;
        }
    }
}

/**
 * @brief SPI DMA 发送完成回调（覆盖 HAL 的 weak 函数）
 * @note  这里只管把 CS 拉高并置完成标志，不调用任何 LVGL 接口，
 *        这样在中断上下文里是安全的。
 *
 *        如果同时用了 lv_port_lcd_ili9341.c（教程那套，它自己重写了这两个弱函数），
 *        必须在编译选项里定义 BSP_ILI9341_NO_HAL_CB，否则链接时会报重复定义。
 */
#ifndef BSP_ILI9341_NO_HAL_CB

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == LCD_SPI.Instance) {
        LCD_CS_HIGH();
        s_dma_done = 1U;
    }
}

/* 出错也要把标志放掉，否则 LVGL 会一直等 */
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == LCD_SPI.Instance) {
        LCD_CS_HIGH();
        s_dma_done = 1U;
    }
}

#endif /* BSP_ILI9341_NO_HAL_CB */

/* ==========================================================================
 * 填充 / 画点
 * ========================================================================== */

void LCD_Fill(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    uint16_t w, h;
    uint32_t total;
    uint16_t i;

    if ((x1 >= s_width) || (y1 >= s_height)) {
        return;
    }
    if (x2 >= s_width) {
        x2 = (uint16_t)(s_width - 1U);
    }
    if (y2 >= s_height) {
        y2 = (uint16_t)(s_height - 1U);
    }
    if ((x2 < x1) || (y2 < y1)) {
        return;
    }

    w = (uint16_t)(x2 - x1 + 1U);
    h = (uint16_t)(y2 - y1 + 1U);

    /* 先按行填好一行像素，再重复发送 h 次 */
    for (i = 0U; i < w; i++) {
        s_line_buf[i * 2U]      = (uint8_t)(color >> 8);
        s_line_buf[i * 2U + 1U] = (uint8_t)(color & 0xFFU);
    }

    LCD_SetWindow(x1, y1, x2, y2);

    LCD_DC_DATA();
    LCD_CS_LOW();
    total = (uint32_t)w * (uint32_t)h;
    while (total > 0U) {
        uint16_t chunk = (total > w) ? w : (uint16_t)total;
        (void)HAL_SPI_Transmit(&LCD_SPI, s_line_buf, (uint16_t)(chunk * 2U), 1000U);
        total -= chunk;
    }
    LCD_CS_HIGH();
}

void LCD_FillScreen(uint16_t color)
{
    LCD_Fill(0U, 0U, (uint16_t)(s_width - 1U), (uint16_t)(s_height - 1U), color);
}

void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    uint8_t buf[2];

    if ((x >= s_width) || (y >= s_height)) {
        return;
    }

    LCD_SetWindow(x, y, x, y);

    buf[0] = (uint8_t)(color >> 8);
    buf[1] = (uint8_t)(color & 0xFFU);

    LCD_DC_DATA();
    LCD_CS_LOW();
    (void)HAL_SPI_Transmit(&LCD_SPI, buf, 2U, 100U);
    LCD_CS_HIGH();
}

/* ==========================================================================
 * 背光
 * ========================================================================== */

void LCD_BackLight(uint8_t on)
{
    if (on != 0U) {
        LCD_BL_ON();
    } else {
        LCD_BL_OFF();
    }
}
