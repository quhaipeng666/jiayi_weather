/*
 * Copyright (c) 2026 加一公司. All rights reserved.
 *
 * 文件: weather_icon.c
 * 作者: 屈海鹏
 * 时间: 2026-11-11
 * 描述: 用 LVGL 图元绘制的天气图标及其分层动画实现。
 */

#include "weather_icon.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include "app_common.h"

#define ICON_SIZE 52

static const uint32_t ICON_SUN     = 0xF7A23B;  /* 橘黄色太阳 */
static const uint32_t ICON_CLOUD   = 0xD8E1E7;  /* 浅色云 */
static const uint32_t ICON_CLOUD_D = 0xAFC0C9;  /* 深色云（雷暴） */
static const uint32_t ICON_RAIN    = 0x5BA8D6;  /* 雨滴蓝 */
static const uint32_t ICON_SNOW    = 0xEEF4F7;  /* 雪花白 */
static const uint32_t ICON_BOLT    = 0xF5D442;  /* 闪电黄 */
static const uint32_t ICON_FOG     = 0xC4D2D9;  /* 雾条灰 */

static lv_obj_t *s_root;    /* 图标根容器 */
static lv_obj_t *s_fx;      /* 浮动动画容器 */
static lv_obj_t *s_rays;    /* 光芒层（旋转） */
static lv_obj_t *s_sun;     /* 太阳层（滑入云中） */
static lv_obj_t *s_cloud;   /* 云层 */
static lv_obj_t *s_rain;    /* 雨滴层 */
static lv_obj_t *s_snow;    /* 雪花层 */
static lv_obj_t *s_bolt;    /* 闪电层 */
static lv_obj_t *s_fog;     /* 雾条层 */
static bool s_sun_slide_running;

/* lv_line 只保存点数组指针而不复制，必须使用静态存储保证生命周期 */
static lv_point_precise_t s_rays_pts[16][2];
static lv_point_precise_t s_rain_pts[3][2];
static lv_point_precise_t s_bolt_pts[5];

/**
 * @brief 创建一个圆形图元。
 * @param parent 父对象。
 * @param cx 圆心 X。
 * @param cy 圆心 Y。
 * @param r 半径。
 * @param rgb 填充颜色。
 * @return 创建的圆形对象。
 */
static lv_obj_t *icon_circle(lv_obj_t *parent, int cx, int cy, int r, uint32_t rgb)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_size(o, r * 2, r * 2);
    lv_obj_set_pos(o, cx - r, cy - r);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(o, app_color(rgb), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_shadow_width(o, 0, 0);
    return o;
}

/**
 * @brief 创建一个圆角矩形图元。
 * @param parent 父对象。
 * @param x 左上角 X。
 * @param y 左上角 Y。
 * @param w 宽度。
 * @param h 高度。
 * @param rgb 填充颜色。
 * @param radius 圆角半径。
 * @return 创建的矩形对象。
 */
static lv_obj_t *icon_rect(lv_obj_t *parent, int x, int y, int w, int h,
                           uint32_t rgb, int radius)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_size(o, w, h);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_style_bg_color(o, app_color(rgb), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_shadow_width(o, 0, 0);
    return o;
}

/**
 * @brief 创建一条线段图元。
 * @param parent 父对象。
 * @param pts 点数组。
 * @param num 点数量。
 * @param rgb 线条颜色。
 * @param width 线宽。
 * @return 创建的线段对象。
 */
static lv_obj_t *icon_line(lv_obj_t *parent, const lv_point_precise_t *pts,
                           uint32_t num, uint32_t rgb, int width)
{
    lv_obj_t *o = lv_line_create(parent);
    lv_line_set_points(o, pts, num);
    lv_obj_set_style_line_color(o, app_color(rgb), 0);
    lv_obj_set_style_line_width(o, width, 0);
    lv_obj_set_style_line_rounded(o, true, 0);
    return o;
}

/**
 * @brief 以中心点为原点绘制一圈太阳光芒。
 * @param parent 父对象。
 * @param cx 中心 X。
 * @param cy 中心 Y。
 * @param r1 内半径。
 * @param r2 外半径。
 * @param count 光芒数量。
 */
static void icon_sun_rays(lv_obj_t *parent, int cx, int cy, int r1, int r2, int count)
{
    if (count > 16) count = 16;
    for (int i = 0; i < count; i++) {
        float a = i * 2.0f * 3.14159265f / (float)count;
        s_rays_pts[i][0].x = cx + r1 * cosf(a);
        s_rays_pts[i][0].y = cy + r1 * sinf(a);
        s_rays_pts[i][1].x = cx + r2 * cosf(a);
        s_rays_pts[i][1].y = cy + r2 * sinf(a);
        icon_line(parent, s_rays_pts[i], 2, ICON_SUN, 2);
    }
}

/**
 * @brief 用三个圆拼出一个云朵形状。
 * @param parent 父对象。
 * @param cx 云中心 X。
 * @param cy 云中心 Y。
 * @param rgb 云颜色。
 */
static void icon_cloud_shape(lv_obj_t *parent, int cx, int cy, uint32_t rgb)
{
    icon_circle(parent, cx, cy, 9, rgb);
    icon_circle(parent, cx - 9, cy + 3, 7, rgb);
    icon_circle(parent, cx + 9, cy + 3, 7, rgb);
}

/**
 * @brief 创建一个透明的图标图层容器（去滚动、无边框）。
 * @param parent 父对象。
 * @return 图层对象。
 */
static lv_obj_t *icon_layer(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_size(o, ICON_SIZE, ICON_SIZE);
    lv_obj_set_pos(o, 0, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_shadow_width(o, 0, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_OFF);
    return o;
}

/**
 * @brief 显示或隐藏某个图标图层。
 * @param layer 图层对象。
 * @param visible true 显示，false 隐藏。
 */
static void icon_show(lv_obj_t *layer, bool visible)
{
    if (visible) {
        lv_obj_remove_flag(layer, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(layer, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief 太阳水平滑动动画回调。
 * @param var 目标对象。
 * @param v 当前位移（像素）。
 */
static void sun_slide_cb(void *var, int32_t v)
{
    lv_obj_set_style_translate_x((lv_obj_t *)var, v, 0);
}

/**
 * @brief 启动或停止多云时太阳进出云层的滑动动画。
 * @param enable true 启动，false 停止并复位。
 */
static void icon_sun_slide(bool enable)
{
    if (enable && !s_sun_slide_running) {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, s_sun);
        lv_anim_set_exec_cb(&a, sun_slide_cb);
        lv_anim_set_values(&a, -7, 7);
        lv_anim_set_duration(&a, 2200);
        lv_anim_set_reverse_duration(&a, 2200);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a);
        s_sun_slide_running = true;
    } else if (!enable && s_sun_slide_running) {
        lv_anim_delete(s_sun, sun_slide_cb);
        lv_obj_set_style_translate_x(s_sun, 0, 0);
        s_sun_slide_running = false;
    }
}

/**
 * @brief 图层浮动动画回调（Y 方向平移）。
 * @param var 目标对象。
 * @param v 当前位移（像素）。
 */
static void icon_float_anim_cb(void *var, int32_t v)
{
    lv_obj_set_style_translate_y((lv_obj_t *)var, v, 0);
}

/**
 * @brief 图层旋转动画回调。
 * @param var 目标对象。
 * @param v 当前角度（0.1 度）。
 */
static void icon_rotate_anim_cb(void *var, int32_t v)
{
    lv_obj_set_style_transform_rotation((lv_obj_t *)var, v, 0);
}

/**
 * @brief 图层透明度动画回调。
 * @param var 目标对象。
 * @param v 当前透明度。
 */
static void icon_opa_anim_cb(void *var, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, 0);
}

/**
 * @brief 光芒伸缩动画回调：内端固定在太阳外圈，仅外端长短变化。
 * @param var 目标对象（光芒层，未使用）。
 * @param v 当前外端半径（像素）。
 */
static void icon_ray_grow_cb(void *var, int32_t v)
{
    (void)var;
    const int cx = 26;
    const int cy = 20;
    for (int i = 0; i < 16; i++) {
        float a = i * 2.0f * 3.14159265f / 16.0f;
        s_rays_pts[i][1].x = cx + v * cosf(a);
        s_rays_pts[i][1].y = cy + v * sinf(a);
        lv_line_set_points(lv_obj_get_child(s_rays, i), s_rays_pts[i], 2);
    }
}

/**
 * @brief 对对象某属性创建无限循环动画。
 * @param obj 目标对象。
 * @param cb 执行回调。
 * @param from 起始值。
 * @param to 结束值。
 * @param dur 单程时长（毫秒）。
 * @param delay 延迟（毫秒）。
 * @param reverse 是否往返播放。
 */
static void anim_loop(lv_obj_t *obj, lv_anim_exec_xcb_t cb, int32_t from,
                      int32_t to, uint32_t dur, uint32_t delay, bool reverse)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, dur);
    if (reverse) {
        lv_anim_set_reverse_duration(&a, dur);
    }
    lv_anim_set_delay(&a, delay);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

/**
 * @brief 创建全部图标图层（光芒、太阳、云、雨、雪、闪电、雾）。
 */
static void create_layers(void)
{
    s_fx = icon_layer(s_root);

    s_rays = icon_layer(s_fx);
    lv_obj_set_style_transform_pivot_x(s_rays, 26, 0);
    lv_obj_set_style_transform_pivot_y(s_rays, 20, 0);
    icon_sun_rays(s_rays, 26, 20, 11, 21, 16);

    s_sun = icon_layer(s_fx);
    icon_circle(s_sun, 26, 20, 8, ICON_SUN);

    s_cloud = icon_layer(s_fx);
    icon_cloud_shape(s_cloud, 26, 32, ICON_CLOUD);

    s_rain = icon_layer(s_fx);
    for (int i = 0; i < 3; i++) {
        int x = 16 + i * 10;
        s_rain_pts[i][0].x = x;
        s_rain_pts[i][0].y = 38;
        s_rain_pts[i][1].x = x - 3;
        s_rain_pts[i][1].y = 48;
        icon_line(s_rain, s_rain_pts[i], 2, ICON_RAIN, 2);
    }

    s_snow = icon_layer(s_fx);
    for (int i = 0; i < 3; i++) {
        icon_circle(s_snow, 16 + i * 10, 40 + (i % 2) * 4, 2, ICON_SNOW);
    }

    s_bolt = icon_layer(s_fx);
    const int bolt_xy[5][2] = {
        { 30, 34 }, { 21, 42 }, { 27, 42 }, { 20, 50 }, { 29, 50 },
    };
    for (int i = 0; i < 5; i++) {
        s_bolt_pts[i].x = bolt_xy[i][0];
        s_bolt_pts[i].y = bolt_xy[i][1];
    }
    icon_line(s_bolt, s_bolt_pts, 5, ICON_BOLT, 2);

    s_fog = icon_layer(s_fx);
    icon_rect(s_fog, 12, 40, 28, 2, ICON_FOG, 1);
    icon_rect(s_fog, 12, 46, 28, 2, ICON_FOG, 1);
}

/**
 * @brief 启动各图层的持续动画（浮动、旋转、雨雪下落、闪电闪烁、雾漂移）。
 */
static void start_animations(void)
{
    // 太阳保持固定不上下浮动，只让云朵轻微浮动
    anim_loop(s_cloud, icon_float_anim_cb, -2, 2, 1600, 0, true);
    // 光芒持续旋转 + 外端长短呼吸（内端固定）
    anim_loop(s_rays, icon_rotate_anim_cb, 0, 3600, 8000, 0, false);
    anim_loop(s_rays, icon_ray_grow_cb, 19, 26, 900, 0, true);

    for (uint32_t i = 0; i < lv_obj_get_child_count(s_rain); i++) {
        anim_loop(lv_obj_get_child(s_rain, i), icon_float_anim_cb,
                  0, 10, 700, i * 230, false);
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(s_snow); i++) {
        anim_loop(lv_obj_get_child(s_snow, i), icon_float_anim_cb,
                  0, 9, 1800, i * 400, false);
    }

    anim_loop(s_bolt, icon_opa_anim_cb, LV_OPA_COVER, LV_OPA_20, 600, 0, true);
    anim_loop(s_fog, sun_slide_cb, -3, 3, 2400, 0, true);
}

/**
 * @brief 根据天气代码显示/隐藏各图层，并同步太阳滑动与云色。
 * @param weather_code WMO 天气代码。
 */
void weather_icon_apply(int weather_code)
{
    bool clear_sky = (weather_code == 0);
    bool partly = (weather_code > 0 && weather_code <= 3);
    bool foggy = (weather_code == 45 || weather_code == 48);
    bool storm = (weather_code >= 95);
    bool snowy = (weather_code >= 71 && weather_code <= 77) ||
                 (weather_code >= 85 && weather_code <= 86);
    bool rainy = (weather_code >= 51 && weather_code <= 67) ||
                 (weather_code >= 80 && weather_code <= 82);
    bool cloudy = !clear_sky;

    icon_show(s_sun, clear_sky || partly);
    icon_show(s_rays, clear_sky);
    icon_show(s_cloud, cloudy);
    icon_show(s_rain, rainy);
    icon_show(s_snow, snowy);
    icon_show(s_bolt, storm);
    icon_show(s_fog, foggy);
    icon_sun_slide(partly);

    uint32_t cloud_rgb = storm ? ICON_CLOUD_D : ICON_CLOUD;
    for (uint32_t i = 0; i < lv_obj_get_child_count(s_cloud); i++) {
        lv_obj_set_style_bg_color(lv_obj_get_child(s_cloud, i),
                                  app_color(cloud_rgb), 0);
    }
}

/**
 * @brief 创建天气图标容器、图层与动画，并默认显示晴天。
 * @param parent 父对象（通常是屏幕）。
 * @param x 图标左上角 X 坐标。
 * @param y 图标左上角 Y 坐标。
 */
void weather_icon_create(lv_obj_t *parent, int x, int y)
{
    s_root = lv_obj_create(parent);
    lv_obj_set_size(s_root, ICON_SIZE, ICON_SIZE);
    lv_obj_set_pos(s_root, x, y);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_root, 0, 0);
    lv_obj_set_style_pad_all(s_root, 0, 0);
    lv_obj_set_style_shadow_width(s_root, 0, 0);
    lv_obj_remove_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(s_root, LV_SCROLLBAR_MODE_OFF);

    create_layers();
    weather_icon_apply(0);
    start_animations();
}
