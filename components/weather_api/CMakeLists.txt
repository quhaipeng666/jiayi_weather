# Copyright (c) 2026 加一公司. All rights reserved.
# 文件: CMakeLists.txt  作者: 屈海鹏  时间: 2026-11-11
# 描述: weather_api 组件构建脚本（定位与天气数据获取）。

idf_component_register(SRCS "weather_api.c"
                       INCLUDE_DIRS "include"
                       REQUIRES app_common esp_http_client esp_event cjson)
