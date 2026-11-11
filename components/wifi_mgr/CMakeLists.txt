# Copyright (c) 2026 加一公司. All rights reserved.
# 文件: CMakeLists.txt  作者: 屈海鹏  时间: 2026-11-11
# 描述: wifi_mgr 组件构建脚本（联网、时间同步、天气刷新服务）。

idf_component_register(SRCS "wifi_mgr.c"
                       INCLUDE_DIRS "include"
                       REQUIRES app_common weather_api
                                esp_wifi esp_event esp_netif nvs_flash)
