#include "LCD.h"
#include "spi.h"
#include "FONT.H"

/* 引脚控制，标签来自 CubeMX */
#define LCD_CS_L    HAL_GPIO_WritePin(LCD_CS_GPIO_Port,  LCD_CS_Pin,  GPIO_PIN_RESET)
#define LCD_CS_H    HAL_GPIO_WritePin(LCD_CS_GPIO_Port,  LCD_CS_Pin,  GPIO_PIN_SET)
#define LCD_DC_L    HAL_GPIO_WritePin(LCD_DC_GPIO_Port,  LCD_DC_Pin,  GPIO_PIN_RESET)
#define LCD_DC_H    HAL_GPIO_WritePin(LCD_DC_GPIO_Port,  LCD_DC_Pin,  GPIO_PIN_SET)
#define LCD_RST_L   HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_RESET)
#define LCD_RST_H   HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET)

/* 每次刷 10 行：240 * 2 * 10 = 4800 字节 */
#define FILL_LINES  10

static uint8_t Lcd_Buf[LCD_W * 2 * FILL_LINES];

static volatile uint8_t Dma_Done = 1;

/* DMA 发完一串数据后进这里，把标志置起来 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if(hspi->Instance == SPI1)
    {
        Dma_Done = 1;
    }
}

/* 交给 DMA 发出去，等它发完再返回；超时就放弃，避免死等卡住整个程序 */
static void LCD_SendDma(uint8_t *buf, uint16_t len)
{
    uint32_t start;

    Dma_Done = 0;
    HAL_SPI_Transmit_DMA(&hspi1, buf, len);

    start = HAL_GetTick();

    while(Dma_Done == 0)
    {
        if((HAL_GetTick() - start) > 50U)
        {
            break;
        }
    }
}

/* 写一个命令字节：DC=0 */
static void LCD_WrCmd(uint8_t cmd)
{
    LCD_DC_L;
    LCD_CS_L;
    HAL_SPI_Transmit(&hspi1, &cmd, 1, 10);
    LCD_CS_H;
}

/* 写一个数据字节：DC=1 */
static void LCD_WrData8(uint8_t dat)
{
    LCD_DC_H;
    LCD_CS_L;
    HAL_SPI_Transmit(&hspi1, &dat, 1, 10);
    LCD_CS_H;
}

/* 写 16 位数据，高字节先发 */
static void LCD_WrData16(uint16_t dat)
{
    uint8_t buf[2];

    buf[0] = (uint8_t)(dat >> 8);
    buf[1] = (uint8_t)(dat & 0xFF);

    LCD_DC_H;
    LCD_CS_L;
    HAL_SPI_Transmit(&hspi1, buf, 2, 10);
    LCD_CS_H;
}

/* 设定要写的矩形范围，然后进入写显存状态 */
static void LCD_SetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    LCD_WrCmd(0x2A);
    LCD_WrData16(x1);
    LCD_WrData16(x2);

    LCD_WrCmd(0x2B);
    LCD_WrData16(y1);
    LCD_WrData16(y2);

    LCD_WrCmd(0x2C);
}

void LCD_Init(void)
{
    LCD_CS_H;
    LCD_DC_H;

    /* 硬件复位 */
    LCD_RST_H;
    HAL_Delay(1);
    LCD_RST_L;
    HAL_Delay(10);
    LCD_RST_H;
    HAL_Delay(120);

    /* ILI9341 上电序列，参数来自厂家资料 */
    LCD_WrCmd(0xCF);
    LCD_WrData8(0x00);
    LCD_WrData8(0xC1);
    LCD_WrData8(0x30);

    LCD_WrCmd(0xED);
    LCD_WrData8(0x64);
    LCD_WrData8(0x03);
    LCD_WrData8(0x12);
    LCD_WrData8(0x81);

    LCD_WrCmd(0xE8);
    LCD_WrData8(0x85);
    LCD_WrData8(0x01);
    LCD_WrData8(0x7A);

    LCD_WrCmd(0xCB);
    LCD_WrData8(0x39);
    LCD_WrData8(0x2C);
    LCD_WrData8(0x00);
    LCD_WrData8(0x34);
    LCD_WrData8(0x02);

    LCD_WrCmd(0xF7);
    LCD_WrData8(0x20);

    LCD_WrCmd(0xEA);
    LCD_WrData8(0x00);
    LCD_WrData8(0x00);

    LCD_WrCmd(0xC0);
    LCD_WrData8(0x21);

    LCD_WrCmd(0xC1);
    LCD_WrData8(0x11);

    LCD_WrCmd(0xC5);
    LCD_WrData8(0x31);
    LCD_WrData8(0x3C);

    LCD_WrCmd(0xC7);
    LCD_WrData8(0x9F);

    LCD_WrCmd(0x36);          /* 扫描方向：竖屏，BGR */
    LCD_WrData8(0x08);

    LCD_WrCmd(0x3A);          /* 像素格式：16 位 565 */
    LCD_WrData8(0x55);

    LCD_WrCmd(0xB1);
    LCD_WrData8(0x00);
    LCD_WrData8(0x1B);

    LCD_WrCmd(0xB6);
    LCD_WrData8(0x0A);
    LCD_WrData8(0xA2);

    LCD_WrCmd(0xF2);
    LCD_WrData8(0x00);

    LCD_WrCmd(0x26);
    LCD_WrData8(0x01);

    LCD_WrCmd(0xE0);          /* 正极性 gamma */
    LCD_WrData8(0x0F);
    LCD_WrData8(0x20);
    LCD_WrData8(0x1D);
    LCD_WrData8(0x0B);
    LCD_WrData8(0x10);
    LCD_WrData8(0x0A);
    LCD_WrData8(0x49);
    LCD_WrData8(0xA9);
    LCD_WrData8(0x3B);
    LCD_WrData8(0x0A);
    LCD_WrData8(0x15);
    LCD_WrData8(0x06);
    LCD_WrData8(0x0C);
    LCD_WrData8(0x06);
    LCD_WrData8(0x00);

    LCD_WrCmd(0xE1);          /* 负极性 gamma */
    LCD_WrData8(0x00);
    LCD_WrData8(0x1F);
    LCD_WrData8(0x22);
    LCD_WrData8(0x04);
    LCD_WrData8(0x0F);
    LCD_WrData8(0x05);
    LCD_WrData8(0x36);
    LCD_WrData8(0x46);
    LCD_WrData8(0x46);
    LCD_WrData8(0x05);
    LCD_WrData8(0x0B);
    LCD_WrData8(0x09);
    LCD_WrData8(0x33);
    LCD_WrData8(0x39);
    LCD_WrData8(0x0F);

    LCD_WrCmd(0x11);          /* 退出睡眠 */
    HAL_Delay(120);

    LCD_WrCmd(0x29);          /* 开显示 */

    LCD_Clear(BLACK);
}

/* 把一个矩形刷成单色，数据走 DMA */
void LCD_Fill(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    uint16_t w;
    uint32_t total;
    uint32_t send;
    uint8_t  hi;
    uint8_t  lo;

    if(x1 > x2 || y1 > y2 || x1 >= LCD_W || y1 >= LCD_H)
    {
        return;
    }
    if(x2 >= LCD_W)
    {
        x2 = LCD_W - 1;
    }
    if(y2 >= LCD_H)
    {
        y2 = LCD_H - 1;
    }

    w  = (uint16_t)(x2 - x1 + 1);
    hi = (uint8_t)(color >> 8);
    lo = (uint8_t)(color & 0xFF);

    LCD_SetWindow(x1, y1, x2, y2);

    /* 缓冲里塞满同样颜色，够 10 行用 */
    for(uint32_t i = 0; i < (uint32_t)w * FILL_LINES; i++)
    {
        Lcd_Buf[i * 2]     = hi;
        Lcd_Buf[i * 2 + 1] = lo;
    }

    total = (uint32_t)w * (uint32_t)(y2 - y1 + 1);

    LCD_DC_H;
    LCD_CS_L;

    while(total > 0)
    {
        send = (uint32_t)w * FILL_LINES;
        if(send > total)
        {
            send = total;
        }

        LCD_SendDma(Lcd_Buf, (uint16_t)(send * 2));

        total -= send;
    }

    LCD_CS_H;
}

void LCD_Clear(uint16_t color)
{
    LCD_Fill(0, 0, LCD_W - 1, LCD_H - 1, color);
}

void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
    uint8_t buf[2];

    if(x >= LCD_W || y >= LCD_H)
    {
        return;
    }

    LCD_SetWindow(x, y, x, y);

    buf[0] = (uint8_t)(color >> 8);
    buf[1] = (uint8_t)(color & 0xFF);

    LCD_DC_H;
    LCD_CS_L;
    HAL_SPI_Transmit(&hspi1, buf, 2, 10);
    LCD_CS_H;
}

/* 画空心矩形框，四条边各刷一条线 */
void LCD_DrawRect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    LCD_Fill(x1, y1, x2, y1, color);
    LCD_Fill(x1, y2, x2, y2, color);
    LCD_Fill(x1, y1, x1, y2, color);
    LCD_Fill(x2, y1, x2, y2, color);
}

/* ---------------- 字符显示，8x16 点阵 ---------------- */

static uint8_t Char_Buf[LCD_FONT_W * LCD_FONT_H * 2];

/* 画一个字符：先算好 8x16 个像素的颜色，再一次性 DMA 送出去 */
void LCD_ShowChar(uint16_t x, uint16_t y, char ch, uint16_t fc, uint16_t bc)
{
    const uint8_t *pfont;
    uint16_t k = 0;
    uint16_t c;
    uint8_t  row;
    uint8_t  col;
    uint8_t  temp;

    if((x + LCD_FONT_W) > LCD_W || (y + LCD_FONT_H) > LCD_H)
    {
        return;
    }

    if(ch < ' ' || ch > '~')
    {
        ch = '?';
    }

    pfont = asc2_1608[(uint8_t)ch - 32U];

    /* 厂家字库是按列存的：每列 2 字节 = 16 像素高，bit7 在最上面。
       这里把它转成按行排列的像素缓冲 */
    for(row = 0; row < LCD_FONT_H; row++)
    {
        for(col = 0; col < LCD_FONT_W; col++)
        {
            temp = pfont[col * 2U + (row >> 3)];

            if((temp & (0x80U >> (row & 0x07U))) != 0U)
            {
                c = fc;
            }
            else
            {
                c = bc;
            }

            Char_Buf[k] = (uint8_t)(c >> 8);
            k++;
            Char_Buf[k] = (uint8_t)(c & 0xFF);
            k++;
        }
    }

    LCD_SetWindow(x, y, (uint16_t)(x + LCD_FONT_W - 1), (uint16_t)(y + LCD_FONT_H - 1));

    LCD_DC_H;
    LCD_CS_L;
    LCD_SendDma(Char_Buf, (uint16_t)sizeof(Char_Buf));
    LCD_CS_H;
}

void LCD_ShowString(uint16_t x, uint16_t y, const char *str, uint16_t fc, uint16_t bc)
{
    while(*str != '\0')
    {
        if((x + LCD_FONT_W) > LCD_W)
        {
            break;
        }

        LCD_ShowChar(x, y, *str, fc, bc);

        x = (uint16_t)(x + LCD_FONT_W);
        str++;
    }
}

/* 显示固定位数的数字，不足的高位补 0 */
void LCD_ShowNum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint16_t fc, uint16_t bc)
{
    char    s[11];
    uint8_t i;

    if(len == 0U)
    {
        return;
    }
    if(len > 10U)
    {
        len = 10U;
    }

    s[len] = '\0';

    for(i = 0; i < len; i++)
    {
        s[len - 1U - i] = (char)('0' + (num % 10U));
        num /= 10U;
    }

    LCD_ShowString(x, y, s, fc, bc);
}

/* 五条色块，用来验证颜色顺序和显示方向 */
void LCD_ShowColorBar(void)
{
    LCD_Fill(0,   0, LCD_W - 1,  63, RED);
    LCD_Fill(0,  64, LCD_W - 1, 127, GREEN);
    LCD_Fill(0, 128, LCD_W - 1, 191, BLUE);
    LCD_Fill(0, 192, LCD_W - 1, 255, WHITE);
    LCD_Fill(0, 256, LCD_W - 1, 319, BLACK);
}
