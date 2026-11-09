/**
 ******************************************************************************
 * @file    lv_port_indev_v9.c
 * @brief   LVGL **9.x** 输入移植层（FT6336U 电容触摸）
 *
 * 8.x → 9.x 的差异：
 *   8.x: lv_indev_drv_t + lv_indev_drv_init + read_cb 字段 + lv_indev_drv_register
 *   9.x: lv_indev_create + lv_indev_set_type + lv_indev_set_read_cb
 *
 *   8.x: LV_INDEV_STATE_PR / LV_INDEV_STATE_REL
 *   9.x: LV_INDEV_STATE_PRESSED / LV_INDEV_STATE_RELEASED
 ******************************************************************************
 */

#include "lvgl.h"
#include "bsp_ft6336.h"

static void touchpad_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    /* 记住上一次的有效坐标：松开时必须把坐标一起给出去，
     * LVGL 要靠它判断"在哪个控件上抬起"，否则可能识别不到点击 */
    static uint16_t last_x = 0U;
    static uint16_t last_y = 0U;

    uint16_t x = 0U;
    uint16_t y = 0U;

    (void)indev;

    if (FT6336_GetPoint(&x, &y) == 0U) {
        last_x = x;
        last_y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }

    data->point.x = last_x;
    data->point.y = last_y;
}

/**
 * @brief  初始化触摸并注册 LVGL 输入设备
 * @retval 0 = 成功，1 = 触摸芯片没应答
 */
uint8_t lv_port_indev_init(void)
{
    lv_indev_t *indev;

    if (FT6336_Init() != 0U) {
        return 1U;              /* 芯片都读不到，注册了也没意义 */
    }

    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touchpad_read_cb);

    return 0U;
}
