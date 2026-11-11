/*
 * Copyright (c) 2026 加一公司. All rights reserved.
 *
 * 文件: weather_icon.h
 * 作者: 屈海鹏
 * 时间: 2026-11-11
 * 描述: 天气图标与动画对外接口。
 */
#ifndef WEATHER_ICON_H
#define WEATHER_ICON_H

#include "lvgl.h"

/**
 * @brief 在指定父对象上创建天气图标（含各动画层），并启动持续动画。
 * @param parent 父对象（通常是屏幕）。
 * @param x 图标左上角 X 坐标。
 * @param y 图标左上角 Y 坐标。
 */
void weather_icon_create(lv_obj_t *parent, int x, int y);

/**
 * @brief 根据天气代码切换图标显示（太阳/云/雨/雪/雷/雾）。
 * @param weather_code WMO 天气代码。
 */
void weather_icon_apply(int weather_code);

#endif /* WEATHER_ICON_H */
