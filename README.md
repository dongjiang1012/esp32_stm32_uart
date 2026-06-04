# ESP32-S3 + STM32F103RB 机械臂无线控制

通过 ESP32-S3 WiFi 热点提供网页控制界面，经 UART 转发指令到 STM32 驱动 6 路舵机。

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

1. 烧录 `esp32/` 和 `stm32/` 分别到对应板子
2. ESP32 上电后创建 WiFi 热点 `RobotArm`（无密码）
3. 手机/电脑连接热点，浏览器访问 `http://192.168.4.1`
4. 网页上可单独控制每个舵机，或使用预设动作（Home/Grab/Wave）

## 项目结构

```
esp32/          ESP32-S3 端 - WiFi AP + Web 控制 + UART 转发
stm32/          STM32F103RB 端 - UART 接收 + 舵机 PWM 驱动
```

## 版本记录

- **v1.0** - 首版，实现局域网无线控制 6 路舵机
