/*
 * Copyright (c) 2026 加一公司. All rights reserved.
 *
 * 文件: ui.c
 * 作者: 屈海鹏
 * 时间: 2026-11-11
 * 描述: 基于 LVGL 的界面构建与数据刷新实现。
 */

#include "ui.h"
#include "app_common.h"
#include "weather_icon.h"
#include "esp_lvgl_port.h"

static lv_obj_t *s_city;
static lv_obj_t *s_status;
static lv_obj_t *s_temperature;
static lv_obj_t *s_condition;
static lv_obj_t *s_feels;
static lv_obj_t *s_high;
static lv_obj_t *s_low;
static lv_obj_t *s_humidity;
static lv_obj_t *s_wind;
static lv_obj_t *s_clothing;
static lv_obj_t *s_today_view;
static lv_obj_t *s_forecast_view;
static lv_obj_t *s_forecast_day[3];
static lv_obj_t *s_forecast_desc[3];
static lv_obj_t *s_forecast_range[3];
static lv_obj_t *s_clock_hour;
static lv_obj_t *s_clock_minute;
static lv_obj_t *s_clock_second;
static bool s_showing_forecast;

/**
 * @brief 创建文本标签并按字号选择字体。
 * @param parent 父对象。
 * @param text 初始文本。
 * @param size 期望字号（>=32 大号、>=24 中号、其余 16px 中文）。
 * @param rgb 文字颜色。
 * @return 标签对象。
 */
static lv_obj_t *make_label(lv_obj_t *parent, const char *text, int size,
                            uint32_t rgb)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, app_color(rgb), 0);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
    lv_obj_set_style_text_letter_space(label, 0, 0);

    const lv_font_t *font = &lv_font_cn_16;
    if (size >= 32) {
        font = &lv_font_montserrat_32;
    } else if (size >= 24) {
        font = &lv_font_montserrat_24;
    }
    lv_obj_set_style_text_font(label, font, 0);
    return label;
}

/**
 * @brief 创建一个统计卡片（标题 + 数值），返回数值标签。
 * @param parent 父对象。
 * @param title 标题文字。
 * @param value 初始数值文字。
 * @param x 卡片 X 坐标。
 * @return 数值标签对象。
 */
static lv_obj_t *make_stat(lv_obj_t *parent, const char *title,
                           const char *value, int x)
{
    lv_obj_t *cell = lv_obj_create(parent);
    lv_obj_set_size(cell, 76, 68);
    lv_obj_set_pos(cell, x, 0);
    lv_obj_set_style_bg_color(cell, app_color(COLOR_SURFACE), 0);
    lv_obj_set_style_border_width(cell, 0, 0);
    lv_obj_set_style_radius(cell, 8, 0);
    lv_obj_set_style_pad_all(cell, 8, 0);
    lv_obj_set_style_shadow_width(cell, 0, 0);

    make_label(cell, title, 14, COLOR_MUTED);
    lv_obj_t *value_label = make_label(cell, value, 16, COLOR_TEXT);
    lv_obj_align(value_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    return value_label;
}

/**
 * @brief 构建顶部城市名与状态提示。
 * @param screen 屏幕对象。
 */
static void build_header(lv_obj_t *screen)
{
    s_city = make_label(screen, "正在定位...", 16, COLOR_TEXT);
    lv_obj_set_width(s_city, 156);
    lv_label_set_long_mode(s_city, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(s_city, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_city, 0, 2);

    s_status = make_label(screen, "连接Wi-Fi中...", 14, COLOR_ACCENT);
    lv_obj_set_width(s_status, 156);
    lv_obj_set_style_text_align(s_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_status, 0, 24);
}

/**
 * @brief 构建主区：天气文字、体感、高低温与时钟。
 * @param screen 屏幕对象。
 */
static void build_hero(lv_obj_t *screen)
{
    s_condition = make_label(screen, "等待天气数据", 14, COLOR_MUTED);
    lv_obj_set_pos(s_condition, 2, 103);

    s_temperature = make_label(screen, "--°", 24, COLOR_TEXT);
    lv_obj_set_pos(s_temperature, 54, 93);

    s_feels = make_label(screen, "体感 --°", 14, COLOR_MUTED);
    lv_obj_set_pos(s_feels, 2, 132);

    s_high = make_label(screen, "--°", 24, COLOR_TEMP_HIGH);
    lv_obj_set_pos(s_high, 96, 81);
    s_low = make_label(screen, "--°", 24, COLOR_CYAN);
    lv_obj_set_pos(s_low, 96, 111);

    s_clock_hour = make_label(screen, "--", 24, COLOR_TEXT);
    lv_obj_set_pos(s_clock_hour, 128, 72);
    s_clock_minute = make_label(screen, "--", 24, COLOR_TEXT);
    lv_obj_set_pos(s_clock_minute, 128, 100);
    s_clock_second = make_label(screen, "--", 24, COLOR_CYAN);
    lv_obj_set_pos(s_clock_second, 128, 128);
}

/**
 * @brief 构建“今天”页面（湿度、风速、穿衣建议）。
 * @param screen 屏幕对象。
 */
static void build_today_view(lv_obj_t *screen)
{
    s_today_view = lv_obj_create(screen);
    lv_obj_set_size(s_today_view, 156, 138);
    lv_obj_set_pos(s_today_view, 0, 164);
    lv_obj_set_style_bg_opa(s_today_view, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_today_view, 0, 0);
    lv_obj_set_style_pad_all(s_today_view, 0, 0);

    s_humidity = make_stat(s_today_view, "湿度", "--%", 0);
    s_wind = make_stat(s_today_view, "风速", "-- km/h", 80);

    lv_obj_t *cell = lv_obj_create(s_today_view);
    lv_obj_set_size(cell, 156, 60);
    lv_obj_set_pos(cell, 0, 74);
    lv_obj_set_style_bg_color(cell, app_color(COLOR_SURFACE), 0);
    lv_obj_set_style_border_width(cell, 0, 0);
    lv_obj_set_style_radius(cell, 8, 0);
    lv_obj_set_style_pad_all(cell, 8, 0);
    lv_obj_set_style_shadow_width(cell, 0, 0);

    make_label(cell, "穿衣建议", 14, COLOR_MUTED);
    s_clothing = make_label(cell, "--", 16, COLOR_ACCENT);
    lv_obj_set_width(s_clothing, 140);
    lv_label_set_long_mode(s_clothing, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_align(s_clothing, LV_ALIGN_BOTTOM_LEFT, 0, 0);
}

/**
 * @brief 构建“三天预报”页面（今天/明天/后天）。
 * @param screen 屏幕对象。
 */
static void build_forecast_view(lv_obj_t *screen)
{
    s_forecast_view = lv_obj_create(screen);
    lv_obj_set_size(s_forecast_view, 156, 138);
    lv_obj_set_pos(s_forecast_view, 0, 164);
    lv_obj_set_style_bg_opa(s_forecast_view, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_forecast_view, 0, 0);
    lv_obj_set_style_pad_all(s_forecast_view, 0, 0);

    const char *names[] = {"今天", "明天", "后天"};
    for (int i = 0; i < 3; i++) {
        int y = i * 42 + 12;
        s_forecast_day[i] = make_label(s_forecast_view, names[i], 14, COLOR_TEXT);
        lv_obj_set_pos(s_forecast_day[i], 0, y);
        s_forecast_desc[i] = make_label(s_forecast_view, "--", 14, COLOR_MUTED);
        lv_obj_set_pos(s_forecast_desc[i], 48, y);
        s_forecast_range[i] = make_label(s_forecast_view, "--/--", 14, COLOR_ACCENT);
        lv_obj_set_pos(s_forecast_range[i], 103, y);
    }
}

/**
 * @brief 切换“今天 / 三天预报”页面显示。
 * @param forecast true 显示三天预报，false 显示今天。
 */
static void show_page(bool forecast)
{
    s_showing_forecast = forecast;
    if (forecast) {
        lv_obj_add_flag(s_today_view, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_forecast_view, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(s_today_view, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_forecast_view, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief 在指定显示上构建完整界面。
 * @param display LVGL 显示对象。
 */
void ui_build(lv_disp_t *display)
{
    lv_obj_t *screen = lv_display_get_screen_active(display);
    lv_obj_set_style_bg_color(screen, app_color(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 8, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

    build_header(screen);
    weather_icon_create(screen, 0, 52);
    build_hero(screen);
    build_today_view(screen);
    build_forecast_view(screen);
    show_page(false);
}

/**
 * @brief 用最新天气数据刷新界面。
 * @param weather 天气数据（调用方保证非空且有效）。
 */
void ui_render(const weather_data_t *weather)
{
    if (!lvgl_port_lock(0)) return;
    if (weather != NULL && weather->valid) {
        lv_label_set_text(s_city, weather->city);
        lv_label_set_text_fmt(s_temperature, "%d°", weather->temperature);
        lv_label_set_text(s_condition, weather->condition);
        weather_icon_apply(weather->weather_code);
        lv_label_set_text_fmt(s_high, "%d°", weather->high[0]);
        lv_label_set_text_fmt(s_low, "%d°", weather->low[0]);
        lv_label_set_text_fmt(s_feels, "体感 %d°", weather->feels_like);
        lv_label_set_text_fmt(s_humidity, "%d%%", weather->humidity);
        lv_label_set_text_fmt(s_wind, "%d km/h", weather->wind);
        lv_label_set_text(s_clothing, app_clothing_text(weather->feels_like));

        for (int i = 0; i < 3; i++) {
            lv_label_set_text(s_forecast_day[i], weather->day_name[i]);
            lv_label_set_text(s_forecast_desc[i], weather->day_condition[i]);
            lv_label_set_text_fmt(s_forecast_range[i], "%d°/%d°",
                                  weather->high[i], weather->low[i]);
        }
        lv_label_set_text(s_status, "实时天气");
        lv_obj_set_style_text_color(s_status, app_color(COLOR_CYAN), 0);
    }
    lvgl_port_unlock();
}

/**
 * @brief 更新状态提示文字与颜色（顶部第二行）。
 * @param text 提示文字。
 * @param rgb 文字颜色（24 位 RGB）。
 */
void ui_set_status(const char *text, uint32_t rgb)
{
    if (!lvgl_port_lock(0)) return;
    if (s_status != NULL) {
        lv_label_set_text(s_status, text);
        lv_obj_set_style_text_color(s_status, app_color(rgb), 0);
    }
    lvgl_port_unlock();
}

/**
 * @brief 更新时钟显示（时/分/秒）。
 * @param hour 小时。
 * @param minute 分钟。
 * @param second 秒。
 */
void ui_clock_update(int hour, int minute, int second)
{
    if (!lvgl_port_lock(0)) return;
    lv_label_set_text_fmt(s_clock_hour, "%02d", hour);
    lv_label_set_text_fmt(s_clock_minute, "%02d", minute);
    lv_label_set_text_fmt(s_clock_second, "%02d", second);
    lvgl_port_unlock();
}

/**
 * @brief 在“今天”与“三天预报”页面之间切换。
 */
void ui_toggle_page(void)
{
    if (!lvgl_port_lock(0)) return;
    show_page(!s_showing_forecast);
    lvgl_port_unlock();
}
