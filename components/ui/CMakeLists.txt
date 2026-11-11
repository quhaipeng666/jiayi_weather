# Copyright (c) 2026 加一公司. All rights reserved.
# 文件: CMakeLists.txt  作者: 屈海鹏  时间: 2026-11-11
# 描述: ui 组件构建脚本（界面构建与刷新）。

idf_component_register(SRCS "ui.c"
                       INCLUDE_DIRS "include"
                       REQUIRES lvgl esp_lvgl_port app_common weather_icon)
