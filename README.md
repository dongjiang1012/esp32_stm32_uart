# ESP32-S3 + STM32F103RB 机械臂无线控制

通过 ESP32-S3 提供网页控制界面，经 UART 转发指令到 STM32 驱动 6 路舵机。

## 硬件连接

```
ESP32-S3              STM32F103RB
GPIO43 (Serial1 TX) → PB11 (USART3 RX)
GPIO44 (Serial1 RX) ← PB10 (USART3 TX)
GND                  → GND
```

- 波特率: 115200
- STM32 板子丝印上的 BLE_TX/BLE_RX = USART3（PB10/PB11）

## 串口协议

| 指令 | 格式 | 示例 | 说明 |
|------|------|------|------|
| 舵机控制 | `S<id>,<angle>,<time_ms>` | `S0,90,500` | id: 0-5, angle: 0-180, time: >=100ms |
| 急停 | `STOP` | | 所有舵机回中 90° |
| 回复 | `OK` / `ERR` | | STM32 返回执行结果 |

## 使用方法

### v1 - 局域网控制
1. 烧录 `esp32/` 和 `stm32/` 分别到对应板子
2. ESP32 上电后创建 WiFi 热点 `RobotArm`（无密码）
3. 手机/电脑连接热点，浏览器访问 `http://192.168.4.1`

### v2 - 公网控制（natapp）
1. 修改 `esp32/src/main.cpp` 中的 WiFi 账号密码
2. 烧录 `esp32/` 和 `stm32/` 分别到对应板子
3. ESP32 上电后连接路由器，串口打印本地 IP
4. PC 端运行 natapp 客户端，配置转发到 ESP32 的本地 IP:80
5. 浏览器访问 natapp 分配的公网地址即可控制

### v3 - PC 桌面控制（Qt）
1. 烧录 `esp32/` 和 `stm32/` 分别到对应板子
2. 用 USB 线连接 ESP32-S3 到电脑
3. 运行 Qt 程序 `RobotArm`，选择 ESP32 的 COM 口，点击 Connect
4. 通过滑块控制 6 路舵机，或使用预设动作（Home/Grab/Wave）

## 项目结构

```
esp32/          ESP32-S3 端 - Web 控制 + USB CDC + UART 转发
stm32/          STM32F103RB 端 - UART 接收 + 舵机 PWM 驱动
qt_controller/  PC 端 Qt 桌面控制软件
```

## 版本记录

- **v1.0** - 首版，WiFi AP 模式局域网无线控制 6 路舵机
- **v2.0** - 改为 STA 模式连接路由器，配合 natapp 实现公网远程控制；新增 WiFi 断线重连、网页信号强度显示
- **v3.0** - 新增 USB CDC 通道，ESP32 同时支持网页和 USB 串口控制；新增 Qt 桌面控制软件
