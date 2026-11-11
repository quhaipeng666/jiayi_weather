/*
 * Copyright (c) 2026 加一公司. All rights reserved.
 *
 * 文件: board.h
 * 作者: 屈海鹏
 * 时间: 2026-11-11
 * 描述: 板级外设对外接口（显示屏、触摸、按键、背光）。
 */
#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>
#include "esp_err.h"
#include "lvgl.h"

/**
 * @brief 初始化背光、SPI 总线、LCD 面板与 LVGL 显示端口。
 * @return 成功返回 LVGL 显示对象，失败返回 NULL。
 */
lv_disp_t *board_init_display(void);

/**
 * @brief 初始化触摸控制器所依赖的 I2C 总线与复位引脚。
 * @return ESP_OK 成功，其它为错误码。
 */
esp_err_t board_init_touch(void);

/**
 * @brief 初始化左侧物理按键（BOOT，低电平有效）。
 * @return ESP_OK 成功，其它为错误码。
 */
esp_err_t board_init_button(void);

/**
 * @brief 读取物理按键当前是否被按下。
 * @return true 表示按下。
 */
bool board_button_pressed(void);

#endif /* BOARD_H */
