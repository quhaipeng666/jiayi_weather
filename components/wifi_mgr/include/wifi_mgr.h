/*
 * Copyright (c) 2026 加一公司. All rights reserved.
 *
 * 文件: wifi_mgr.h
 * 作者: 屈海鹏
 * 时间: 2026-11-11
 * 描述: Wi-Fi 联网、时间同步与天气刷新服务对外接口。
 */
#ifndef WIFI_MGR_H
#define WIFI_MGR_H

#include <stdint.h>
#include "esp_err.h"
#include "app_common.h"

/* 天气数据更新回调 */
typedef void (*wifi_weather_cb_t)(const weather_data_t *weather);
/* 状态文字更新回调 */
typedef void (*wifi_status_cb_t)(const char *text, uint32_t rgb);

/**
 * @brief 启动 Wi-Fi 与后台天气刷新任务。
 * @param on_weather 天气更新回调（可为 NULL）。
 * @param on_status 状态提示回调（可为 NULL）。
 * @return ESP_OK 成功，其它为错误码。
 */
esp_err_t wifi_mgr_start(wifi_weather_cb_t on_weather, wifi_status_cb_t on_status);

/**
 * @brief 请求立即刷新一次天气。
 */
void wifi_mgr_request_refresh(void);

#endif /* WIFI_MGR_H */
