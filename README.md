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
| Battery Holder | 3x AA | 1 | 4.5V output |
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
- 3x AA batteries (4.5V) → ESP32 5V pin
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

```
┌─────────────────────────────────────────────────────────┐
│                      Main Loop                          │
└─────────────────────────────────────────────────────────┘
                           │
                           ▼
              ┌────────────────────────┐
              │ Ultrasonic detects     │
              │ baby's feet?           │
              └────────────────────────┘
                    │           │
                   Yes          No
                    │           │
                    ▼           │
         ┌──────────────────┐   │
         │ Play welcome     │   │
         │ music (001.mp3)  │   │
         └──────────────────┘   │
                    │           │
                    ▼           │
              ┌────────────────────────┐
              │ Hall sensor detects    │
              │ magnet? (buckle loose) │
              └────────────────────────┘
                    │           │
                   Yes          No
                    │           │
                    ▼           ▼
         ┌──────────────────┐  ┌──────────────────┐
         │ Start 30s timer  │  │ Buckle fastened  │
         └──────────────────┘  │ - All good!      │
                    │          └──────────────────┘
                    ▼
              ┌────────────────────────┐
              │ 30 seconds passed &    │
              │ still detecting magnet?│
              └────────────────────────┘
                    │           │
                   Yes          No
                    │           │
                    ▼           ▼
         ┌──────────────────┐  ┌──────────────────┐
         │ Play reminder    │  │ Timer cancelled  │
         │ (002.mp3)        │  │ (buckle fastened)│
         └──────────────────┘  └──────────────────┘
```

## Power Saving / 省电模式

The device uses **deep sleep** to maximize battery life:

| State | Power | Behavior |
|-------|-------|----------|
| Deep Sleep | ~10μA | Wakes every 3 seconds to check |
| Active (no baby) | ~70mA | Quick check, then back to sleep |
| Active (baby detected) | ~70mA | Continuous monitoring |

**Estimated battery life with 4x AA:**
- Without sleep: ~35-40 hours continuous
- With sleep: **several weeks to months**

To disable sleep mode for debugging, set in code:
```cpp
#define SLEEP_ENABLED false
```

## Configuration / 配置调整

Key parameters in the code you may need to adjust:

```cpp
#define ULTRASONIC_THRESHOLD_CM 30    // Detection distance (cm)
#define HALL_THRESHOLD 2000           // Hall sensor threshold (test and adjust)
#define BUCKLE_REMINDER_DELAY_MS 30000  // Reminder delay (30 seconds)
#define SLEEP_INTERVAL_US 3000000     // Sleep interval (3 seconds)
```

## Future Enhancements / 未来扩展

- [ ] WiFi notifications to smartphone
- [ ] Battery level monitoring
- [ ] Adjustable detection distance via web interface
- [ ] Multiple language support

## License

MIT License
