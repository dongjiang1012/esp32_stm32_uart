# ESP32-S3 + STM32F103RB 六轴机械臂无线控制系统

基于 ESP32-S3 和 STM32F103RB 的六路舵机机械臂控制系统，支持**网页远程控制**、**公网穿透访问**和 **PC 桌面软件控制**三种方式。

## 系统架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        控制端 (三种方式)                          │
│                                                                 │
│   手机/电脑浏览器          natapp 公网          PC Qt 桌面软件    │
│   http://192.168.x.x     公网域名访问          RobotArm.exe     │
│         │                     │                     │           │
│         ▼                     ▼                     ▼           │
│   ┌──────────┐          ┌──────────┐         ┌──────────┐      │
│   │  WiFi STA │    WiFi  │  WiFi STA │  USB   │  USB CDC  │     │
│   │  路由器    │◄────────│  路由器    │◄───────│  虚拟串口  │     │
│   └────┬─────┘          └────┬─────┘         └────┬─────┘      │
│        │                     │                     │            │
└────────┼─────────────────────┼─────────────────────┼────────────┘
         │                     │                     │
         ▼                     ▼                     ▼
┌─────────────────────────────────────────────────────────────────┐
│                  ESP32-S3 (中枢, FreeRTOS)                       │
│                                                                 │
│   ┌─────────────┐   ┌──────────────┐   ┌───────────────────┐   │
│   │  Web Server  │   │  WiFi 管理    │   │  USB CDC 转发     │   │
│   │  + 登录认证   │   │  STA/断线重连  │   │  Serial 指令解析  │   │
│   └──────┬──────┘   └──────────────┘   └────────┬──────────┘   │
│          │                                       │              │
│          └───────────────┬───────────────────────┘              │
│                          ▼                                      │
│                  ┌───────────────┐                              │
│                  │   Serial1     │                              │
│                  │  GPIO43/44    │                              │
│                  │  115200 bps   │                              │
│                  └───────┬───────┘                              │
└──────────────────────────┼──────────────────────────────────────┘
                           │ UART
                           ▼
┌─────────────────────────────────────────────────────────────────┐
│                    STM32F103RB (执行器)                           │
│                                                                 │
│   ┌──────────────┐   ┌─────────────────────────────────────┐   │
│   │  USART3 解析  │   │         Timer3 软件 PWM              │   │
│   │  PB10/PB11   │──►│  6 路舵机渐进驱动 (50Hz, 20us 分辨率) │   │
│   └──────────────┘   └─────────────────────────────────────┘   │
│                                                                 │
│   舵机引脚: PC10 PC11 PC12 PD2 PB5 PB8                          │
│   ADC 电压检测: PC3                                              │
└─────────────────────────────────────────────────────────────────┘
```

## 硬件清单

| 硬件 | 型号 | 数量 |
|------|------|------|
| 主控板 | ESP32-S3-DevKitM-1 | 1 |
| 执行板 | STM32F103RB (最小系统板) | 1 |
| 舵机 | SG90 / MG90S | 6 |
| 机械臂 | 6DOF 机械臂支架 | 1 |
| 连接线 | 杜邦线 | 若干 |
| 电源 | 5V 2A+ (舵机供电) | 1 |

## 硬件接线

### ESP32-S3 <-> STM32F103RB (UART)

```
ESP32-S3                    STM32F103RB
─────────                   ───────────
GPIO43 (Serial1 TX)  ───►  PB11 (USART3 RX)
GPIO44 (Serial1 RX)  ◄───  PB10 (USART3 TX)
GND                   ────  GND
```

> STM32 板子丝印上的 **BLE_TX/BLE_RX** = USART3（PB10/PB11）

### STM32F103RB <-> 舵机 (PWM)

| 舵机编号 | 功能 | STM32 引脚 |
|----------|------|-----------|
| S0 | Gripper (夹爪) | PC10 |
| S1 | Rotate1 (旋转) | PC11 |
| S2 | ArmX-1 (臂节1) | PC12 |
| S3 | ArmX-2 (臂节2) | PD2 |
| S4 | ArmX-3 (臂节3) | PB5 |
| S5 | Base (底座) | PB8 |

### STM32F103RB 电池电压检测

| 功能 | STM32 引脚 |
|------|-----------|
| ADC 电压采集 | PC3 |

### ESP32-S3 <-> PC (USB)

ESP32-S3 的 USB-C 口直接连接电脑，通过 USB CDC 虚拟串口通信，波特率 115200。

## 串口协议

ESP32 与 STM32 之间、Qt 软件与 ESP32 之间，共用同一套指令协议：

| 指令 | 格式 | 示例 | 说明 |
|------|------|------|------|
| 舵机控制 | `S<id>,<angle>,<time_ms>` | `S0,90,500` | id: 0-5, angle: 0-180, time: >=100ms |
| 急停 | `STOP` | `STOP` | 所有舵机回中 90° |
| 电压查询 | `VOLT` | `VOLT` | 返回 STM32 ADC 采集的电池电压 (V) |

**STM32 应答：**

| 应答 | 含义 |
|------|------|
| `OK` | 指令执行成功 |
| `ERR` | 参数错误（id/angle/time 越界） |
| `<float>` | VOLT 指令返回的电压值，如 `3.30` |

**参数范围：**
- `id`：0 ~ 5（对应 6 路舵机）
- `angle`：0 ~ 180 度
- `time_ms`：>= 100 毫秒（舵机从当前位置移动到目标位置的时间）

## 使用方法

### 方式一：局域网 WiFi 控制

1. 在 `esp32/src/main.cpp` 中修改 WiFi 账号密码：
   ```cpp
   const char* WIFI_SSID = "你的WiFi名";
   const char* WIFI_PASS = "你的WiFi密码";
   ```
2. 分别烧录 `esp32/` 和 `stm32/` 固件到对应板子
3. ESP32 上电后连接路由器，串口打印本地 IP
4. 手机/电脑连接同一网络，浏览器访问 ESP32 的 IP 地址
5. 使用默认账号登录：用户名 `admin`，密码 `password`

### 方式二：公网远程控制

通过 [natapp](https://natapp.cn/) 内网穿透，实现任意网络下的远程控制。

1. 按照方式一配置 WiFi 并烧录固件
2. PC 端运行 natapp 客户端，配置转发到 ESP32 的本地 IP:80
3. 浏览器访问 natapp 分配的公网地址即可控制

### 方式三：PC 桌面软件控制

通过 USB 数据线连接 ESP32-S3，使用 Qt 桌面软件控制。

1. 分别烧录 `esp32/` 和 `stm32/` 固件
2. 用 USB 数据线连接 ESP32-S3 到电脑
3. 运行 `qt_controller/dist/RobotArm.exe` 或从源码运行 `python robot_arm.py`
4. 在软件中选择 ESP32 的 COM 口，点击 **Connect**
5. 通过滑块控制 6 路舵机，或使用预设动作（Home / Grab / Wave）

**Qt 软件功能：**
- 自动检测可用串口
- 6 路舵机独立控制（滑块 0-180 度 + 移动时间设置）
- 急停按钮（STOP）
- 3 组预设动作序列（Home / Grab / Wave）
- 实时收发日志显示
- 命令队列，保证指令顺序执行

## v4.0 新特性

### ESP32 端
- **FreeRTOS 多任务架构**：WiFi 管理、Web 服务器、USB CDC 串口分别运行在独立任务中
- **Web 登录认证**：网页端需用户名/密码登录，基于 token 的会话管理（1 小时超时）
- **UART 互斥锁**：Web 和 USB 通道共享 Serial1，通过 mutex 保证线程安全
- **电池电压显示**：网页端每 5 秒刷新 STM32 ADC 采集的电池电压，低压红色警示

### STM32 端
- **ADC 电压检测**：新增 `VOLT` 指令，通过 PC3 引脚采集电池电压
- **LED 状态指示**：收到指令时 LED 亮，处理完毕后灭

## 舵机 PWM 驱动说明

STM32 端使用 **Timer3 中断**实现软件 PWM，而非硬件 PWM：

- **PWM 频率**：50Hz（周期 20ms）
- **中断周期**：20us（50kHz），通过计数 1000 次实现 20ms 周期
- **脉宽范围**：mid_pulseWidth ± 50（对应 0-180 度）
- **渐进移动**：舵机不会瞬间跳到目标角度，而是按设定时间匀速移动
- **6 路独立**：每路舵机有独立的目标位置、移动方向、步进延时

```
时间轴 ──────────────────────────────────►
       ┌──────────────────────────────────┐
  20ms │ ██████████████░░░░░░░░░░░░░░░░░░ │  ← 脉宽决定角度
       └──────────────────────────────────┘
       ◄─ 脉宽 ─►
       0.5ms(0°) ~ 2.5ms(180°)
```

## 项目结构

```
esp32_stm32_uart/
├── README.md
├── .gitignore
│
├── esp32/                          # ESP32-S3 固件 (PlatformIO)
│   ├── platformio.ini              #   板型: esp32-s3-devkitm-1
│   └── src/
│       └── main.cpp                #   FreeRTOS 多任务: Web Server + USB CDC + UART
│
├── stm32/                          # STM32F103RB 固件 (PlatformIO)
│   ├── platformio.ini              #   板型: genericSTM32F103RB, Maple 框架
│   └── src/
│       ├── main.cpp                #   USART3 指令解析 + ADC 电压采集
│       ├── servo.h                 #   舵机数据结构定义
│       └── servo.cpp               #   Timer3 中断 + 6 路软件 PWM
│
└── qt_controller/                  # PC 端 Qt 控制软件
    ├── robot_arm.py                #   Python + PyQt5 (主要使用)
    ├── RobotArm.pro                #   Qt 项目文件 (C++ 版)
    ├── main.cpp                    #   Qt 入口 (C++ 版)
    ├── mainwindow.h                #   Qt 头文件 (C++ 版)
    └── mainwindow.cpp              #   Qt 实现 (C++ 版)
```

## 开发环境

| 组件 | 版本/工具 |
|------|----------|
| ESP32 SDK | Arduino (espressif32) |
| STM32 SDK | Arduino (ststm32, Maple 框架) |
| 构建工具 | PlatformIO |
| Qt 控制软件 | Python 3 + PyQt5 + pyserial |
| 打包工具 | PyInstaller (可选) |

### 从源码构建 Qt 软件

```bash
# 安装依赖
pip install PyQt5 pyserial

# 直接运行
cd qt_controller
python robot_arm.py

# (可选) 打包为 exe
pip install pyinstaller
pyinstaller --onefile --windowed --name RobotArm robot_arm.py
# 生成文件: dist/RobotArm.exe
```

### 编译固件

```bash
# 安装 PlatformIO CLI
pip install platformio

# 编译 ESP32 固件
cd esp32
pio run

# 编译 STM32 固件
cd stm32
pio run

# 烧录 (通过 USB)
pio run --target upload
```

## 版本记录

| 版本 | 日期 | 更新内容 |
|------|------|---------|
| v1.0 | 2025-06-05 | 首版：WiFi AP 模式局域网无线控制 6 路舵机 |
| v2.0 | 2025-06-05 | 改为 STA 模式连接路由器，配合 natapp 实现公网远程控制；新增 WiFi 断线重连 |
| v3.0 | 2025-06-05 | 新增 USB CDC 通道，ESP32 同时支持网页和 USB 串口控制；新增 Qt 桌面控制软件 |
| v4.0 | 2025-06-05 | FreeRTOS 多任务架构；Web 登录认证 + token 会话管理；电池电压检测与显示 |

## License

MIT
