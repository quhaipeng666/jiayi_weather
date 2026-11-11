# Copyright (c) 2026 加一公司. All rights reserved.
# 文件: CMakeLists.txt  作者: 屈海鹏  时间: 2026-11-11
# 描述: 加一天气 - ESP32-C6 桌面天气站工程入口。

cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)
project(jiayi_weather)

