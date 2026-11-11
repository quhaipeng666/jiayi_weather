/*
 * Copyright (c) 2026 加一公司. All rights reserved.
 *
 * 文件: app_common.c
 * 作者: 屈海鹏
 * 时间: 2026-11-11
 * 描述: 公共辅助实现（颜色转换、天气文本、穿衣建议）。
 */

#include "app_common.h"

/**
 * @brief 将 24 位 RGB 数值转换为 LVGL 颜色对象。
 * @param rgb 形如 0xRRGGBB 的颜色值。
 * @return LVGL 颜色。
 */
lv_color_t app_color(uint32_t rgb)
{
    return lv_color_hex(rgb);
}

/**
 * @brief 返回天气代码对应的中文描述（完整）。
 * @param code WMO 天气代码。
 * @return 中文描述字符串。
 */
const char *app_weather_text(int code)
{
    if (code == 0) return "晴";
    if (code <= 3) return "多云";
    if (code == 45 || code == 48) return "雾";
    if (code <= 57) return "毛毛雨";
    if (code <= 67) return "雨";
    if (code <= 77) return "雪";
    if (code <= 82) return "阵雨";
    if (code <= 86) return "阵雪";
    if (code >= 95) return "雷暴";
    return "多云";
}

/**
 * @brief 返回天气代码对应的中文描述（简短）。
 * @param code WMO 天气代码。
 * @return 简短中文描述字符串。
 */
const char *app_weather_short_text(int code)
{
    if (code == 0) return "晴";
    if (code <= 3) return "多云";
    if (code == 45 || code == 48) return "雾";
    if (code <= 67) return "雨";
    if (code <= 77) return "雪";
    if (code <= 86) return "阵雨";
    if (code >= 95) return "雷暴";
    return "多云";
}

/**
 * @brief 根据体感温度返回穿衣建议文本。
 * @param temp 体感温度（摄氏度）。
 * @return 穿衣建议字符串。
 */
const char *app_clothing_text(int temp)
{
    if (temp <= 5) return "羽绒服，注意保暖";
    if (temp <= 12) return "厚外套 + 毛衣";
    if (temp <= 20) return "薄外套 / 长袖";
    if (temp <= 28) return "长袖 / 衬衫";
    return "短袖，注意防晒";
}
