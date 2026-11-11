# 加一天气 · ESP32-C6 桌面天气站

基于 **ESP32-C6 + 1.47 英寸 ST7789 液晶屏**的桌面天气站。设备联网后自动定位并获取本地天气，
提供中文界面、动画天气图标与实时时钟，通过板载 BOOT 键切换页面。

- 工程名：`jiayi_weather`（固件产物 `build/jiayi_weather.bin`）
- 目标芯片：ESP32-C6（4MB Flash 起步，本项目按 8MB 配置）
- 开发框架：ESP-IDF v6.1 + LVGL v9.5.0

---

## 一、硬件

### 1.1 硬件清单

| 部件 | 规格 |
|------|------|
| 主控 | ESP32-C6（RISC-V 单核 160MHz，Wi-Fi 6 / BLE） |
| 显示屏 | 1.47 英寸 IPS，172×320，ST7789 驱动，SPI 接口 |
| 触摸 | AXS5106L 电容触摸（I²C，本工程未启用交互） |
| 按键 | BOOT 键 ×1（低电平有效），用于切换页面 |
| 背光 | GPIO22 PWM 调光（默认 30%，降低功耗与发热） |
| 存储 | Flash 8MB（应用分区 4MB） |

### 1.2 引脚定义

| 功能 | GPIO | 说明 |
|------|------|------|
| LCD SCLK | 7 | SPI2 时钟 |
| LCD MOSI | 6 | SPI2 数据 |
| LCD CS | 14 | 片选 |
| LCD DC | 15 | 数据/命令选择 |
| LCD RST | 21 | 复位 |
| LCD BL | 22 | 背光，LEDC PWM |
| 触摸 SDA | 18 | I²C |
| 触摸 SCL | 19 | I²C |
| 触摸 RST | 20 | 复位 |
| 按键 | 9 | BOOT，内部上拉，按下为低 |

### 1.3 硬件连接框图

```
                ┌──────────────────────────────┐
                │           ESP32-C6           │
                │                              │
   SPI2 ────────┤ GPIO7(SCLK) GPIO6(MOSI)      │
   LCD  ────────┤ GPIO14(CS) GPIO15(DC)        │
                │ GPIO21(RST)                  │
   PWM  ────────┤ GPIO22(BL) ──▶ 背光          │
                │                              │
   I²C  ────────┤ GPIO18(SDA) GPIO19(SCL)      │──▶ AXS5106L 触摸
                │ GPIO20(触摸复位)              │
                │                              │
   按键 ────────┤ GPIO9(BOOT)                  │
                │                              │
                │   Wi-Fi 2.4GHz ──▶ 互联网     │
                └──────────────────────────────┘
```

---

## 二、软件架构

### 2.1 分层设计

工程按职责拆分为 6 个独立组件，依赖单向、界面与网络解耦：

```
┌───────────────────────────────────────────────────────────┐
│                        main                              │
│        应用入口：装配模块、时钟任务、按键任务              │
└───────┬───────────────┬───────────────────┬──────────────┘
        │               │                   │
        ▼               ▼                   ▼
┌───────────────┐ ┌─────────────┐ ┌──────────────────────┐
│    board      │ │     ui      │ │      wifi_mgr        │
│ LCD/触摸/按键  │ │ 界面构建刷新 │ │ WiFi/NTP/定时刷新     │
│ /背光 PWM     │ └──────┬──────┘ └──────────┬───────────┘
└───────────────┘        │                   │
                         ▼                   ▼
                 ┌───────────────┐   ┌──────────────────┐
                 │ weather_icon  │   │   weather_api    │
                 │ 图标绘制/动画  │   │ 定位+天气获取解析 │
                 └───────┬───────┘   └────────┬─────────┘
                         │                    │
                         └────────┬───────────┘
                                  ▼
                       ┌─────────────────────┐
                       │     app_common      │
                       │ 数据结构/配色/字体/  │
                       │ 天气文本/穿衣建议    │
                       └─────────────────────┘
```

### 2.2 组件职责

| 组件 | 职责 | 主要接口 |
|------|------|----------|
| `app_common` | 天气数据结构、配色常量、GB2312 中文字体、天气文案 | `app_color` / `app_weather_text` / `app_clothing_text` |
| `board` | LCD 面板、LVGL 端口、触摸 I²C、按键、背光 PWM | `board_init_display` / `board_init_touch` / `board_button_pressed` |
| `ui` | 界面构建、数据刷新、页面切换、时钟更新 | `ui_build` / `ui_render` / `ui_toggle_page` / `ui_clock_update` |
| `weather_icon` | 用 LVGL 图元绘制天气图标并实现分层动画 | `weather_icon_create` / `weather_icon_apply` |
| `weather_api` | IP 定位、反向地理编码、天气请求与 JSON 解析 | `weather_fetch` |
| `wifi_mgr` | Wi-Fi 连接与重连、NTP 对时、定时刷新服务 | `wifi_mgr_start` / `wifi_mgr_request_refresh` |

> 设计要点：`wifi_mgr` 不直接依赖 `ui`，通过回调注入（`ui_render` / `ui_set_status`），
> 网络层与界面层可独立替换与测试。

### 2.3 数据流

```
上电
 │
 ├─ board_init_touch()      初始化触摸 I²C（预留）
 ├─ board_init_display()    LCD → LVGL → 背光
 ├─ ui_build()              构建界面（含图标动画）
 │
 └─ wifi_mgr_start()
      │
      ├─ 连接 Wi-Fi ──┬─ 断线自动重连
      │               └─ 获得 IP
      ├─ NTP 对时（ntp.aliyun.com）
      │
      └─ weather_fetch()
           ├─ 手动配置经纬度？ → 直接用
           ├─ 否则 ip-api.com 定位城市 + 经纬度
           ├─ bigdatacloud.net 反查区县（简体中文）
           └─ open-meteo.com 获取当前 + 三天预报
                 │
                 └─ 回调 ui_render() 刷新界面
                       （每 30 分钟刷新一次）
```

### 2.4 任务模型

| 任务 | 周期/触发 | 职责 |
|------|-----------|------|
| LVGL 任务 | 框架内部 | 界面渲染与动画 |
| `weather_net` | 事件驱动 | Wi-Fi、NTP、天气抓取 |
| `clock` | 200ms | 读取系统时间刷新时/分/秒 |
| `button` | 20ms 轮询 | BOOT 键消抖并切换页面 |

### 2.5 界面布局（172×320）

```
┌────────────────────────────┐
│       西安市雁塔区          │  城市（居中）
│        实时天气             │  状态
│  ┌────┐ 多云   ↑28° [时]   │  图标 / 天气 / 高低温 / 时钟
│  │图标│ 23°    ↓15° [分]   │
│  └────┘ 体感 20°      [秒]  │
├────────────────────────────┤
│ ┌────────┐ ┌────────┐      │  湿度 / 风速卡片
│ ├────────┴─┴────────┤      │
│ │      穿衣建议       │      │  滚动文字
│ └────────────────────┘      │
└────────────────────────────┘
        （BOOT 键切换三天预报页）
```

---

## 三、目录结构

```
.
├── CMakeLists.txt             工程入口（project: jiayi_weather）
├── partitions.csv             自定义分区表（应用 4MB）
├── sdkconfig.defaults         默认配置（编译参数、字体、分区表）
├── run_idf.py                 Git Bash 环境下的 idf.py 包装脚本
├── components/
│   ├── app_common/            公共定义 + 中文点阵字体
│   ├── board/                 板级外设驱动
│   ├── ui/                    界面层
│   ├── weather_icon/          天气图标与动画
│   ├── weather_api/           定位与天气数据
│   └── wifi_mgr/              联网与刷新服务
└── main/                      应用入口
```

每个组件包含 `include/<name>.h`（对外接口）与 `<name>.c`（实现），文件头含版权与作者信息。

---

## 四、编译

### 4.1 环境要求

- ESP-IDF **v6.1**
- 目标芯片 `esp32c6`
- Python 3.13（ESP-IDF 自带虚拟环境）

### 4.2 命令行编译（Windows / ESP-IDF 终端）

```bash
idf.py set-target esp32c6      # 首次
idf.py menuconfig              # 配置 Wi-Fi 与定位（见第五节）
idf.py build
idf.py -p COM3 flash
idf.py -p COM3 monitor
```

### 4.3 Git Bash / MSYS2 环境

MSYS2 会向子进程注入 `MSYSTEM`，导致 `idf.py` 直接跳过构建。使用仓库内脚本：

```bash
export IDF_PATH=D:/esp/.espressif/v6.1/esp-idf
export IDF_TOOLS_PATH=C:/Espressif/tools
export IDF_PYTHON_ENV_PATH=$IDF_TOOLS_PATH/python/v6.1/venv
python run_idf.py build
python run_idf.py -p COM3 flash
```

### 4.4 VS Code（ESP-IDF 插件）

底部工具栏依次点击 **Build → Flash → Monitor** 即可。

### 4.5 依赖

第三方组件由 IDF 组件管理器自动下载（见 `main/idf_component.yml`、`dependencies.lock`）：

| 组件 | 版本 | 用途 |
|------|------|------|
| `espressif/esp_lvgl_port` | 2.9.0 | LVGL 与 ESP-IDF 对接 |
| `lvgl/lvgl` | 9.5.0 | 图形库（版本已锁定） |
| `jbrilha/esp_lcd_st7789` | 1.0.2 | ST7789 面板驱动 |
| `espressif/cjson` | 1.7.19 | JSON 解析 |

---

## 五、配置

执行 `idf.py menuconfig` → **Weather station**：

| 配置项 | 说明 | 示例 |
|--------|------|------|
| `WEATHER_WIFI_SSID` | 2.4GHz Wi-Fi 名称 | `MyWiFi` |
| `WEATHER_WIFI_PASSWORD` | Wi-Fi 密码 | `********` |
| `WEATHER_LOCATION_NAME` | 界面显示的地名 | `西安市雁塔区` |
| `WEATHER_LAT` / `WEATHER_LON` | 天气查询经纬度 | `34.2225` / `108.9480` |

> **定位说明**：IP 定位依赖运营商出口 IP，通常只能到城市级（区县字段常为空）。
> 建议手动填写 `WEATHER_LOCATION_NAME` 与经纬度，以获得准确的区县名称与天气。
> 若经纬度留空，则自动使用 IP 定位。

> **安全提示**：Wi-Fi 凭据保存在本地 `sdkconfig`（已被 `.gitignore` 排除），请勿提交到公开仓库。

---

## 六、使用说明

1. 上电后自动连接 Wi-Fi、同步时间，约 5–10 秒后显示天气。
2. 主页面显示：城市、天气图标、当前温度、天气描述、体感、最高/最低温、实时时钟。
3. 下方卡片显示湿度、风速与穿衣建议（文字过长自动横向滚动）。
4. 按**左侧 BOOT 键**在「今天」与「三天预报」之间切换。
5. 天气每 30 分钟自动刷新；网络断开会自动重连并重新抓取。

### 天气图标动画

| 天气 | 动画效果 |
|------|----------|
| 晴 | 光芒绕太阳旋转，外端长短呼吸 |
| 多云 | 太阳在云后左右滑入滑出 |
| 雨 | 雨滴错峰下落 |
| 雪 | 雪花飘落 |
| 雷暴 | 深色云 + 闪电闪烁 |
| 雾 | 雾条左右漂移 |

---

## 七、常见问题

| 现象 | 排查方向 |
|------|----------|
| 屏幕只有背光、无显示 | 核对引脚（SCLK 7 / MOSI 6 / CS 14 / DC 15 / RST 21）；确认 SPI 模式为 0 |
| 颜色异常（如橘色变蓝） | 面板 RGB/BGR 顺序：`LCD_RGB_ELEMENT_ORDER_RGB`，必要时切换为 BGR |
| 天气不更新 | 检查 Wi-Fi 凭据；确认能访问 `ip-api.com` 与 `api.open-meteo.com` |
| 时间不对 | 需先联网完成 NTP 同步；时区固定为 UTC+8 |
| 烧录报 `PermissionError` | 降低波特率或改用 460800；关闭占用 COM 口的串口工具 |
| 中文显示为方块 | 字库为 GB2312 子集，个别生僻字需重新生成字体 |

---

## 八、版权

Copyright (c) 2026 加一公司. All rights reserved.

作者：屈海鹏
