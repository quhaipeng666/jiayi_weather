/*
 * Copyright (c) 2026 加一公司. All rights reserved.
 *
 * 文件: weather_api.c
 * 作者: 屈海鹏
 * 时间: 2026-11-11
 * 描述: 基于 HTTP + JSON 的定位与天气数据获取实现。
 */

#include "weather_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "sdkconfig.h"

#define HTTP_BUF_CAPACITY 6144

static const char *TAG = "weather_api";
static char s_http_buf[HTTP_BUF_CAPACITY];
static size_t s_http_len;

/**
 * @brief HTTP 客户端数据接收回调，把响应体累积到静态缓冲区。
 * @param event HTTP 事件。
 * @return 始终返回 ESP_OK。
 */
static esp_err_t http_event_cb(esp_http_client_event_t *event)
{
    if (event->event_id == HTTP_EVENT_ON_DATA && event->data_len > 0) {
        size_t remain = HTTP_BUF_CAPACITY - s_http_len - 1;
        size_t copy = (size_t)event->data_len < remain ? (size_t)event->data_len : remain;
        memcpy(s_http_buf + s_http_len, event->data, copy);
        s_http_len += copy;
        s_http_buf[s_http_len] = '\0';
    }
    return ESP_OK;
}

/**
 * @brief 发起一次 HTTP GET 请求并解析为 JSON。
 * @param url 请求地址。
 * @param root 输出 JSON 根节点（调用方负责 cJSON_Delete）。
 * @return true 成功，false 失败。
 */
static bool http_get_json(const char *url, cJSON **root)
{
    s_http_len = 0;
    s_http_buf[0] = '\0';

    esp_http_client_config_t cfg = {
        .url = url,
        .event_handler = http_event_cb,
        .timeout_ms = 15000,
        .buffer_size = 1024,
        .buffer_size_tx = 1024,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) return false;

    esp_err_t err = esp_http_client_perform(client);
    int code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (err != ESP_OK || code < 200 || code >= 300) {
        ESP_LOGW(TAG, "请求失败 (%s, HTTP %d)", esp_err_to_name(err), code);
        return false;
    }
    *root = cJSON_Parse(s_http_buf);
    return *root != NULL;
}

/**
 * @brief 按名称取 JSON 字段（区分大小写）。
 * @param parent 父节点。
 * @param name 字段名。
 * @return 字段节点，不存在返回 NULL。
 */
static cJSON *json_item(cJSON *parent, const char *name)
{
    return cJSON_GetObjectItemCaseSensitive(parent, name);
}

/**
 * @brief 取 JSON 数值字段，非数值时返回默认值。
 * @param parent 父节点。
 * @param name 字段名。
 * @param fallback 默认值。
 * @return 取到的整数或默认值。
 */
static int json_number(cJSON *parent, const char *name, int fallback)
{
    cJSON *item = json_item(parent, name);
    return cJSON_IsNumber(item) ? (int)item->valuedouble : fallback;
}

/**
 * @brief 通过公网 IP 粗略定位城市与经纬度（中文）。
 * @param city 输出城市名缓冲区。
 * @param cap 缓冲区大小。
 * @param lat 输出纬度。
 * @param lon 输出经度。
 * @return true 成功，false 失败。
 */
static bool locate_by_ip(char *city, size_t cap, double *lat, double *lon)
{
    cJSON *root = NULL;
    if (!http_get_json("http://ip-api.com/json/?lang=zh-CN", &root)) return false;

    cJSON *status = json_item(root, "status");
    cJSON *jlat = json_item(root, "lat");
    cJSON *jlon = json_item(root, "lon");
    cJSON *jcity = json_item(root, "city");
    bool ok = cJSON_IsString(status) &&
              strcmp(status->valuestring, "success") == 0 &&
              cJSON_IsNumber(jlat) && cJSON_IsNumber(jlon) &&
              cJSON_IsString(jcity);
    if (ok) {
        snprintf(city, cap, "%s", jcity->valuestring);
        *lat = jlat->valuedouble;
        *lon = jlon->valuedouble;
    }
    cJSON_Delete(root);
    return ok;
}

/**
 * @brief 反查经纬度对应的城市与区县（简体中文），拼成“城市+区县”。
 * @param city_name 输入初始城市名，输出拼接后的名称。
 * @param cap 缓冲区大小。
 * @param lat 纬度。
 * @param lon 经度。
 */
static void reverse_geocode(char *city_name, size_t cap, double lat, double lon)
{
    char url[256];
    snprintf(url, sizeof(url),
             "http://api.bigdatacloud.net/data/reverse-geocode-client"
             "?latitude=%.4f&longitude=%.4f&localityLanguage=zh-Hans", lat, lon);

    cJSON *root = NULL;
    if (!http_get_json(url, &root)) return;

    cJSON *jcity = json_item(root, "city");
    cJSON *jloc = json_item(root, "locality");
    const char *gc = cJSON_IsString(jcity) ? jcity->valuestring : NULL;
    const char *loc = cJSON_IsString(jloc) ? jloc->valuestring : NULL;
    if (loc != NULL && loc[0] != '\0') {
        if (gc != NULL && gc[0] != '\0' && strcmp(gc, loc) != 0) {
            snprintf(city_name, cap, "%s%s", gc, loc);
        } else {
            snprintf(city_name, cap, "%s", loc);
        }
    } else if (gc != NULL && gc[0] != '\0') {
        snprintf(city_name, cap, "%s", gc);
    }
    cJSON_Delete(root);
}

/**
 * @brief 请求 Open-Meteo 天气预报 JSON。
 * @param lat 纬度。
 * @param lon 经度。
 * @param root 输出 JSON 根节点。
 * @return true 成功，false 失败。
 */
static bool request_forecast(double lat, double lon, cJSON **root)
{
    char url[512];
    snprintf(url, sizeof(url),
             "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
             "&current=temperature_2m,relative_humidity_2m,apparent_temperature,"
             "weather_code,wind_speed_10m"
             "&daily=weather_code,temperature_2m_max,temperature_2m_min"
             "&forecast_days=3&timezone=auto", lat, lon);
    return http_get_json(url, root);
}

/**
 * @brief 从预报 JSON 中解析当前天气与未来三天数据。
 * @param root 预报 JSON 根节点。
 * @param out 输出天气数据。
 * @return true 成功，false 字段缺失。
 */
static bool parse_forecast(cJSON *root, weather_data_t *out)
{
    cJSON *current = json_item(root, "current");
    cJSON *daily = json_item(root, "daily");
    if (current == NULL || daily == NULL) return false;

    out->temperature = json_number(current, "temperature_2m", 0);
    out->feels_like = json_number(current, "apparent_temperature", 0);
    out->humidity = json_number(current, "relative_humidity_2m", 0);
    out->wind = json_number(current, "wind_speed_10m", 0);
    out->weather_code = json_number(current, "weather_code", -1);
    snprintf(out->condition, sizeof(out->condition), "%s",
             app_weather_text(out->weather_code));

    cJSON *codes = json_item(daily, "weather_code");
    cJSON *highs = json_item(daily, "temperature_2m_max");
    cJSON *lows = json_item(daily, "temperature_2m_min");
    if (!cJSON_IsArray(codes) || !cJSON_IsArray(highs) || !cJSON_IsArray(lows)) {
        return false;
    }

    const char *names[] = {"今天", "明天", "后天"};
    for (int i = 0; i < 3; i++) {
        cJSON *code = cJSON_GetArrayItem(codes, i);
        cJSON *high = cJSON_GetArrayItem(highs, i);
        cJSON *low = cJSON_GetArrayItem(lows, i);
        snprintf(out->day_name[i], sizeof(out->day_name[i]), "%s", names[i]);
        out->high[i] = cJSON_IsNumber(high) ? (int)high->valuedouble : 0;
        out->low[i] = cJSON_IsNumber(low) ? (int)low->valuedouble : 0;
        snprintf(out->day_condition[i], sizeof(out->day_condition[i]), "%s",
                 app_weather_short_text(cJSON_IsNumber(code) ?
                                        (int)code->valuedouble : -1));
    }
    return true;
}

/**
 * @brief 确定定位信息：优先使用手动配置，否则用 IP 定位 + 反查区县。
 * @param city 输出城市名称。
 * @param cap 缓冲区大小。
 * @param lat 输出纬度。
 * @param lon 输出经度。
 * @return true 成功，false 失败。
 */
static bool resolve_location(char *city, size_t cap, double *lat, double *lon)
{
    if (strlen(CONFIG_WEATHER_LAT) > 0 && strlen(CONFIG_WEATHER_LON) > 0) {
        *lat = atof(CONFIG_WEATHER_LAT);
        *lon = atof(CONFIG_WEATHER_LON);
        snprintf(city, cap, "%s", CONFIG_WEATHER_LOCATION_NAME);
        return true;
    }
    if (!locate_by_ip(city, cap, lat, lon)) return false;
    reverse_geocode(city, cap, *lat, *lon);
    return true;
}

/**
 * @brief 获取本地天气：定位后请求预报并解析。
 * @param out 输出的天气数据。
 * @return true 成功，false 失败。
 */
bool weather_fetch(weather_data_t *out)
{
    if (out == NULL) return false;

    char city[sizeof(out->city)];
    city[0] = '\0';
    double lat = 0;
    double lon = 0;
    if (!resolve_location(city, sizeof(city), &lat, &lon)) return false;

    cJSON *root = NULL;
    if (!request_forecast(lat, lon, &root)) return false;

    weather_data_t data = {0};
    snprintf(data.city, sizeof(data.city), "%s", city);
    bool ok = parse_forecast(root, &data);
    cJSON_Delete(root);
    if (!ok) return false;

    data.valid = true;
    *out = data;
    ESP_LOGI(TAG, "天气已更新");
    return true;
}
