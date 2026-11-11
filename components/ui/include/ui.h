/*
 * Copyright (c) 2026 加一公司. All rights reserved.
 *
 * 文件: ui.h
 * 作者: 屈海鹏
 * 时间: 2026-11-11
 * 描述: 界面构建与刷新对外接口。
 */
#ifndef UI_H
#define UI_H

#include <stdint.h>
#include "lvgl.h"
#include "app_common.h"

/**
 * @brief 在指定显示上构建完整界面。
 * @param display LVGL 显示对象。
 */
void ui_build(lv_disp_t *display);

/**
 * @brief 用最新天气数据刷新界面。
 * @param weather 天气数据（调用方保证非空且有效）。
 */
void ui_render(const weather_data_t *weather);

/**
 * @brief 更新状态提示文字与颜色（顶部第二行）。
 * @param text 提示文字。
 * @param rgb 文字颜色（24 位 RGB）。
 */
void ui_set_status(const char *text, uint32_t rgb);

/**
 * @brief 更新时钟显示（时/分/秒）。
 * @param hour 小时。
 * @param minute 分钟。
 * @param second 秒。
 */
void ui_clock_update(int hour, int minute, int second);

/**
 * @brief 在“今天”与“三天预报”页面之间切换。
 */
void ui_toggle_page(void);

#endif /* UI_H */
