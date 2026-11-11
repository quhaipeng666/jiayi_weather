# Copyright (c) 2026 加一公司. All rights reserved.
# 文件: CMakeLists.txt  作者: 屈海鹏  时间: 2026-11-11
# 描述: app_common 组件构建脚本（公共定义与中文字体）。

idf_component_register(SRCS "app_common.c" "lv_font_cn_16.c"
                       INCLUDE_DIRS "include"
                       REQUIRES lvgl)
