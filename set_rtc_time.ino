/*
 * DS3231 RTC Time Setter for ESP32-C3 Supermini
 *
 * Upload this sketch ONCE to set the RTC time,
 * then upload the main highchair_sensor.ino code.
 *
 * Option 1: Uses compile time (__DATE__, __TIME__) -
 *           compile and upload immediately for best accuracy
 * Option 2: Set manually by uncommenting the manual line
 */

#include <RTClib.h>
#include <Wire.h>

RTC_DS3231 rtc;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== DS3231 RTC Time Setter ===\n");

  Wire.begin(8, 9);  // SDA=GPIO8, SCL=GPIO9

  if (!rtc.begin()) {
    Serial.println("ERROR: RTC not found! Check wiring:");
    Serial.println("  SDA -> GPIO8");
    Serial.println("  SCL -> GPIO9");
    Serial.println("  VCC -> 3.3V");
    Serial.println("  GND -> GND");
    while (1) delay(1000);
  }

  Serial.println("RTC found!");

  // Show current RTC time before setting
  DateTime before = rtc.now();
  Serial.print("Current RTC time: ");
  printDateTime(before);

  // === Set time to compile time ===
  // Compile and upload IMMEDIATELY for best accuracy
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // === OR set manually (uncomment below, comment out the line above) ===
  // rtc.adjust(DateTime(2026, 5, 12, 18, 37, 0));  // YYYY, MM, DD, HH, MM, SS

  // Verify
  DateTime after = rtc.now();
  Serial.print("New RTC time:     ");
  printDateTime(after);

  Serial.println("\nDone! RTC time is set.");
  Serial.println("Now upload highchair_sensor.ino as the main program.");
}

void loop() {
  // Print time every second to verify RTC is ticking
  DateTime now = rtc.now();
  printDateTime(now);
  delay(1000);
}

void printDateTime(DateTime dt) {
  char buf[25];
  sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d",
    dt.year(), dt.month(), dt.day(),
    dt.hour(), dt.minute(), dt.second());
  Serial.println(buf);
}
