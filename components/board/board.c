/*
 * Copyright (c) 2026 加一公司. All rights reserved.
 *
 * 文件: board.c
 * 作者: 屈海鹏
 * 时间: 2026-11-11
 * 描述: 板级外设初始化实现（ST7789 显示屏、AXS5106 触摸、BOOT 按键、背光）。
 */

#include "board.h"
#include <stdint.h>
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7789.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LCD_H_RES 172
#define LCD_V_RES 320
#define LCD_COL_OFFSET 34
#define LCD_BRIGHTNESS_PERCENT 30
#define BOARD_BTN_GPIO GPIO_NUM_9

static const char *TAG = "board";
static i2c_master_dev_handle_t s_touch_dev;

/**
 * @brief 初始化背光 PWM，按设定亮度点亮背光。
 */
static void backlight_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = GPIO_NUM_22,
        .duty = 255 * LCD_BRIGHTNESS_PERCENT / 100,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));
}

/**
 * @brief 初始化 LCD 使用的 SPI2 总线。
 */
static void spi_bus_init(void)
{
    spi_bus_config_t bus = {
        .sclk_io_num = GPIO_NUM_7,
        .mosi_io_num = GPIO_NUM_6,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 40 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
}

/**
 * @brief 创建并初始化 ST7789 面板（含偏移、反色、开显示）。
 * @param io_out 输出 LCD IO 句柄。
 * @param panel_out 输出面板句柄。
 */
static void panel_init(esp_lcd_panel_io_handle_t *io_out,
                       esp_lcd_panel_handle_t *panel_out)
{
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = GPIO_NUM_14,
        .dc_gpio_num = GPIO_NUM_15,
        .spi_mode = 0,
        .pclk_hz = 40 * 1000 * 1000,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST,
                                             &io_cfg, io_out));

    esp_lcd_panel_dev_config_t dev = {
        .reset_gpio_num = GPIO_NUM_21,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(*io_out, &dev, panel_out));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(*panel_out));
    ESP_ERROR_CHECK(esp_lcd_panel_init(*panel_out));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(*panel_out, LCD_COL_OFFSET, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(*panel_out, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(*panel_out, true));
}

/**
 * @brief LVGL 输入设备读取回调，按 AXS5106 协议读取触摸坐标。
 * @param input 输入设备对象（未使用）。
 * @param data 输出的触摸状态与坐标。
 */
static void touch_read_cb(lv_indev_t *input, lv_indev_data_t *data)
{
    (void)input;
    static const uint8_t cmd[] = {0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00,
                                  0x00, 0x0E, 0x00, 0x00, 0x00};
    uint8_t resp[14] = {0};
    esp_err_t ret = i2c_master_transmit_receive(s_touch_dev, cmd, sizeof(cmd),
                                                resp, sizeof(resp), 20);
    if (ret != ESP_OK || resp[0] == 0xFF || resp[1] == 0) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    int x = ((resp[2] & 0x0F) << 8) | resp[3];
    int y = ((resp[4] & 0x0F) << 8) | resp[5];
    data->point.x = (x < LCD_H_RES) ? x : LCD_H_RES - 1;
    data->point.y = (y < LCD_V_RES) ? y : LCD_V_RES - 1;
    data->state = LV_INDEV_STATE_PRESSED;
}

/**
 * @brief 初始化 LVGL 端口、注册显示与触摸输入设备。
 * @param io LCD IO 句柄。
 * @param panel 面板句柄。
 * @return 成功返回 LVGL 显示对象，失败返回 NULL。
 */
static lv_disp_t *lvgl_port_setup(esp_lcd_panel_io_handle_t io,
                                  esp_lcd_panel_handle_t panel)
{
    const lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&port_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io,
        .panel_handle = panel,
        .buffer_size = LCD_H_RES * 40,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {.swap_xy = false, .mirror_x = false, .mirror_y = false},
        .flags = {.buff_dma = true, .swap_bytes = true},
    };
    lv_disp_t *display = lvgl_port_add_disp(&disp_cfg);
    if (display == NULL) {
        ESP_LOGE(TAG, "显示注册失败");
        return NULL;
    }

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(indev, display);
    lv_indev_set_read_cb(indev, touch_read_cb);
    return display;
}

/**
 * @brief 初始化背光、SPI、面板与 LVGL 显示端口。
 * @return 成功返回 LVGL 显示对象，失败返回 NULL。
 */
lv_disp_t *board_init_display(void)
{
    backlight_init();
    spi_bus_init();

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_handle_t panel = NULL;
    panel_init(&io, &panel);
    return lvgl_port_setup(io, panel);
}

/**
 * @brief 初始化触摸控制器所依赖的 I2C 总线与复位引脚。
 * @return ESP_OK 成功，其它为错误码。
 */
esp_err_t board_init_touch(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPIO_NUM_18,
        .scl_io_num = GPIO_NUM_19,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &bus), TAG, "I2C 总线初始化失败");

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x3B,
        .scl_speed_hz = 400000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &dev_cfg, &s_touch_dev),
                        TAG, "触摸设备添加失败");

    ESP_RETURN_ON_ERROR(gpio_set_direction(GPIO_NUM_20, GPIO_MODE_OUTPUT), TAG,
                        "触摸复位引脚配置失败");
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_20, 0));
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_20, 1));
    vTaskDelay(pdMS_TO_TICKS(300));
    return ESP_OK;
}

/**
 * @brief 初始化左侧物理按键（BOOT，低电平有效，内部上拉）。
 * @return ESP_OK 成功，其它为错误码。
 */
esp_err_t board_init_button(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << BOARD_BTN_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return gpio_config(&cfg);
}

/**
 * @brief 读取物理按键当前是否被按下。
 * @return true 表示按下。
 */
bool board_button_pressed(void)
{
    return gpio_get_level(BOARD_BTN_GPIO) == 0;
}
