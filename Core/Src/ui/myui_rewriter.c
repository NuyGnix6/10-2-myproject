#include "myui_rewriter.h"
#include "appdata.h"
LV_FONT_DECLARE(lv_font_ui_18);
LV_FONT_DECLARE(lv_font_num_32);
LV_FONT_DECLARE(lv_font_ui_26);

static lv_obj_t *g_lab_temp_val;     /* 温度数值 */
static lv_obj_t *g_lab_humi_val;     /* 湿度数值 */
static lv_obj_t *g_dot_fire;         /* 明火状态圆点 */
static lv_obj_t *g_lab_fire_state;   /* 明火状态文字 */
static lv_obj_t *g_lab_fire_ok;  /* 明火状态文字2 */

static lv_obj_t *page_main;
static lv_obj_t *page_setting;
static lv_obj_t *page_alarm;

static lv_obj_t *g_alarm_title;      /* 报警页主标题 */
static lv_obj_t *g_alarm_sub;        /* 报警页副标题，带当前值 */

static void page_switch(lv_obj_t *show, lv_obj_t *hide);

typedef struct {
    int32_t temp_high;
    int32_t humi_high;
} threshold_t;

static threshold_t g_thr = { 40, 75 };


typedef struct {
    lv_obj_t *slider;
    int32_t step;      /* -1 或 +1 */
} step_ctx_t;

typedef struct {
    lv_obj_t *label;     /* 右侧「阈值 xx °C」 */
    lv_obj_t *cur_label; /* 左侧大号当前值 */
    const char *unit;
    step_ctx_t minus;
    step_ctx_t plus;
    lv_obj_t *slider;
} limit_ctx_t;


static limit_ctx_t g_ctx_temp;
static limit_ctx_t g_ctx_humi;



//回调


static void back_to_main_cb(lv_event_t *e)
{
    page_switch(page_main, page_setting);
}

static void main_to_setting_cb(lv_event_t *e)
{
    /* 串口命令（T/H）或按键可能改过阈值，每次进页面都从 Store_Data 重新读一遍，
       否则显示的还是上电时那次的快照 */
    g_thr.temp_high = (int32_t)Temp_HLimit;
    g_thr.humi_high = (int32_t)Humi_HLimit;

    if(g_ctx_temp.slider) {
        lv_slider_set_value(g_ctx_temp.slider, g_thr.temp_high, LV_ANIM_OFF);
        lv_obj_send_event(g_ctx_temp.slider, LV_EVENT_VALUE_CHANGED, NULL);
    }
    if(g_ctx_humi.slider) {
        lv_slider_set_value(g_ctx_humi.slider, g_thr.humi_high, LV_ANIM_OFF);
        lv_obj_send_event(g_ctx_humi.slider, LV_EVENT_VALUE_CHANGED, NULL);
    }
    page_switch(page_setting, page_main);
}

static void limit_slider_cb(lv_event_t *e)
{
    limit_ctx_t *ctx = (limit_ctx_t *)lv_event_get_user_data(e);
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t v = lv_slider_get_value(slider);
    lv_label_set_text_fmt(ctx->label, "阈值 %d %s", (int)v, ctx->unit);
}


static void step_slider_cb(lv_event_t *e)
{
    step_ctx_t *ctx = (step_ctx_t *)lv_event_get_user_data(e);
    int32_t v = lv_slider_get_value(ctx->slider) + ctx->step;
    lv_slider_set_value(ctx->slider, v, LV_ANIM_OFF);
    lv_obj_send_event(ctx->slider, LV_EVENT_VALUE_CHANGED, NULL);
}

static void save_finish_cb(lv_timer_t *t)
{
    lv_obj_t *lab = lv_timer_get_user_data(t);
    lv_label_set_text(lab, "保存");
    page_switch(page_main, page_setting);
}

static void save_cb(lv_event_t *e)
{
    lv_obj_t *lab = lv_event_get_target(e);

    if(g_ctx_temp.slider)
    {
        g_thr.temp_high = lv_slider_get_value(g_ctx_temp.slider);
        Temp_HLimit     = (uint32_t)g_thr.temp_high;
    }
    if(g_ctx_humi.slider)
    {
        g_thr.humi_high = lv_slider_get_value(g_ctx_humi.slider);
        Humi_HLimit     = (uint32_t)g_thr.humi_high;
    }
    MyFlash_Save();      /* 写 Flash，会卡 1~2 秒，见下面说明 */



    lv_label_set_text(lab, "已保存");

    lv_timer_t *t = lv_timer_create(save_finish_cb, 350, lab);
    lv_timer_set_repeat_count(t, 1);
}

/* 主页明火卡片：只反映 ALARM_FLAME 这一位，其他报警不影响它 */
static void fire_card_set(uint8_t on)
{
    if(on != 0U) {
        lv_obj_set_style_bg_color(g_dot_fire, COL_WARN, 0);
        lv_label_set_text(g_lab_fire_state, "检测到明火");
        lv_label_set_text(g_lab_fire_ok, "报警");
        lv_obj_set_style_text_color(g_lab_fire_ok, COL_WARN, 0);
    } else {
        lv_obj_set_style_bg_color(g_dot_fire, COL_OK, 0);
        lv_label_set_text(g_lab_fire_state, "未检测到明火");
        lv_label_set_text(g_lab_fire_ok, "正常");
        lv_obj_set_style_text_color(g_lab_fire_ok, COL_OK, 0);
    }
}

/* 报警页：按优先级挑一条显示，明火 > 传感器故障 > 温度 > 湿度。
   同时有几项异常时只显示最严重的那条，否则 240 宽塞不下 */
static void alarm_page_show(uint8_t flag,
                            uint8_t temp_int, uint8_t temp_dec,
                            uint8_t humi_int, uint8_t humi_dec)
{
    if((flag & ALARM_FLAME) != 0U)
    {
        lv_label_set_text(g_alarm_title, "检测到明火");
        lv_label_set_text(g_alarm_sub, "请立即排查火源");
    }
    else if((flag & ALARM_SENSOR) != 0U)
    {
        lv_label_set_text(g_alarm_title, "未检测到温湿度");
        lv_label_set_text(g_alarm_sub, "请检测 DHT11");
    }
    else if((flag & ALARM_TEMP) != 0U)
    {
        lv_label_set_text(g_alarm_title, "温度上限");
        lv_label_set_text_fmt(g_alarm_sub, "当前 %d.%02d°C 阈值 %d°C",
                              temp_int, temp_dec, (int)Temp_HLimit);
    }
    else
    {
        lv_label_set_text(g_alarm_title, "湿度上限");
        lv_label_set_text_fmt(g_alarm_sub, "当前 %d.%02d%% 阈值 %d%%",
                              humi_int, humi_dec, (int)Humi_HLimit);
    }
}

/* 点一下报警页就收起，但不动 alarm_flag —— 蜂鸣器继续响，
   等报警消失后重新出现才会再弹出来，避免一碰屏幕就再也看不到 */
static void alarm_dismiss_cb(lv_event_t *e)
{
    lv_obj_add_flag(page_alarm, LV_OBJ_FLAG_HIDDEN);
}

/* 传感器数据刷新。只在 LVGLTask 里调用，不要在别的任务里调 */
void myui_update(uint8_t temp_int, uint8_t temp_dec,
                 uint8_t humi_int, uint8_t humi_dec,
                 uint8_t fire_percent, uint8_t alarm_flag)
{
    static uint8_t last_fire  = 0xFFU;     /* 上次明火位，变化才刷卡片 */
    static uint8_t last_alarm = 0xFFU;     /* 上次整个报警字，变化才刷报警页 */
    uint8_t fire;

    (void)fire_percent;                    /* 界面上没有显示百分比的控件 */

    if((alarm_flag & ALARM_SENSOR) != 0U)
    {
        /* DHT11 拔了就别显示 0.0，那看起来像真的 0 度 */
        lv_label_set_text(g_lab_temp_val, "--.-");
        lv_label_set_text(g_lab_humi_val, "--.-");

        if(g_ctx_temp.cur_label != NULL) { lv_label_set_text(g_ctx_temp.cur_label, "--.-"); }
        if(g_ctx_humi.cur_label != NULL) { lv_label_set_text(g_ctx_humi.cur_label, "--.-"); }
    }
    else
    {
        lv_label_set_text_fmt(g_lab_temp_val, "%d.%02d", temp_int, temp_dec);
        lv_label_set_text_fmt(g_lab_humi_val, "%d.%02d", humi_int, humi_dec);

        /* 设置页那两个大字也显示当前值 */
        if(g_ctx_temp.cur_label != NULL)
        {
            lv_label_set_text_fmt(g_ctx_temp.cur_label, "%d.%02d", temp_int, temp_dec);
        }
        if(g_ctx_humi.cur_label != NULL)
        {
            lv_label_set_text_fmt(g_ctx_humi.cur_label, "%d.%02d", humi_int, humi_dec);
        }
    }

    fire = ((alarm_flag & ALARM_FLAME) != 0U) ? 1U : 0U;

    if(fire != last_fire)
    {
        last_fire = fire;
        fire_card_set(fire);
    }

    /* 任何一位变化都重刷报警页：温湿度超限、拔掉 DHT11 也能弹出来 */
    if(alarm_flag != last_alarm)
    {
        last_alarm = alarm_flag;

        if(alarm_flag != 0U)
        {
            alarm_page_show(alarm_flag, temp_int, temp_dec, humi_int, humi_dec);
            lv_obj_remove_flag(page_alarm, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_add_flag(page_alarm, LV_OBJ_FLAG_HIDDEN);
        }
    }
}


static lv_obj_t *card_base(lv_obj_t *parent, int32_t h)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, lv_pct(100), h);
    lv_obj_set_style_bg_color(card, COL_CARD, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

static lv_obj_t *row_base(lv_obj_t *parent,lv_flex_flow_t flow)
{
    lv_obj_t *row= lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(row, 4, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row, flow);
    return row;
}

static lv_obj_t *env_card_create (lv_obj_t *parent,const char *name,const char *value,
                                  const char *unit,lv_color_t color,lv_obj_t **p_value)
{
    //设置卡片
    lv_obj_t *card = card_base(parent, 80);
    //内部使用flex
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN,
                                LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);


    lv_obj_t *label = lv_label_create(card);
    lv_label_set_text(label, name);
    lv_obj_set_style_text_color(label, COL_DIM, 0);
    lv_obj_set_style_text_font(label, &lv_font_ui_18, 0);

    lv_obj_t *row= row_base(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
                                LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START);

    lv_obj_t *label_val= lv_label_create(row);
    lv_label_set_text(label_val, value);
    lv_obj_set_style_text_color(label_val, color, 0);
    lv_obj_set_style_text_font(label_val, &lv_font_num_32, 0);
    if(p_value)     *p_value = label_val;
    //添加单位
    lv_obj_t *label_unit= lv_label_create(row);
    lv_label_set_text(label_unit, unit);
    lv_obj_set_style_text_color(label_unit, color, 0);
    lv_obj_set_style_text_font(label_unit, &lv_font_ui_18, 0);

    return card;
}

//明火检查
static lv_obj_t *fire_card_create(lv_obj_t *parent)
{
    lv_obj_t *card = card_base(parent, 40);
    //取消边距
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_pad_hor(card, 12, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN,
                                LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *row = row_base(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_align(row,  LV_FLEX_ALIGN_START,
                                LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 6, 0);
    //设置绿色圆点
    g_dot_fire = lv_obj_create(row);
    lv_obj_set_size(g_dot_fire, 8, 8);
    lv_obj_set_style_bg_color(g_dot_fire, COL_OK, 0);
    lv_obj_set_style_radius(g_dot_fire, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(g_dot_fire, 0, 0);
    lv_obj_set_style_bg_opa(g_dot_fire, LV_OPA_COVER, 0);

    //设置标签
    g_lab_fire_state = lv_label_create(row);
    lv_label_set_text(g_lab_fire_state, "未检测到明火");
    lv_obj_set_style_text_color(g_lab_fire_state, COL_TEXT, 0);
    lv_obj_set_style_text_font(g_lab_fire_state, &lv_font_ui_18, 0);

    g_lab_fire_ok = lv_label_create(card);
    lv_label_set_text(g_lab_fire_ok, "正常");
    lv_obj_set_style_text_color(g_lab_fire_ok, COL_OK, 0);
    lv_obj_set_style_text_font(g_lab_fire_ok, &lv_font_ui_18, 0);

    /* 原来这里挂了 fire_card_cb，点一下会假装弹报警页。
       现在报警页由真实 alarm_flag 驱动，这个演示入口去掉了 */

    return card;
}

static lv_obj_t *setting_threshold_card_create(lv_obj_t *parent)
{
    lv_obj_t *card = card_base(parent, 36);
    //取消边距
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *label = lv_label_create(card);
    lv_label_set_text(label, "设置阈值");
    lv_obj_set_style_text_color(label, COL_TEXT, 0);
    lv_obj_set_style_text_font(label, &lv_font_ui_18, 0);
    lv_obj_set_align(label, LV_ALIGN_CENTER);
    //添加箭头
    lv_obj_t *arrow_label = lv_label_create(card);
    lv_label_set_text(arrow_label, ">");
    lv_obj_set_style_text_color(arrow_label, COL_DIM, 0);
    lv_obj_set_style_text_font(arrow_label, &lv_font_ui_18, 0);
    lv_obj_align(arrow_label, LV_ALIGN_RIGHT_MID, -12, 0);
    //按下时变亮
    lv_obj_set_style_bg_color(card, COL_LIGHT, LV_STATE_PRESSED);
    //添加点击事件
    lv_obj_add_event_cb(card, main_to_setting_cb, LV_EVENT_CLICKED, NULL);

    return card;
}

static lv_obj_t *page_create(lv_obj_t *parent)
{
    lv_obj_t *page = lv_obj_create(parent);
    lv_obj_set_size(page, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_radius(page, 0, 0);
    lv_obj_set_style_pad_all(page, 8, 0);
    lv_obj_set_style_pad_row(page, 8, 0);
    lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    return page;
}

static lv_obj_t *page_main_create(lv_obj_t *parent)
{
    lv_obj_t *page = page_create(parent);
    lv_obj_t *head = card_base(page, 36);
    //添加标题
    lv_obj_t *head_label = lv_label_create(head);
    lv_label_set_text(head_label, "智能网关");
    lv_obj_set_style_text_color(head_label, COL_TEXT, 0);
    lv_obj_set_style_text_font(head_label, &lv_font_ui_18, 0);
    lv_obj_set_align(head_label, LV_ALIGN_CENTER);
    lv_obj_set_style_pad_all(head_label, 0, 0);

    env_card_create(page, "温度", "25.4", "°C", COL_TEMP,&g_lab_temp_val);
    env_card_create(page, "湿度", "60.0", "%RH", COL_HUMI,&g_lab_humi_val);
    fire_card_create(page);
    setting_threshold_card_create(page);
    return page;
}

static lv_obj_t *page_setting_create(lv_obj_t *parent)
{
    lv_obj_t *page = page_create(parent);
    lv_obj_t *head_setting = card_base(page, 36);
    //添加标题
    lv_obj_t *head_label_setting = lv_label_create(head_setting);
    lv_label_set_text(head_label_setting, "设置阈值");
    lv_obj_set_style_text_color(head_label_setting, COL_TEXT, 0);
    lv_obj_set_style_text_font(head_label_setting, &lv_font_ui_18, 0);
    lv_obj_set_align(head_label_setting, LV_ALIGN_CENTER);
    lv_obj_set_style_pad_all(head_label_setting, 0, 0);
    //返回按钮
    lv_obj_t *return_label = lv_label_create(head_setting);
    lv_label_set_text(return_label, "<");
    lv_obj_set_style_text_color(return_label, COL_DIM, 0);
    lv_obj_set_style_text_font(return_label, &lv_font_ui_18, 0);
    lv_obj_align(return_label, LV_ALIGN_LEFT_MID, 12, 0);
    lv_obj_add_flag(return_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(return_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(return_label, COL_LIGHT, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(return_label, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_add_event_cb(return_label, back_to_main_cb, LV_EVENT_CLICKED, NULL);
    //保存
    lv_obj_t *save_label = lv_label_create(head_setting);
    lv_label_set_text(save_label, "保存");
    lv_obj_set_style_text_color(save_label, COL_OK, 0);
    lv_obj_set_style_text_font(save_label, &lv_font_ui_18, 0);
    lv_obj_align(save_label, LV_ALIGN_RIGHT_MID, -12, 0);
    lv_obj_add_flag(save_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(save_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(save_label, COL_LIGHT, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(save_label, LV_OPA_COVER, LV_STATE_PRESSED);
    //添加点击事件
    lv_obj_add_event_cb(save_label, save_cb, LV_EVENT_CLICKED, NULL);
    return page;
}

static lv_obj_t *page_alarm_create(lv_obj_t *parent)
{
    lv_obj_t *page = page_create(parent);
    lv_obj_set_style_bg_color(page, COL_ALARM, 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(page, 0, 0);
    lv_obj_set_style_pad_row(page, 18, 0);
    lv_obj_set_flex_align(page, LV_FLEX_ALIGN_CENTER,
                                LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    g_alarm_title = lv_label_create(page);
    lv_label_set_text(g_alarm_title, "检测到明火");
    lv_obj_set_style_text_font(g_alarm_title, &lv_font_ui_26, 0);
    lv_obj_set_style_text_color(g_alarm_title, lv_color_white(), 0);

    g_alarm_sub = lv_label_create(page);
    lv_label_set_text(g_alarm_sub, "请立即排查火源");
    lv_obj_set_style_text_font(g_alarm_sub, &lv_font_ui_18, 0);
    lv_obj_set_style_text_color(g_alarm_sub, lv_color_white(), 0);
    lv_obj_set_style_text_opa(g_alarm_sub, LV_OPA_80, 0);

    lv_obj_add_flag(page, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(page, alarm_dismiss_cb, LV_EVENT_CLICKED, NULL);

    return page;
}

static void page_switch(lv_obj_t *show, lv_obj_t *hide)
{
    lv_obj_add_flag(hide, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(show, LV_OBJ_FLAG_HIDDEN);
}



static lv_obj_t *limit_block_create(lv_obj_t *parent, const char *name,
                                    const char *cur, const char *unit,
                                    int32_t limit,int16_t max, lv_color_t color, limit_ctx_t *ctx)
{
    lv_obj_t *card = card_base(parent, 112);
    //开启flex布局
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *row = row_base(card,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    //设置标签
    lv_obj_t *name_label = lv_label_create(row);
    lv_label_set_text(name_label, name);
    lv_obj_set_style_text_color(name_label, COL_DIM, 0);
    lv_obj_set_style_text_font(name_label, &lv_font_ui_18, 0);

    lv_obj_t *lab_limit = lv_label_create(row);   /* 滑块一变就刷它 */
    lv_label_set_text_fmt(lab_limit, "阈值 %d %s", (int)limit, unit);
    lv_obj_set_style_text_font(lab_limit, &lv_font_ui_18, 0);
    lv_obj_set_style_text_color(lab_limit, color, 0);

    lv_obj_t *row2 = row_base(card,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row2, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START);
    lv_obj_t *cur_label = lv_label_create(row2);
    lv_label_set_text_fmt(cur_label, "%s", cur);
    lv_obj_set_style_text_font(cur_label, &lv_font_num_32, 0);
    lv_obj_set_style_text_color(cur_label, COL_TEXT, 0);

    lv_obj_t *label_unit = lv_label_create(row2);
    lv_label_set_text_fmt(label_unit, "%s", unit);
    lv_obj_set_style_text_font(label_unit, &lv_font_ui_18, 0);
    lv_obj_set_style_text_color(label_unit, COL_DIM, 0);

    lv_obj_t *row3 = row_base(card,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row3, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(row3, 8, 0);
    //创建-按钮
    lv_obj_t *sub_btn = lv_label_create(row3);
    lv_label_set_text(sub_btn, "-");
    lv_obj_set_style_text_color(sub_btn, COL_TEXT, 0);
    lv_obj_set_style_text_font(sub_btn, &lv_font_ui_18, 0);
    lv_obj_add_flag(sub_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(sub_btn, COL_LIGHT, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(sub_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(sub_btn, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_add_event_cb(sub_btn, step_slider_cb, LV_EVENT_CLICKED, &ctx->minus);

    //创建滑块
    lv_obj_t *slider = lv_slider_create(row3);
    lv_obj_set_height(slider, 18);
    lv_obj_set_flex_grow(slider, 1);
    lv_slider_set_range(slider, 0, max);
    lv_slider_set_value(slider, limit, LV_ANIM_OFF);
    lv_obj_set_style_pad_ver(slider, 5, LV_PART_MAIN);
    //设置滑块颜色
    lv_obj_set_style_bg_color(slider, color, LV_PART_KNOB );
    lv_obj_set_style_bg_color(slider, color, LV_PART_INDICATOR );
    lv_obj_set_style_bg_color(slider, COL_CARD, LV_PART_MAIN );
    //设置圆点大小
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB );
    lv_obj_set_style_width(slider, 16, LV_PART_KNOB);
    lv_obj_set_style_height(slider, 16, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 0, LV_PART_KNOB);

    ctx->slider = slider;
    ctx->label = lab_limit;
    ctx->cur_label = cur_label;
    ctx->unit  = unit;
    ctx->minus.slider = slider;
    ctx->minus.step   = -1;
    ctx->plus.slider  = slider;
    ctx->plus.step    = +1;

    /* 拖动滑条 / +/- 按钮都会发 VALUE_CHANGED，这个监听负责刷新「阈值 xx」 */
    lv_obj_add_event_cb(slider, limit_slider_cb, LV_EVENT_VALUE_CHANGED, ctx);
    lv_obj_send_event(slider, LV_EVENT_VALUE_CHANGED, NULL);


    //设置按钮
    lv_obj_t *add_btn = lv_label_create(row3);
    lv_label_set_text(add_btn, "+");
    lv_obj_set_style_text_color(add_btn, COL_TEXT, 0);
    lv_obj_set_style_text_font(add_btn, &lv_font_ui_18, 0);
    lv_obj_add_flag(add_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(add_btn, COL_LIGHT, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(add_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(add_btn, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_add_event_cb(add_btn, step_slider_cb, LV_EVENT_CLICKED, &ctx->plus);



    return card;
}
void myui_rewriter_init(void)
{
    //设置背景颜色
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    //真实屏幕
    lv_obj_t *root = lv_obj_create(scr);
    lv_obj_set_size(root, 240, 320);
    lv_obj_set_style_bg_color(root, COL_BG, 0);
    lv_obj_align(root, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(root,  LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(root, 0, 0);

    lv_obj_set_style_pad_all(root, 0, 0);
    //创建页面
    page_main = page_main_create(root);
    //设置页面
    page_setting = page_setting_create(root);
    lv_obj_add_flag(page_setting, LV_OBJ_FLAG_HIDDEN);
    //报警页面
    page_alarm = page_alarm_create(root);
    lv_obj_add_flag(page_alarm, LV_OBJ_FLAG_HIDDEN);

    g_thr.temp_high = (int32_t)Temp_HLimit;
    g_thr.humi_high = (int32_t)Humi_HLimit;

    lv_obj_t *limit_temp_block = limit_block_create(page_setting, "温度", "30.4", "°C",
                                                    (int32_t)Temp_HLimit, 50, COL_TEMP, &g_ctx_temp);
    lv_obj_t *limit_humi_block = limit_block_create(page_setting, "湿度", "50.0", "%",
                                                    (int32_t)Humi_HLimit, 100, COL_HUMI, &g_ctx_humi);



   

}


