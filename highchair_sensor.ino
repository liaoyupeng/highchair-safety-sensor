/*
 * High Chair Safety Sensor / 婴儿座椅安全带提醒器
 *
 * Hardware:
 * - ESP32-C3 Supermini
 * - HC-SR04 Ultrasonic Sensor
 * - 49E Linear Hall Effect Sensor
 * - DFPlayer Mini + Speaker
 *
 * Power Saving:
 * - Always uses deep sleep, wakes every 3 seconds to check
 * - Uses wake cycle counting for 30-second timer
 * - Extremely low average power consumption
 *
 * Author: Yupeng Liao
 * License: MIT
 */

#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include "esp_sleep.h"

// ==================== Pin Definitions ====================
#define TRIG_PIN 2      // Ultrasonic Trig
#define ECHO_PIN 3      // Ultrasonic Echo
#define HALL_PIN 1      // Hall sensor (ADC)
#define DFPLAYER_RX 20  // ESP32 RX <- DFPlayer TX
#define DFPLAYER_TX 21  // ESP32 TX -> DFPlayer RX

// ==================== Configuration ====================
#define ULTRASONIC_THRESHOLD_CM 30    // Distance to detect baby
#define HALL_THRESHOLD 2000           // Hall sensor threshold (adjust!)
#define SLEEP_INTERVAL_US 3000000     // 3 seconds between checks
#define REMINDER_CYCLES 10            // 10 cycles × 3s = 30 seconds

// Audio tracks (on TF card: /01/001.mp3, /01/002.mp3)
#define TRACK_WELCOME 1
#define TRACK_REMINDER 2

// Debug mode - set to false to disable Serial output
#define DEBUG_MODE true

// ==================== RTC Memory (persists across sleep) ====================
RTC_DATA_ATTR bool babyDetected = false;
RTC_DATA_ATTR int buckleLooseCount = 0;      // Counts cycles with loose buckle
RTC_DATA_ATTR int detectionCount = 0;        // Counts consecutive baby detections
RTC_DATA_ATTR int noDetectionCount = 0;      // Counts consecutive no-detections
RTC_DATA_ATTR bool welcomePlayed = false;

// Thresholds for confirming state changes
#define CONFIRM_DETECTION 2       // 2 cycles to confirm baby
#define CONFIRM_NO_DETECTION 2    // 2 cycles to confirm baby left

// ==================== Global Variables ====================
HardwareSerial dfPlayerSerial(1);
DFRobotDFPlayerMini dfPlayer;

// ==================== Setup ====================
void setup() {
  if (DEBUG_MODE) {
    Serial.begin(115200);
    delay(100);
  }

  // Initialize pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(HALL_PIN, INPUT);

  // Check wake reason
  esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
  if (reason != ESP_SLEEP_WAKEUP_TIMER) {
    // Fresh boot - reset all state
    debugPrint("Fresh boot - initializing");
    babyDetected = false;
    buckleLooseCount = 0;
    detectionCount = 0;
    noDetectionCount = 0;
    welcomePlayed = false;
  } else {
    debugPrint("Woke from sleep");
  }

  // Main detection logic
  checkSensors();

  // Go back to sleep
  goToSleep();
}

void loop() {
  // Never reaches here - device sleeps after setup()
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

void goToSleep() {
  debugPrint("Sleeping...\n");
  if (DEBUG_MODE) Serial.flush();

  esp_sleep_enable_timer_wakeup(SLEEP_INTERVAL_US);
  esp_deep_sleep_start();
}

void debugPrint(String msg) {
  if (DEBUG_MODE) {
    Serial.println(msg);
  }
}
