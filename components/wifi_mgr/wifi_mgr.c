/*
 * Copyright (c) 2026 加一公司. All rights reserved.
 *
 * 文件: wifi_mgr.c
 * 作者: 屈海鹏
 * 时间: 2026-11-11
 * 描述: Wi-Fi 连接、NTP 时间同步与天气定时刷新的实现。
 */

#include "wifi_mgr.h"
#include <stdio.h>
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "weather_api.h"

#define WIFI_CONNECTED_BIT BIT0
#define REFRESH_INTERVAL_MS (30 * 60 * 1000)

static const char *TAG = "wifi_mgr";
static EventGroupHandle_t s_events;
static TaskHandle_t s_task;
static wifi_weather_cb_t s_on_weather;
static wifi_status_cb_t s_on_status;

/**
 * @brief 向界面推送一条状态提示。
 * @param text 提示文字。
 * @param rgb 文字颜色。
 */
static void notify_status(const char *text, uint32_t rgb)
{
    if (s_on_status != NULL) {
        s_on_status(text, rgb);
    }
}

/**
 * @brief Wi-Fi/网络事件处理：断线重连、获取 IP 后置位。
 * @param arg 未使用。
 * @param base 事件基。
 * @param id 事件 ID。
 * @param data 事件数据（未使用）。
 */
static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_events, WIFI_CONNECTED_BIT);
        esp_wifi_connect();
        notify_status("Wi-Fi断开，重连中...", COLOR_ACCENT);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_events, WIFI_CONNECTED_BIT);
    }
}

/**
 * @brief 初始化并启动 Wi-Fi STA 模式，连接配置的接入点。
 * @return ESP_OK 成功，其它为错误码。
 */
static esp_err_t wifi_init_sta(void)
{
    if (strlen(CONFIG_WEATHER_WIFI_SSID) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    s_events = xEventGroupCreate();
    if (s_events == NULL) return ESP_ERR_NO_MEM;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               wifi_event_handler, NULL));

    wifi_config_t wifi_cfg = {0};
    snprintf((char *)wifi_cfg.sta.ssid, sizeof(wifi_cfg.sta.ssid), "%s",
             CONFIG_WEATHER_WIFI_SSID);
    snprintf((char *)wifi_cfg.sta.password, sizeof(wifi_cfg.sta.password), "%s",
             CONFIG_WEATHER_WIFI_PASSWORD);
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    return esp_wifi_connect();
}

/**
 * @brief 通过 NTP 同步系统时间（HTTPS 证书校验需要准确时钟）。
 * @return true 同步成功，false 失败。
 */
static bool sync_time(void)
{
    static bool sntp_ready = false;
    if (!sntp_ready) {
        esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("ntp.aliyun.com");
        sntp_ready = (esp_netif_sntp_init(&cfg) == ESP_OK);
    }
    notify_status("同步时间中...", COLOR_ACCENT);
    if (!sntp_ready) return false;
    if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(15000)) != ESP_OK) return false;
    ESP_LOGI(TAG, "NTP 时间同步完成");
    return true;
}

/**
 * @brief 后台任务：等待联网、同步时间，然后定时刷新天气并回调界面。
 * @param arg 未使用。
 */
static void network_task(void *arg)
{
    (void)arg;
    bool time_ready = false;
    for (;;) {
        if ((xEventGroupGetBits(s_events) & WIFI_CONNECTED_BIT) == 0) {
            notify_status("正在重连Wi-Fi...", COLOR_ACCENT);
            xEventGroupWaitBits(s_events, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE,
                                pdMS_TO_TICKS(15000));
            continue;
        }
        if (!time_ready) {
            time_ready = sync_time();
            if (!time_ready) {
                notify_status("时间同步失败，重试中...", COLOR_ACCENT);
                ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5000));
                continue;
            }
        }

        weather_data_t data;
        if (weather_fetch(&data)) {
            if (s_on_weather != NULL) {
                s_on_weather(&data);
            }
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(REFRESH_INTERVAL_MS));
            notify_status("刷新天气中...", COLOR_ACCENT);
        } else {
            notify_status("定位失败", COLOR_ACCENT);
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(60000));
        }
    }
}

/**
 * @brief 启动 Wi-Fi 与后台天气刷新任务。
 * @param on_weather 天气更新回调（可为 NULL）。
 * @param on_status 状态提示回调（可为 NULL）。
 * @return ESP_OK 成功，其它为错误码。
 */
esp_err_t wifi_mgr_start(wifi_weather_cb_t on_weather, wifi_status_cb_t on_status)
{
    s_on_weather = on_weather;
    s_on_status = on_status;

    esp_err_t err = wifi_init_sta();
    if (err != ESP_OK) {
        notify_status("请在menuconfig设置Wi-Fi", COLOR_ACCENT);
        return err;
    }
    xTaskCreate(network_task, "weather_net", 8192, NULL, 5, &s_task);
    return ESP_OK;
}

/**
 * @brief 请求立即刷新一次天气。
 */
void wifi_mgr_request_refresh(void)
{
    if (s_task != NULL) {
        xTaskNotifyGive(s_task);
    }
}
