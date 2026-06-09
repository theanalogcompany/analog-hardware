/* One-time recovery: NTAG drifted to 0x02 -> put it back to 0x55.
 * reTerminal E1002: I2C on GPIO19 (SDA) / GPIO20 (SCL). */
#include <Wire.h>
#define CUR_ADDR  0x02      // where the scan shows it now
#define I2C_SDA   19
#define I2C_SCL   20
const uint8_t CC[4] = { 0xE1, 0x10, 0x6D, 0x00 };

void setup() {
  Serial.begin(115200);
  delay(1500);
  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.println("\n--- NTAG address recovery ---");

  uint8_t b0[16];
  Wire.beginTransmission(CUR_ADDR);
  Wire.write(0x00);
  if (Wire.endTransmission(false) != 0) { Serial.println("no ACK at 0x02 - re-scan"); return; }
  if (Wire.requestFrom((uint8_t)CUR_ADDR, (uint8_t)16) != 16) { Serial.println("read failed"); return; }
  for (int i = 0; i < 16; i++) b0[i] = Wire.read();   // byte0 reads as 0x04, that's expected

  b0[0]  = 0xAA;                                       // restores 7-bit addr 0x55
  b0[12]=CC[0]; b0[13]=CC[1]; b0[14]=CC[2]; b0[15]=CC[3];  // keep CC valid; bytes 1..11 (UID/lock) preserved

  Wire.beginTransmission(CUR_ADDR);                   // still 0x02 at this moment
  Wire.write(0x00);
  for (int i = 0; i < 16; i++) Wire.write(b0[i]);
  uint8_t rc = Wire.endTransmission();
  Serial.printf("write rc=%d (0=ok)\n", rc);

  delay(10);                                          // EEPROM commit (datasheet wants ~4-5ms min)

  Wire.beginTransmission(0x55);
  Serial.println(Wire.endTransmission() == 0 ? "RECOVERED: ACKs at 0x55" : "not yet - re-run / re-scan");
}
void loop() {}