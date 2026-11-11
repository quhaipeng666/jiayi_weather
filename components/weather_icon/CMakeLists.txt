# Copyright (c) 2026 加一公司. All rights reserved.
# 文件: CMakeLists.txt  作者: 屈海鹏  时间: 2026-11-11
# 描述: weather_icon 组件构建脚本（天气图标与动画）。

idf_component_register(SRCS "weather_icon.c"
                       INCLUDE_DIRS "include"
                       REQUIRES lvgl app_common)
