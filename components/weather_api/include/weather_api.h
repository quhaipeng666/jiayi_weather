/*
 * Copyright (c) 2026 加一公司. All rights reserved.
 *
 * 文件: weather_api.h
 * 作者: 屈海鹏
 * 时间: 2026-11-11
 * 描述: 天气数据获取对外接口。
 */
#ifndef WEATHER_API_H
#define WEATHER_API_H

#include <stdbool.h>
#include "app_common.h"

/**
 * @brief 获取本地天气：先定位（IP 或手动配置），再请求天气预报。
 * @param out 输出的天气数据。
 * @return true 成功，false 失败。
 */
bool weather_fetch(weather_data_t *out);

#endif /* WEATHER_API_H */
