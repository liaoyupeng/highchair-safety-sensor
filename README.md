# High Chair Safety Sensor / 婴儿座椅安全带提醒器

A smart sensor system that reminds parents to buckle up their baby when seated in a high chair.

当宝宝坐上婴儿座椅时，自动提醒家长系好安全带。

## Features / 功能

### 1. Ultrasonic Detection / 超声波检测
- Detects baby's feet using ultrasonic sensor
- When detected, plays a welcome music to remind parent to buckle up
- 通过超声波传感器检测宝宝伸出来的脚
- 检测到后播放提示音乐

### 2. Hall Effect Sensor Detection / 霍尔传感器检测
- Monitors the buckle status using a magnet attached to the buckle
- Device is placed under the seat
- If buckle is not fastened (magnet detected near sensor), starts 30-second countdown
- After 30 seconds, plays voice reminder: "Please buckle up the baby"
- 利用霍尔传感器检测安全带扣的状态
- 装置放在座椅底下
- 如果安全带没系上（检测到磁场），30秒后播放语音提醒

## Hardware / 硬件清单

| Component | Model | Quantity | Notes |
|-----------|-------|----------|-------|
| Microcontroller | ESP32-C3 Supermini | 1 | 22x18mm, WiFi/BLE |
| Ultrasonic Sensor | HC-SR04 | 1 | 2cm-450cm range |
| Hall Effect Sensor | 49E (Linear) or KY-035 | 1 | Analog output |
| MP3 Player | DFPlayer Mini | 1 | TF card slot |
| Speaker | 8Ω 2-3W | 1 | Small speaker |
| Battery Holder | 4x AA | 1 | 6V output (regulated to 3.3V) |
| Magnet | Neodymium (small) | 1 | Attach to buckle |
| Jumper Wires | Dupont F-F | Several | For connections |
| TF Card | Any | 1 | Store MP3 files |

## Wiring Diagram / 接线图

```
                    ESP32-C3 Supermini
                    ┌─────────────────┐
                    │                 │
    Battery+ ───────┤ 5V          GND ├─────── Battery-
                    │                 │
                    │ 3.3V        GND ├─────┬─── HC-SR04 GND
                    │                 │     ├─── 49E GND
                    │                 │     └─── DFPlayer GND
                    │                 │
    HC-SR04 VCC ────┤ 5V              │
    HC-SR04 Trig ───┤ GPIO2           │
    HC-SR04 Echo ───┤ GPIO3           │
                    │                 │
    49E VCC ────────┤ 3.3V            │
    49E OUT ────────┤ GPIO1 (ADC)     │
                    │                 │
    DFPlayer VCC ───┤ 3.3V            │
    DFPlayer RX ────┤ GPIO21 (TX)     │
    DFPlayer TX ────┤ GPIO20 (RX)     │
                    │                 │
                    └─────────────────┘

    DFPlayer Speaker Pins ──── Speaker (8Ω 2W)
```

### Wiring Details / 接线说明

#### Power / 电源
- 4x AA batteries (6V) → ESP32 5V pin
- ESP32 provides regulated 3.3V for sensors

#### HC-SR04 Ultrasonic Sensor / 超声波传感器
| HC-SR04 Pin | ESP32 Pin |
|-------------|-----------|
| VCC | 5V |
| Trig | GPIO2 |
| Echo | GPIO3 |
| GND | GND |

#### 49E Hall Effect Sensor / 霍尔传感器
| 49E Pin | ESP32 Pin |
|---------|-----------|
| VCC | 3.3V |
| OUT | GPIO1 (ADC) |
| GND | GND |

Note: If using KY-035 module, connect AO (Analog Out) to GPIO1.

#### DFPlayer Mini / MP3 播放模块
| DFPlayer Pin | ESP32 Pin |
|--------------|-----------|
| VCC | 3.3V |
| GND | GND |
| RX | GPIO21 (ESP TX) |
| TX | GPIO20 (ESP RX) |
| SPK1 | Speaker + |
| SPK2 | Speaker - |

## Installation / 安装

### 1. Prepare TF Card / 准备 TF 卡
Create folder structure:
```
/01/
    001.mp3  - Welcome music (宝宝坐好啦)
    002.mp3  - Reminder (请给宝宝系上安全带)
```

### 2. Physical Installation / 物理安装
1. Mount the device under the high chair seat
2. Position ultrasonic sensor to detect baby's feet
3. Attach small neodymium magnet to the buckle
4. Position hall sensor where it can detect the magnet when buckle is unfastened

### 3. Upload Code / 上传代码
1. Install Arduino IDE
2. Add ESP32 board support
3. Install DFRobotDFPlayerMini library
4. Upload `highchair_sensor.ino`

## Logic Flow / 逻辑流程

The device always sleeps, waking every 3 seconds to check:

```
┌─────────────────────────────────────────────────────────┐
│                    Deep Sleep (3s)                       │
└─────────────────────────────────────────────────────────┘
                           │
                      Wake up
                           │
                           ▼
              ┌────────────────────────┐
              │ Check ultrasonic       │
              │ Baby detected?         │
              └────────────────────────┘
                    │           │
                   Yes          No ──→ Reset counters
                    │                        │
                    ▼                        │
         ┌──────────────────┐               │
         │ First detection? │               │
         │ → Play welcome   │               │
         └──────────────────┘               │
                    │                        │
                    ▼                        │
              ┌────────────────────────┐    │
              │ Check hall sensor      │    │
              │ Buckle loose?          │    │
              └────────────────────────┘    │
                    │           │           │
                   Yes          No          │
                    │           │           │
                    ▼           ▼           │
         ┌──────────────┐  ┌──────────────┐│
         │ looseCount++ │  │ looseCount=0 ││
         └──────────────┘  └──────────────┘│
                    │                       │
                    ▼                       │
         ┌──────────────────┐              │
         │ looseCount >= 10?│              │
         │ (30 seconds)     │              │
         └──────────────────┘              │
              │         │                   │
             Yes        No                  │
              │         │                   │
              ▼         └───────────────────┤
         ┌──────────────┐                   │
         │ Play reminder│                   │
         │ looseCount=0 │                   │
         └──────────────┘                   │
                    │                       │
                    └───────────────────────┤
                                            │
                                            ▼
                              ┌────────────────────────┐
                              │   Go back to sleep     │
                              └────────────────────────┘
```

## Power Saving / 省电模式

The device **always** uses deep sleep - wakes every 3 seconds, checks sensors, then goes back to sleep.

```
Sleep 3s → Wake → Check sensors → Sleep 3s → Wake → ...
```

| State | Duration | Power |
|-------|----------|-------|
| Deep Sleep | ~3 seconds | ~10μA |
| Active check | ~100ms | ~70mA |

**Average power:** ~1-2mA (mostly sleeping!)

**Estimated battery life with 4x AA (2500mAh):**
- Continuous run (no sleep): ~35 hours
- With sleep: **2-3 months**

## Configuration / 配置调整

Key parameters in the code you may need to adjust:

```cpp
#define ULTRASONIC_THRESHOLD_CM 30    // Detection distance (cm)
#define HALL_THRESHOLD 2000           // Hall sensor threshold (test and adjust!)
#define SLEEP_INTERVAL_US 3000000     // Sleep interval (3 seconds)
#define REMINDER_CYCLES 10            // 10 × 3s = 30 second reminder delay
```

### Debug Mode
Set `DEBUG_MODE` to `false` in production to save a bit more power:
```cpp
#define DEBUG_MODE false
```

## Future Enhancements / 未来扩展

- [ ] WiFi notifications to smartphone
- [ ] Battery level monitoring
- [ ] Adjustable detection distance via web interface
- [ ] Multiple language support

## License

MIT License
