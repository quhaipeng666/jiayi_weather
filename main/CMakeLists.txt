# Copyright (c) 2026 加一公司. All rights reserved.
# 文件: CMakeLists.txt  作者: 屈海鹏  时间: 2026-11-11
# 描述: main 组件构建脚本（应用入口，装配各功能模块）。

idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES app_common board ui
                                     weather_icon weather_api wifi_mgr
                                     esp_lvgl_port)
