/*
 * I2C scanner for reTerminal E1002
 * Confirms the NFC Tag 2 Click (NTAG I2C Plus) is wired correctly.
 * Expected: 0x44 (onboard SHT4x temp/humidity sensor) AND 0x55 (the NTAG chip).
 *
 * Board: XIAO_ESP32S3 | USB CDC On Boot: Disabled | Upload speed: 115200
 */

#include <Wire.h>

void setup() {
  Serial.begin(115200);
  delay(1500);
  Wire.begin(19, 20);          // SDA = GPIO19, SCL = GPIO20 on the reTerminal header
  Serial.println("\nI2C scanner starting...");
}

void loop() {
  int found = 0;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  device found at 0x%02X\n", addr);
      found++;
    }
  }
  if (found == 0) Serial.println("  no devices found - check wiring");
  else            Serial.printf("  %d device(s) total\n", found);
  Serial.println("---");
  delay(3000);
}
