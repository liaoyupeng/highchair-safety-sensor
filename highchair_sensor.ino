/*
 * High Chair Safety Sensor / 婴儿座椅安全带提醒器
 *
 * Hardware:
 * - ESP32-C3 Supermini
 * - HC-SR04 Ultrasonic Sensor
 * - 49E Linear Hall Effect Sensor
 * - DFPlayer Mini + Speaker
 * - DS3231 RTC Module (for night mode)
 * - Voltage divider for battery monitoring (100K + 100K)
 *
 * Power Saving:
 * - Always uses deep sleep, wakes every 3 seconds to check
 * - Uses wake cycle counting for 30-second timer
 * - Night mode: skips detection between 10pm - 7am
 * - Extremely low average power consumption
 *
 * Author: Yupeng Liao
 * License: MIT
 */

#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <RTClib.h>
#include <Wire.h>
#include <LittleFS.h>
#include "esp_sleep.h"

// ==================== Pin Definitions ====================
#define TRIG_PIN 2      // Ultrasonic Trig
#define ECHO_PIN 3      // Ultrasonic Echo
#define HALL_PIN 1      // Hall sensor (ADC)
#define BATTERY_PIN 0   // Battery voltage (ADC) - via voltage divider
#define DFPLAYER_RX 20  // ESP32 RX <- DFPlayer TX
#define DFPLAYER_TX 21  // ESP32 TX -> DFPlayer RX
#define I2C_SDA 8       // DS3231 SDA
#define I2C_SCL 9       // DS3231 SCL

// ==================== Configuration ====================
#define ULTRASONIC_THRESHOLD_CM 30    // Distance to detect baby
#define HALL_THRESHOLD 2000           // Hall sensor threshold (adjust!)
#define SLEEP_INTERVAL_US 3000000     // 3 seconds between checks
#define REMINDER_CYCLES 10            // 10 cycles × 3s = 30 seconds

// Battery monitoring
// Using voltage divider: 10K + 10K, so Vout = Vin / 2
// 4x AA full = 6.4V → ADC sees 3.2V
// 4x AA low = 4.4V → ADC sees 2.2V
#define BATTERY_DIVIDER_RATIO 2.0     // Voltage divider ratio (R1+R2)/R2
#define BATTERY_LOW_VOLTAGE 4.6       // Low battery threshold (volts)
#define BATTERY_WARN_CYCLES 200       // Warn every 200 cycles (~10 minutes)

// Audio tracks (on TF card: /01/001.mp3, /01/002.mp3, /01/003.mp3)
#define TRACK_WELCOME 1       // 001.mp3 - Welcome music
#define TRACK_REMINDER 2      // 002.mp3 - Buckle reminder
#define TRACK_LOW_BATTERY 3   // 003.mp3 - Low battery warning

// Night mode - skip detection during sleep hours
#define NIGHT_MODE_ENABLED true
#define NIGHT_START_HOUR 22   // 10:00 PM
#define NIGHT_END_HOUR 7      // 7:00 AM
#define NIGHT_SLEEP_INTERVAL_US 3600000000ULL  // 1 hour (in microseconds)

// Debug mode - set to false to disable Serial output
#define DEBUG_MODE true

// Flash logging - saves logs to internal flash, read via Serial on fresh boot
#define FLASH_LOG_ENABLED true
#define LOG_FILE "/log.txt"
#define MAX_LOG_SIZE 50000    // Max log file size in bytes (~50KB)

// ==================== RTC Memory (persists across sleep) ====================
RTC_DATA_ATTR bool babyDetected = false;
RTC_DATA_ATTR int buckleLooseCount = 0;      // Counts cycles with loose buckle
RTC_DATA_ATTR int detectionCount = 0;        // Counts consecutive baby detections
RTC_DATA_ATTR int noDetectionCount = 0;      // Counts consecutive no-detections
RTC_DATA_ATTR bool welcomePlayed = false;
RTC_DATA_ATTR int batteryCheckCount = 0;     // Counter for battery warning interval
RTC_DATA_ATTR bool lowBatteryWarned = false; // Prevent repeated warnings

// Thresholds for confirming state changes
#define CONFIRM_DETECTION 1       // 1 cycle to confirm baby (immediate)
#define CONFIRM_NO_DETECTION 3    // 3 cycles to confirm baby left

// ==================== Global Variables ====================
HardwareSerial dfPlayerSerial(1);
DFRobotDFPlayerMini dfPlayer;
RTC_DS3231 rtc;

// ==================== Setup ====================
void setup() {
  if (DEBUG_MODE) {
    Serial.begin(115200);
    delay(100);
  }

  // Initialize LittleFS for flash logging
  if (FLASH_LOG_ENABLED) {
    if (!LittleFS.begin(true)) {  // true = format if mount fails
      Serial.println("LittleFS mount failed!");
    }
  }

  // Initialize pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(HALL_PIN, INPUT);
  pinMode(BATTERY_PIN, INPUT);

  // Initialize I2C for RTC
  Wire.begin(I2C_SDA, I2C_SCL);

  // Check wake reason
  esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
  if (reason != ESP_SLEEP_WAKEUP_TIMER) {
    // Fresh boot - reset all state
    // Print existing logs first (if connected to USB)
    if (DEBUG_MODE && FLASH_LOG_ENABLED) {
      printLogs();
    }
    debugPrint("Fresh boot - initializing");
    babyDetected = false;
    buckleLooseCount = 0;
    detectionCount = 0;
    noDetectionCount = 0;
    welcomePlayed = false;
    batteryCheckCount = 0;
    lowBatteryWarned = false;
  } else {
    debugPrint("Woke from sleep");
  }

  // Check night mode first - skip everything if in night hours
  if (NIGHT_MODE_ENABLED && isNightMode()) {
    debugPrint("Night mode - sleeping for 1 hour");
    goToSleep(NIGHT_SLEEP_INTERVAL_US);
    return;
  }

  // Check battery level
  checkBattery();

  // Main detection logic
  checkSensors();

  // Go back to sleep
  goToSleep();
}

void loop() {
  // Never reaches here - device sleeps after setup()
}

// ==================== Battery Monitoring ====================
void checkBattery() {
  // Read battery voltage via ADC
  int adcValue = analogRead(BATTERY_PIN);

  // ESP32-C3 ADC: 12-bit (0-4095), reference 3.3V
  float adcVoltage = (adcValue / 4095.0) * 3.3;
  float batteryVoltage = adcVoltage * BATTERY_DIVIDER_RATIO;

  debugPrint("Battery: " + String(batteryVoltage, 2) + "V (ADC: " + String(adcValue) + ")");

  // Check if battery is low
  if (batteryVoltage < BATTERY_LOW_VOLTAGE && batteryVoltage > 1.0) {  // >1.0 to ignore no-battery
    batteryCheckCount++;

    // Warn every BATTERY_WARN_CYCLES (~10 minutes) or on first detection
    if (!lowBatteryWarned || batteryCheckCount >= BATTERY_WARN_CYCLES) {
      debugPrint("Low battery warning!");
      playTrack(TRACK_LOW_BATTERY);
      lowBatteryWarned = true;
      batteryCheckCount = 0;
    }
  } else {
    // Battery OK - reset warning flag
    lowBatteryWarned = false;
    batteryCheckCount = 0;
  }
}

// ==================== Main Logic ====================
void checkSensors() {
  // Read ultrasonic
  float distance = readUltrasonic();
  bool detected = (distance > 0 && distance < ULTRASONIC_THRESHOLD_CM);

  debugPrint("Distance: " + String(distance) + " cm");

  // ===== Baby detection state machine =====
  if (detected) {
    detectionCount++;
    noDetectionCount = 0;

    // Confirm baby is sitting
    if (detectionCount >= CONFIRM_DETECTION && !babyDetected) {
      babyDetected = true;
      debugPrint("Baby confirmed!");

      // Play welcome music (only once per session)
      if (!welcomePlayed) {
        playTrack(TRACK_WELCOME);
        welcomePlayed = true;
      }
    }
  } else {
    noDetectionCount++;
    detectionCount = 0;

    // Confirm baby left
    if (noDetectionCount >= CONFIRM_NO_DETECTION && babyDetected) {
      babyDetected = false;
      buckleLooseCount = 0;
      welcomePlayed = false;
      debugPrint("Baby left");
    }
  }

  // ===== Buckle check (only when baby is sitting) =====
  if (babyDetected) {
    int hallValue = analogRead(HALL_PIN);
    bool buckleLoose = (hallValue > HALL_THRESHOLD);

    debugPrint("Hall: " + String(hallValue) + (buckleLoose ? " LOOSE" : " OK"));

    if (buckleLoose) {
      buckleLooseCount++;
      debugPrint("Loose count: " + String(buckleLooseCount) + "/" + String(REMINDER_CYCLES));

      // 30 seconds passed (10 cycles × 3 seconds)
      if (buckleLooseCount >= REMINDER_CYCLES) {
        debugPrint("Playing reminder!");
        playTrack(TRACK_REMINDER);
        buckleLooseCount = 0;  // Reset for next reminder cycle
      }
    } else {
      // Buckle fastened - reset counter
      if (buckleLooseCount > 0) {
        debugPrint("Buckle fastened!");
      }
      buckleLooseCount = 0;
    }
  }
}

// ==================== Helper Functions ====================

bool isNightMode() {
  if (!rtc.begin()) {
    debugPrint("RTC not found - night mode disabled");
    return false;
  }

  DateTime now = rtc.now();
  int hour = now.hour();

  debugPrint("Current time: " + String(hour) + ":" + String(now.minute()));

  // Night mode: 22:00 (10pm) to 7:00 (7am)
  // This means: hour >= 22 OR hour < 7
  if (hour >= NIGHT_START_HOUR || hour < NIGHT_END_HOUR) {
    return true;
  }
  return false;
}

float readUltrasonic() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return -1;

  return duration * 0.0343 / 2;
}

void playTrack(int track) {
  // Initialize DFPlayer
  dfPlayerSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX, DFPLAYER_TX);
  delay(200);

  if (dfPlayer.begin(dfPlayerSerial)) {
    dfPlayer.volume(25);
    delay(100);
    dfPlayer.play(track);
    delay(3000);  // Wait for audio to play
    debugPrint("Track " + String(track) + " played");
  } else {
    debugPrint("DFPlayer not found!");
  }
}

void goToSleep(uint64_t sleepUs = SLEEP_INTERVAL_US) {
  debugPrint("Sleeping for " + String((uint32_t)(sleepUs / 1000000)) + " seconds...\n");
  if (DEBUG_MODE) Serial.flush();

  esp_sleep_enable_timer_wakeup(sleepUs);
  esp_deep_sleep_start();
}

void debugPrint(String msg) {
  // Add timestamp if RTC is available
  String logLine = "";
  if (rtc.begin()) {
    DateTime now = rtc.now();
    char timestamp[20];
    sprintf(timestamp, "[%02d:%02d:%02d] ", now.hour(), now.minute(), now.second());
    logLine = String(timestamp) + msg;
  } else {
    logLine = msg;
  }

  // Print to Serial if connected
  if (DEBUG_MODE) {
    Serial.println(logLine);
  }

  // Write to flash
  if (FLASH_LOG_ENABLED) {
    logToFlash(logLine);
  }
}

void logToFlash(String msg) {
  // Check file size, clear if too large
  File f = LittleFS.open(LOG_FILE, "r");
  if (f) {
    if (f.size() > MAX_LOG_SIZE) {
      f.close();
      LittleFS.remove(LOG_FILE);
    } else {
      f.close();
    }
  }

  // Append to log file
  f = LittleFS.open(LOG_FILE, "a");
  if (f) {
    f.println(msg);
    f.close();
  }
}

void printLogs() {
  Serial.println("\n========== SAVED LOGS ==========");
  File f = LittleFS.open(LOG_FILE, "r");
  if (f) {
    while (f.available()) {
      Serial.write(f.read());
    }
    f.close();
    Serial.println("========== END OF LOGS ==========\n");
  } else {
    Serial.println("No logs found.");
  }
}

void clearLogs() {
  LittleFS.remove(LOG_FILE);
  Serial.println("Logs cleared.");
}
