/*
 * High Chair Safety Sensor / 婴儿座椅安全带提醒器
 *
 * Hardware:
 * - ESP32-C3 Supermini
 * - HC-SR04 Ultrasonic Sensor
 * - 49E Linear Hall Effect Sensor
 * - DFPlayer Mini + Speaker
 *
 * Features:
 * - Deep sleep mode for power saving (wakes every few seconds to check)
 * - Ultrasonic detection for baby presence
 * - Hall sensor for buckle status
 * - Audio reminders via DFPlayer Mini
 *
 * Author: Yupeng Liao
 * License: MIT
 */

#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include "esp_sleep.h"

// ==================== Pin Definitions ====================
// Ultrasonic Sensor (HC-SR04)
#define TRIG_PIN 2
#define ECHO_PIN 3

// Hall Effect Sensor (49E)
#define HALL_PIN 1  // ADC pin

// DFPlayer Mini (using Serial1)
#define DFPLAYER_RX 20  // ESP32 RX <- DFPlayer TX
#define DFPLAYER_TX 21  // ESP32 TX -> DFPlayer RX

// ==================== Configuration ====================
// Detection thresholds
#define ULTRASONIC_THRESHOLD_CM 30    // Distance to detect baby's feet
#define HALL_THRESHOLD 2000           // ADC value threshold (adjust based on testing)
#define BUCKLE_REMINDER_DELAY_MS 30000  // 30 seconds

// Sleep configuration
#define SLEEP_ENABLED true            // Set to false to disable sleep (for debugging)
#define SLEEP_INTERVAL_US 3000000     // 3 seconds between wake-ups when idle
#define ACTIVE_CHECK_INTERVAL_MS 100  // Check interval when baby is detected

// Audio track numbers (stored in /01/ folder on TF card)
#define TRACK_WELCOME 1     // 001.mp3 - Welcome music
#define TRACK_REMINDER 2    // 002.mp3 - Buckle reminder

// ==================== Global Variables ====================
HardwareSerial dfPlayerSerial(1);  // Use Serial1
DFRobotDFPlayerMini dfPlayer;

// Use RTC memory to persist state across deep sleep
RTC_DATA_ATTR bool babyDetected = false;
RTC_DATA_ATTR bool timerStarted = false;
RTC_DATA_ATTR unsigned long timerStartTime = 0;
RTC_DATA_ATTR bool welcomePlayed = false;
RTC_DATA_ATTR bool reminderPlayed = false;
RTC_DATA_ATTR int consecutiveDetections = 0;
RTC_DATA_ATTR int consecutiveNoDetections = 0;

// Debounce thresholds
#define DETECTION_CONFIRM_COUNT 2     // Need 2 consecutive detections to confirm
#define NO_DETECTION_CONFIRM_COUNT 3  // Need 3 consecutive no-detections to clear

bool dfPlayerReady = false;

// ==================== Setup ====================
void setup() {
  // Initialize Serial for debugging
  Serial.begin(115200);

  // Check wake-up reason
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("Woke up from timer");
  } else {
    Serial.println("High Chair Safety Sensor Starting...");
    // Fresh boot - reset state
    babyDetected = false;
    timerStarted = false;
    welcomePlayed = false;
    reminderPlayed = false;
    consecutiveDetections = 0;
    consecutiveNoDetections = 0;
  }

  // Initialize pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(HALL_PIN, INPUT);

  // Initialize DFPlayer
  initDFPlayer();
}

// ==================== Main Loop ====================
void loop() {
  // Read ultrasonic sensor
  float distance = readUltrasonic();
  bool currentlyDetected = (distance > 0 && distance < ULTRASONIC_THRESHOLD_CM);

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.print(" cm, Detected: ");
  Serial.println(currentlyDetected ? "YES" : "NO");

  // ========== State: No baby detected yet ==========
  if (!babyDetected) {
    if (currentlyDetected) {
      consecutiveDetections++;
      consecutiveNoDetections = 0;

      if (consecutiveDetections >= DETECTION_CONFIRM_COUNT) {
        // Confirmed: baby sat down
        babyDetected = true;
        welcomePlayed = false;
        reminderPlayed = false;
        timerStarted = false;
        Serial.println("Baby confirmed! Playing welcome music.");
        playTrack(TRACK_WELCOME);
        welcomePlayed = true;
      }
    } else {
      consecutiveDetections = 0;
    }

    // No baby - go to deep sleep to save power
    if (!babyDetected && SLEEP_ENABLED) {
      goToSleep();
    }
  }

  // ========== State: Baby is sitting ==========
  if (babyDetected) {
    // Check if baby left
    if (!currentlyDetected) {
      consecutiveNoDetections++;
      consecutiveDetections = 0;

      if (consecutiveNoDetections >= NO_DETECTION_CONFIRM_COUNT) {
        // Confirmed: baby left
        babyDetected = false;
        timerStarted = false;
        welcomePlayed = false;
        reminderPlayed = false;
        Serial.println("Baby left the chair.");

        // Go back to sleep mode
        if (SLEEP_ENABLED) {
          goToSleep();
        }
      }
    } else {
      consecutiveNoDetections = 0;
    }

    // Check buckle status
    int hallValue = analogRead(HALL_PIN);
    bool buckleLoose = (hallValue > HALL_THRESHOLD);

    Serial.print("Hall: ");
    Serial.print(hallValue);
    Serial.print(", Buckle: ");
    Serial.println(buckleLoose ? "LOOSE" : "FASTENED");

    // Buckle is loose - start or continue timer
    if (buckleLoose) {
      if (!timerStarted) {
        timerStarted = true;
        timerStartTime = millis();
        reminderPlayed = false;
        Serial.println("Buckle loose. Starting 30s timer.");
      }

      // Check if timer expired
      if (timerStarted && !reminderPlayed) {
        unsigned long elapsed = millis() - timerStartTime;
        if (elapsed > BUCKLE_REMINDER_DELAY_MS) {
          Serial.println("Timer expired. Playing reminder.");
          playTrack(TRACK_REMINDER);
          reminderPlayed = true;
          // Reset for next reminder cycle
          timerStartTime = millis();
        }
      }
    } else {
      // Buckle fastened - cancel timer
      if (timerStarted) {
        Serial.println("Buckle fastened! Timer cancelled.");
        timerStarted = false;
        reminderPlayed = false;
      }
    }

    // Stay awake and check frequently while baby is sitting
    delay(ACTIVE_CHECK_INTERVAL_MS);
  }
}

// ==================== Functions ====================

/**
 * Initialize DFPlayer Mini
 */
void initDFPlayer() {
  dfPlayerSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX, DFPLAYER_TX);
  delay(100);

  if (dfPlayer.begin(dfPlayerSerial)) {
    Serial.println("DFPlayer ready.");
    dfPlayer.volume(25);  // Set volume (0-30)
    dfPlayerReady = true;
  } else {
    Serial.println("DFPlayer not found - continuing without audio.");
    dfPlayerReady = false;
  }
}

/**
 * Read distance from ultrasonic sensor
 * Returns distance in cm, or -1 if out of range
 */
float readUltrasonic() {
  // Send trigger pulse
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Read echo pulse duration
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);  // 30ms timeout

  if (duration == 0) {
    return -1;  // No echo received
  }

  // Calculate distance: speed of sound = 343 m/s = 0.0343 cm/us
  float distance = duration * 0.0343 / 2;
  return distance;
}

/**
 * Play a track from the TF card
 */
void playTrack(int trackNum) {
  if (!dfPlayerReady) {
    initDFPlayer();  // Try to reinitialize after sleep
  }

  if (dfPlayerReady) {
    dfPlayer.play(trackNum);
    delay(500);  // Wait for playback to start
  }
}

/**
 * Enter deep sleep mode to save power
 * Wakes up after SLEEP_INTERVAL_US microseconds
 */
void goToSleep() {
  Serial.println("Going to sleep...");
  Serial.flush();

  // Configure wake-up timer
  esp_sleep_enable_timer_wakeup(SLEEP_INTERVAL_US);

  // Enter deep sleep
  esp_deep_sleep_start();

  // Code never reaches here - device resets on wake
}
