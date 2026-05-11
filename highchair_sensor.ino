/*
 * High Chair Safety Sensor / 婴儿座椅安全带提醒器
 *
 * Hardware:
 * - ESP32-C3 Supermini
 * - HC-SR04 Ultrasonic Sensor
 * - 49E Linear Hall Effect Sensor
 * - DFPlayer Mini + Speaker
 *
 * Author: Yupeng Liao
 * License: MIT
 */

#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>

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

// Audio track numbers (stored in /01/ folder on TF card)
#define TRACK_WELCOME 1     // 001.mp3 - Welcome music
#define TRACK_REMINDER 2    // 002.mp3 - Buckle reminder

// ==================== Global Variables ====================
HardwareSerial dfPlayerSerial(1);  // Use Serial1
DFRobotDFPlayerMini dfPlayer;

bool babyDetected = false;
bool buckleLoose = false;
bool timerStarted = false;
unsigned long timerStartTime = 0;
bool welcomePlayed = false;
bool reminderPlayed = false;

// Debounce
unsigned long lastDetectionTime = 0;
#define DETECTION_DEBOUNCE_MS 1000

// ==================== Setup ====================
void setup() {
  // Initialize Serial for debugging
  Serial.begin(115200);
  Serial.println("High Chair Safety Sensor Starting...");

  // Initialize pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(HALL_PIN, INPUT);

  // Initialize DFPlayer
  dfPlayerSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX, DFPLAYER_TX);

  if (!dfPlayer.begin(dfPlayerSerial)) {
    Serial.println("DFPlayer Mini not detected!");
    Serial.println("Please check connections and TF card.");
    while (true) {
      delay(1000);  // Halt if DFPlayer not found
    }
  }

  Serial.println("DFPlayer Mini initialized.");
  dfPlayer.volume(25);  // Set volume (0-30)

  delay(1000);
  Serial.println("System ready!");
}

// ==================== Main Loop ====================
void loop() {
  // Read sensors
  float distance = readUltrasonic();
  int hallValue = analogRead(HALL_PIN);

  // Debug output
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.print(" cm, Hall: ");
  Serial.println(hallValue);

  // Check if baby is sitting (ultrasonic detects feet)
  bool currentBabyDetected = (distance > 0 && distance < ULTRASONIC_THRESHOLD_CM);

  // Baby just sat down
  if (currentBabyDetected && !babyDetected) {
    if (millis() - lastDetectionTime > DETECTION_DEBOUNCE_MS) {
      babyDetected = true;
      welcomePlayed = false;
      reminderPlayed = false;
      timerStarted = false;
      lastDetectionTime = millis();
      Serial.println("Baby detected! Playing welcome music.");
      playTrack(TRACK_WELCOME);
      welcomePlayed = true;
    }
  }

  // Baby left the chair
  if (!currentBabyDetected && babyDetected) {
    if (millis() - lastDetectionTime > DETECTION_DEBOUNCE_MS) {
      babyDetected = false;
      timerStarted = false;
      welcomePlayed = false;
      reminderPlayed = false;
      lastDetectionTime = millis();
      Serial.println("Baby left the chair.");
    }
  }

  // Check buckle status (only when baby is sitting)
  if (babyDetected) {
    // Hall sensor detects magnet = buckle is loose (not fastened)
    bool currentBuckleLoose = (hallValue > HALL_THRESHOLD);

    // Buckle became loose - start timer
    if (currentBuckleLoose && !timerStarted) {
      timerStarted = true;
      timerStartTime = millis();
      reminderPlayed = false;
      Serial.println("Buckle loose detected. Starting 30s timer.");
    }

    // Buckle fastened - cancel timer
    if (!currentBuckleLoose && timerStarted) {
      timerStarted = false;
      Serial.println("Buckle fastened! Timer cancelled.");
    }

    // Timer expired and buckle still loose - play reminder
    if (timerStarted && !reminderPlayed) {
      if (millis() - timerStartTime > BUCKLE_REMINDER_DELAY_MS) {
        Serial.println("30 seconds passed. Playing buckle reminder.");
        playTrack(TRACK_REMINDER);
        reminderPlayed = true;
        // Reset timer for repeated reminders every 30 seconds
        timerStartTime = millis();
      }
    }

    buckleLoose = currentBuckleLoose;
  }

  delay(100);  // Small delay between readings
}

// ==================== Functions ====================

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
  // Distance = (duration * 0.0343) / 2
  float distance = duration * 0.0343 / 2;

  return distance;
}

/**
 * Play a track from the TF card
 * Track numbers correspond to files: 001.mp3, 002.mp3, etc.
 */
void playTrack(int trackNum) {
  dfPlayer.play(trackNum);
  delay(500);  // Wait for playback to start
}
