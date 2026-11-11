/*
 * Copyright (c) 2026 加一公司. All rights reserved.
 *
 * 文件: app_common.h
 * 作者: 屈海鹏
 * 时间: 2026-11-11
 * 描述: 公共定义（天气数据结构、界面配色、中文字体与文本辅助接口）。
 */
#ifndef APP_COMMON_H
#define APP_COMMON_H

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"
#include "lv_font_cn_16.h"

/* 天气数据（当前天气 + 未来三天预报） */
typedef struct {
    char city[48];              /* 城市/区县名称 */
    char condition[32];         /* 当前天气中文描述 */
    char day_name[3][16];       /* 未来三天名称：今天/明天/后天 */
    char day_condition[3][24];  /* 未来三天天气简述 */
    int temperature;            /* 当前温度 */
    int feels_like;             /* 体感温度 */
    int humidity;               /* 相对湿度 */
    int wind;                   /* 风速 km/h */
    int weather_code;           /* WMO 天气代码 */
    int high[3];                /* 三天最高温 */
    int low[3];                 /* 三天最低温 */
    bool valid;                 /* 数据是否有效 */
} weather_data_t;

/* 界面配色（24 位 RGB） */
#define COLOR_BG        0x101D23
#define COLOR_SURFACE   0x1B2B31
#define COLOR_TEXT      0xF2F1E9
#define COLOR_MUTED     0x9BAEAD
#define COLOR_ACCENT    0xF5A66B
#define COLOR_CYAN      0x77D4D0
#define COLOR_TEMP_HIGH 0xF08A5D

/* 中文点阵字体（GB2312 全字符集，见 lv_font_cn_16.h） */

/**
 * @brief 将 24 位 RGB 数值转换为 LVGL 颜色对象。
 * @param rgb 形如 0xRRGGBB 的颜色值。
 * @return LVGL 颜色。
 */
lv_color_t app_color(uint32_t rgb);

/**
 * @brief 返回天气代码对应的中文描述（完整）。
 * @param code WMO 天气代码。
 * @return 中文描述字符串。
 */
const char *app_weather_text(int code);

/**
 * @brief 返回天气代码对应的中文描述（简短）。
 * @param code WMO 天气代码。
 * @return 简短中文描述字符串。
 */
const char *app_weather_short_text(int code);

/**
 * @brief 根据体感温度返回穿衣建议文本。
 * @param temp 体感温度（摄氏度）。
 * @return 穿衣建议字符串。
 */
const char *app_clothing_text(int temp);

#endif /* APP_COMMON_H */
