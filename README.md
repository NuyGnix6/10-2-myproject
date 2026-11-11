# 环境监测与告警终端

基于 **STM32F407VGT6 + FreeRTOS + LVGL 9** 的环境监测终端。2.4 寸电容触摸屏本机显示温湿度与明火状态，超限触发蜂鸣器与全屏告警页，数据经 WiFi 上报 MQTT 云端，阈值支持触摸与串口双通道修改且断电不丢。

---

## 功能特性

- **图形界面**：LVGL 9 移植，三个页面（主页 / 阈值设置 / 全屏告警），局部刷新 + DMA 传输
- **触摸交互**：FT6336U 电容触摸，支持点按与滑条拖动
- **多传感器采集**：DHT11 温湿度、火焰传感器（ADC）
- **报警仲裁**：明火 > 传感器故障 > 温度 > 湿度，四项报警共用同一个告警页，按优先级显示
- **迟滞判定**：高于阈值置位、低于阈值解除、相等时保持，避免阈值附近反复抖动
- **参数掉电保护**：3 组阈值写 Flash Sector 11，上电比对标志字，校验失败回落默认值
- **双通道配置**：串口命令 `T/H/F` 与触摸屏滑条都能改阈值
- **数据上云**：ESP8266 走 AT 指令接入 WiFi，手工拼装 MQTT 报文（不依赖协议库）
- **故障自检**：DHT11 连续 3 次读取失败判定掉线，界面显示 `--.-`；ADC 采样全失败沿用上次有效值

## 硬件清单

| 部件 | 型号 | 接口 |
|---|---|---|
| 主控 | STM32F407VGT6（LQFP100，1MB Flash / 192KB RAM） | — |
| 显示屏 | ILI9341，2.4 寸，240×320，SPI | SPI1 + DMA2 |
| 触摸 | FT6336U 电容触摸（与屏同排线，经转接板） | 软件 I2C |
| 温湿度 | DHT11 | 单总线 |
| 火焰 | 火焰传感器模块（AO 模拟输出） | ADC1_IN0 |
| 无线 | ESP8266（ESP-AT 固件） | USART2 |
| 提示 | 有源蜂鸣器 | GPIO |
| 按键 | 3 个轻触按键（设置 / 加 / 减） | GPIO 上拉 |

## 引脚分配

| 引脚 | 功能 | 标签 | 说明 |
|---|---|---|---|
| PA1 | TIM2_CH2 | LCD_BL | 背光 PWM，10 kHz，默认占空比 80% |
| PA4 | GPIO_Out | LCD_CS | 片选，低有效 |
| PA5 | SPI1_SCK | — | AF5 |
| PA7 | SPI1_MOSI | — | AF5 |
| PB0 | GPIO_Out | LCD_DC | 命令/数据选择 |
| PB1 | GPIO_Out | LCD_RST | 复位，高有效 |
| PC0 | GPIO_Out | — | 触摸复位（T_CS） |
| PC1 | GPIO_In | T_IRQ | 触摸中断，上拉 |
| PC2 | GPIO_Out | T_CLK | 触摸 I2C SCL（软件模拟） |
| PC3 | GPIO_Out | T_SDI | 触摸 I2C SDA（软件模拟） |
| PC4 | GPIO_In | — | 触摸 T_SDO，上拉 |
| PA0 | ADC1_IN0 | — | 火焰传感器 AO |
| PA9 / PA10 | USART1 | — | 调试口 / 命令口，115200 8N1 |
| PA2 / PA3 | USART2 | — | ESP8266，115200 8N1 |
| PB12 | GPIO | DHT11 | 单总线，初始高 |
| PB13 | GPIO | Buzzer | 低电平响 |
| PB14 | GPIO_In | Key_Set | 上拉，低有效 |
| PB15 | GPIO_In | Key_Down | 上拉，低有效 |
| PC6 | GPIO_In | Key_Up | 上拉，低有效 |

> 触摸与显示共用一条 10~14 脚的排线。电容屏下 `T_CLK=SCL`、`T_CS=RST`、`T_SDI=SDA`、`T_SDO` 悬空、`T_IRQ=INT`，这是转接板手册里的映射，接错会导致读不到触摸芯片。

## 软件架构

### 任务划分

| 任务 | 优先级 | 栈（字） | 周期 | 职责 |
|---|---|---|---|---|
| `UartTask` | 4 | 256 | 5 s | ESP8266 连接、AT 指令、MQTT 上报 |
| `KeyTask` | 2 | 128 | 20 ms | 按键扫描，按页面调阈值 |
| `AlarmTask` | 2 | 128 | 100 ms | 阈值比较、迟滞判定、蜂鸣器控制 |
| `LVGLTask` | 2 | 3072 | 5 ms | `lv_timer_handler`、界面刷新（数据 200 ms 一次） |
| `DHT11Task` | 1 | 128 | 2 s | 温湿度采集、故障计数 |
| `LightTask` | 1 | 128 | 500 ms | 火焰 ADC 采样（10 次平均） |
| `CMDTask` | 1 | 128 | 10 ms | 串口命令解析 |

### 数据流

```
DHT11Task ──┐                    ┌──> AlarmTask ──> alarm_flag / Buzzer
            ├─> 队列（长度 1，    │
LightTask ──┘   覆盖写入）        └──> LVGLTask ──> myui_update() ──> 屏幕
                                        ▲
CMDTask ──> Store_Data[1..3] ───────────┘
KeyTask ──> Store_Data[1..3]
                 │
                 └──> MyFlash_Save() ──> Sector 11
```

采集任务用 `xQueueOverwrite` 写长度为 1 的队列，读端只取最新一帧，不做无谓的临界区开销。`alarm_flag` 由 AlarmTask 单点写入，读端不会读到半截。

### 目录结构

```
10-2 myproject/
├── Core/
│   ├── Inc/                     CubeMX 生成的头文件
│   └── Src/
│       ├── main.c               时钟、外设初始化、任务创建
│       ├── freertos.c           CubeMX 的 RTOS 初始化壳
│       └── ui/
│           ├── myui_rewriter.c  LVGL 界面（页面、卡片、回调、刷新）
│           └── Font/            按需裁剪的位图字体（18 / 26 / 32 px）
├── Hardware/
│   ├── bsp_ili9341.c            屏驱动：初始化序列、开窗、填充
│   ├── lv_port_disp_v9.c        LVGL 9 显示移植层
│   ├── bsp_ft6336.c             触摸驱动（软件 I2C）
│   ├── lv_port_indev_v9.c       LVGL 9 输入移植层
│   ├── uart.c                   USART1 环形缓冲 + 中断接收
│   ├── ESP.c                    USART2 环形缓冲
│   ├── MyFlash.c                Flash 参数存储
│   ├── DHT11.c / Light.c        传感器驱动
│   └── Buzzer.c / Key.c         蜂鸣器、按键
├── Tasks/                       7 个 FreeRTOS 任务 + AppData.h
└── Middlewares/LVGL/            LVGL 9.5 源码 + lv_conf.h
```

## 关键实现

### 时钟与 HAL 时基

HSE 8 MHz → PLLM 8 / PLLN 336 / PLLP 2 → **168 MHz**，APB1 = 42 MHz，APB2 = 84 MHz。

HAL 时基必须从 SysTick 改到 **TIM6**，否则 HAL 的 `HAL_Delay` / `HAL_GetTick` 会和 FreeRTOS 的 SysTick 抢同一个中断。

### LVGL 9 移植

- **帧缓冲**：`240 × 40 × 2 = 19200` 字节，单缓冲 `LV_DISPLAY_RENDER_MODE_PARTIAL`，32 字节对齐
- **避开 CCM**：F407 的 64KB CCM 在 `0x10000000`，DMA 访问不到。缓冲若落进去会表现为花屏或全白，移植时显式检查地址范围
- **单任务约束**：`LV_USE_OS = LV_OS_NONE`，所有 `lv_*` 调用收敛到 `LVGLTask`。其他任务只写共享变量，绝不直接碰 LVGL
- **字节序**：LVGL 的 `lv_color_t` 是小端，ILI9341 要 16 位大端，flush 时逐像素交换高低字节
- **字号裁剪**：字体用 `lv_font_conv` 按实际用到的汉字生成，避免整字库占用几百 KB Flash

### 显示链路

SPI1 主机模式，8 位，CPOL Low / CPHA 1Edge，软件 NSS，预分频 2（APB2 84 MHz → **42 MHz**），DMA2_Stream3 内存到外设。

`disp_flush_cb` 里开窗 → 换字节序 → 启动传输 → `lv_display_flush_ready`。当前用阻塞式 `HAL_SPI_Transmit`，接 DMA 只需把 `DISP_USE_DMA` 改成 1。

### Flash 参数存储

F4 按**扇区**擦除，最小 16KB，所以参数区选在 `0x080E0000`（Sector 11，最后 128KB）。

```
偏移 +0x00  STORE_FLAG  0x5A5A5A5A   上电校验，不等就擦除并写默认值
偏移 +0x04  Temp_HLimit
偏移 +0x08  Humi_HLimit
偏移 +0x0C  Flame_HLimit
```

`MyFlash_Save()` 擦整个扇区再逐字回写，耗时 1~2 秒，期间 CPU 取指会被 Flash 操作阻塞，界面会僵一下，属于硬件限制。

### MQTT 报文手工拼装

不引入协议库，直接拼字节：

```c
buf[pos++] = 0x32;                      /* PUBLISH + QoS1 */
buf[pos++] = (uint8_t)rem;              /* 剩余长度 */
buf[pos++] = (uint8_t)(tlen >> 8);      /* topic 长度高字节 */
...
```

上报内容：`{"t":25.4,"h":60.2,"l":55,"tmax":30,"hmax":80,"lmin":40}`，主题 `env/data`，5 秒一次。

> `AT+CWJAP="SSID","PASSWORD"` 里的 WiFi 账号密码硬编码在 `Tasks/UartTask.c` 的 `ESP_Connect()` 里，换网络时改这两处。

### 串口命令协议

波特率 115200，8N1，命令以 `\r\n` 结尾。

| 命令 | 作用 | 范围 |
|---|---|---|
| `T30` | 设置温度上限 | 0 ~ 50 |
| `H80` | 设置湿度上限 | 20 ~ 90 |
| `F70` | 设置明火阈值 | 0 ~ 90 |
| `S` | 当前参数写入 Flash | — |
| `?` | 查询三组阈值 | — |

超范围返回 `xxx Set Error`，未知命令返回 `Unknown [XX]`（XX 是收到的首字节十六进制，便于排查误码）。单条命令最长 8 字节，超长整条丢弃，避免后半截被当成新命令。

> 串口改的阈值只改 RAM，**必须再发 `S` 才落盘**。

## 构建

**环境**：Keil MDK 5 + ARMCC V5.06（AC5），工程文件 `MDK-ARM/10-2 myproject.uvprojx`。

**必开选项**：

- C99 模式（LVGL 9 依赖）
- 源文件编码 UTF-8 with signature（否则 AC5 按 GBK 解析中文，报 `missing closing quote`）

**命令行编译**：

```bash
UV4.exe -b "MDK-ARM/10-2 myproject.uvprojx" -j0 -o build.log
```

**资源占用**：`Code 316190 B ≈ 309 KB`，`RO-data 50994 B ≈ 50 KB`，`RW + ZI = 87752 B ≈ 86 KB`（主 SRAM 128 KB）。


## 后续计划

- 设置页补上明火阈值，改为分页或压缩卡片高度
- 报警历史写入 Flash 环形缓冲，界面加历史页
- ESP8266 配网改为串口命令下发 SSID / 密码

---

**License**：MIT
