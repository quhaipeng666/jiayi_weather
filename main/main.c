/*
 * Copyright (c) 2026 加一公司. All rights reserved.
 *
 * 文件: main.c
 * 作者: 屈海鹏
 * 时间: 2026-11-11
 * 描述: 应用入口与任务调度（时钟刷新、按键翻页、模块装配）。
 */

#include <stdlib.h>
#include <time.h>
#include "app_main.h"
#include "board.h"
#include "esp_err.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "ui.h"
#include "wifi_mgr.h"

/**
 * @brief 每秒刷新一次时钟显示（时/分/秒）。
 * @param arg 未使用。
 */
static void clock_task(void *arg)
{
    (void)arg;
    for (;;) {
        time_t now = time(NULL);
        struct tm t;
        localtime_r(&now, &t);
        ui_clock_update(t.tm_hour, t.tm_min, t.tm_sec);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/**
 * @brief 轮询物理按键，按下一次在“今天/三天”页面间切换。
 * @param arg 未使用。
 */
static void button_task(void *arg)
{
    (void)arg;
    bool last = false;
    for (;;) {
        bool pressed = board_button_pressed();
        if (pressed && !last) {
            ui_toggle_page();
        }
        last = pressed;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/**
 * @brief 应用入口：初始化存储、外设、界面与联网服务。
 */
void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(err);
    }

    setenv("TZ", "CST-8", 1);
    tzset();

    board_init_touch();
    lv_disp_t *display = board_init_display();
    if (display == NULL) {
        return;
    }

    lvgl_port_lock(0);
    ui_build(display);
    lvgl_port_unlock();

    board_init_button();
    xTaskCreate(clock_task, "clock", 3072, NULL, 4, NULL);
    xTaskCreate(button_task, "button", 2048, NULL, 5, NULL);
    wifi_mgr_start(ui_render, ui_set_status);
}
