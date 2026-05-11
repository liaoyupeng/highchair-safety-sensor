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

### 3. Battery Monitoring / 电池电量监测
- Monitors battery voltage via voltage divider
- Plays warning audio when battery is low (<4.6V)
- Warning repeats every ~10 minutes while battery is low
- 通过分压电路监测电池电压
- 电池电量低于4.6V时播放提醒
- 每约10分钟重复提醒一次

### 4. Night Mode / 夜间模式
- Automatically disables detection during sleep hours (10pm - 7am)
- Uses DS3231 RTC module to keep accurate time
- During night mode, sleeps for 1 hour between checks (instead of 3 seconds)
- Saves battery by skipping unnecessary checks at night
- 夜间自动关闭检测（晚上10点到早上7点）
- 使用 DS3231 实时时钟模块保持准确时间
- 夜间模式下每小时唤醒一次（而不是3秒），更省电

### 5. Flash Logging / 内置日志存储
- Logs are saved to ESP32's internal flash (LittleFS)
- Each log entry includes timestamp from RTC
- On fresh boot (USB reconnect), saved logs are printed to Serial
- Max log size: ~50KB (auto-clears when full)
- 日志保存到 ESP32 内置 Flash（LittleFS）
- 每条日志带 RTC 时间戳
- 重新插 USB 时自动打印之前保存的日志
- 最大约 50KB，满了自动清空

## Hardware / 硬件清单

| Component | Model | Quantity | Notes |
|-----------|-------|----------|-------|
| Microcontroller | ESP32-C3 Supermini | 1 | 22x18mm, WiFi/BLE |
| Ultrasonic Sensor | HC-SR04 | 1 | 2cm-450cm range |
| Hall Effect Sensor | 49E (Linear) or KY-035 | 1 | Analog output |
| RTC Module | DS3231 | 1 | For night mode timing |
| MP3 Player | DFPlayer Mini | 1 | TF card slot |
| Speaker | 8Ω 2-3W | 1 | Small speaker |
| Battery Holder | 4x AA | 1 | 6V output (regulated to 3.3V) |
| Resistors | 100KΩ | 2 | For battery voltage divider (low power) |
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
    DS3231 VCC ─────┤ 3.3V            │
    DS3231 SDA ─────┤ GPIO8           │
    DS3231 SCL ─────┤ GPIO9           │
                    │                 │
                    └─────────────────┘

    DFPlayer Speaker Pins ──── Speaker (8Ω 2W)
```

### Wiring Details / 接线说明

#### Power / 电源
- 4x AA batteries (6V) → ESP32 5V pin
- ESP32 provides regulated 3.3V for sensors

#### Battery Voltage Monitor / 电池电压监测
Uses a voltage divider to measure battery voltage:
```
Battery+ (6V) ─── [100KΩ] ───┬─── GPIO0 (ADC)
                             │
                          [100KΩ]
                             │
Battery- (GND) ──────────────┴─── GND
```
This divides voltage by 2, so 6V battery → 3V at ADC (safe for 3.3V input).
Using 100KΩ resistors keeps the divider current low (~0.03mA) for better battery life.

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

#### DS3231 RTC Module / 实时时钟模块
| DS3231 Pin | ESP32 Pin |
|------------|-----------|
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO8 |
| SCL | GPIO9 |

Note: DS3231 has a built-in CR2032 battery holder to keep time when main power is off.

## Installation / 安装

### 1. Prepare TF Card / 准备 TF 卡
Create folder structure:
```
/01/
    001.mp3  - Welcome music (宝宝坐好啦)
    002.mp3  - Buckle reminder (请给宝宝系上安全带)
    003.mp3  - Low battery warning (电池电量不足，请及时更换)
```

### 2. Physical Installation / 物理安装
1. Mount the device under the high chair seat
2. Position ultrasonic sensor to detect baby's feet
3. Attach small neodymium magnet to the buckle
4. Position hall sensor where it can detect the magnet when buckle is unfastened

### 3. Upload Code / 上传代码
1. Install Arduino IDE
2. Add ESP32 board support
3. Install libraries:
   - DFRobotDFPlayerMini
   - RTClib (by Adafruit)
4. Upload `highchair_sensor.ino`

### 4. Set RTC Time (First Time Only) / 设置时钟（仅首次）
Upload this sketch once to set the RTC time, then upload the main code:
```cpp
#include <RTClib.h>
#include <Wire.h>

RTC_DS3231 rtc;

void setup() {
  Wire.begin(8, 9);  // SDA=GPIO8, SCL=GPIO9
  rtc.begin();

  // Set time to compile time (run immediately after compiling!)
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // Or set manually: rtc.adjust(DateTime(2026, 5, 11, 20, 30, 0));

  Serial.begin(115200);
  Serial.println("RTC time set!");
}

void loop() {}
```
After setting, the CR2032 battery on DS3231 will keep the time.

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
              │ Night mode?            │
              │ (10pm - 7am)           │
              └────────────────────────┘
                    │           │
                   Yes          No
                    │           │
                    ▼           ▼
              Go to sleep   Continue
                           │
                           ▼
              ┌────────────────────────┐
              │ Check ultrasonic       │
              │ Baby detected?         │
              └────────────────────────┘
                    │           │
                   Yes          No ──→ Reset counters (after 3× no detection)
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
#define CONFIRM_DETECTION 1           // 1 cycle (3s) to confirm baby seated
#define CONFIRM_NO_DETECTION 3        // 3 cycles (9s) to confirm baby left
#define BATTERY_LOW_VOLTAGE 4.6       // Low battery threshold (volts)
#define BATTERY_WARN_CYCLES 200       // Warn every 200 cycles (~10 minutes)

// Night mode settings
#define NIGHT_MODE_ENABLED true       // Set to false to disable
#define NIGHT_START_HOUR 22           // 10:00 PM
#define NIGHT_END_HOUR 7              // 7:00 AM
#define NIGHT_SLEEP_INTERVAL_US 3600000000ULL  // 1 hour sleep during night

// Flash logging settings
#define FLASH_LOG_ENABLED true        // Set to false to disable
#define MAX_LOG_SIZE 50000            // Max log file size (~50KB)
```

### Debug Mode
Set `DEBUG_MODE` to `false` in production to save a bit more power:
```cpp
#define DEBUG_MODE false
#define FLASH_LOG_ENABLED false       // Also disable flash logging
```

## Future Enhancements / 未来扩展

- [ ] WiFi notifications to smartphone
- [x] ~~Battery level monitoring~~ (已实现)
- [x] ~~Night mode auto on/off~~ (已实现)
- [x] ~~Flash logging~~ (已实现)
- [ ] Adjustable detection distance via web interface
- [ ] Multiple language support

## License

MIT License
