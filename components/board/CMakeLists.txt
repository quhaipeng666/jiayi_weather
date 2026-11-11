# Copyright (c) 2026 加一公司. All rights reserved.
# 文件: CMakeLists.txt  作者: 屈海鹏  时间: 2026-11-11
# 描述: board 组件构建脚本（显示屏、触摸、按键、背光）。

idf_component_register(SRCS "board.c"
                       INCLUDE_DIRS "include"
                       REQUIRES lvgl esp_lvgl_port esp_lcd_st7789
                                esp_lcd driver esp_driver_gpio
                                esp_driver_i2c esp_driver_spi esp_driver_ledc)
