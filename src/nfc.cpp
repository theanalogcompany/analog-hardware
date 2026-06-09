/*
 * Analog NFC — write a static sms: URL to the NTAG I2C Plus (NT3H2111).
 *
 * Ported from tools/analog_nfc_write_sms.ino into nfcBegin()/nfcWriteSmsUrl().
 *
 * Block 0 quirks (both fixes carried over verbatim):
 *  - The NTAG's I2C address lives in byte 0 of block 0, but the chip ALWAYS
 *    reports 0x04 on a read. So we force b0[0] = 0xAA (= addr 0x55) before the
 *    block-0 write and NEVER write back the 0x04 it reports (that drifts it to 0x02).
 *  - Block 0 is committed with a plain single write, NOT the verify-retry path:
 *    byte 0 reads back as 0x04, so verify-retry would false-fail a good write.
 *    NDEF blocks (1+) keep the verify-retry.
 */

#include "nfc.h"
#include <Wire.h>

#define NTAG_ADDR 0x55
#define I2C_SDA   19
#define I2C_SCL   20

static const uint8_t CC[4] = { 0xE1, 0x10, 0x6D, 0x00 };

// ---- low-level 16-byte block read / write over I2C ----
static bool readBlock(uint8_t block, uint8_t* buf) {
  Wire.beginTransmission(NTAG_ADDR);
  Wire.write(block);
  if (Wire.endTransmission(false) != 0) return false;
  uint8_t n = Wire.requestFrom((uint8_t)NTAG_ADDR, (uint8_t)16);
  if (n != 16) return false;
  for (int i = 0; i < 16; i++) buf[i] = Wire.read();
  return true;
}

static bool writeBlock(uint8_t block, const uint8_t* buf) {
  // NT3H2x11 NAKs while committing the previous write to EEPROM.
  // Write, let it commit, read back to confirm, retry until it sticks.
  // (Only used for blocks 1+, where byte 0 isn't involved.)
  for (int attempt = 0; attempt < 12; attempt++) {
    Wire.beginTransmission(NTAG_ADDR);
    Wire.write(block);
    for (int i = 0; i < 16; i++) Wire.write(buf[i]);
    if (Wire.endTransmission() == 0) {
      delay(8);
      uint8_t chk[16];
      if (readBlock(block, chk)) {
        bool same = true;
        for (int i = 0; i < 16; i++) if (chk[i] != buf[i]) { same = false; break; }
        if (same) return true;
      }
    }
    delay(10);
  }
  return false;
}

static void printBlock(const char* label, uint8_t* b) {
  Serial.printf("%-16s", label);
  for (int i = 0; i < 16; i++) Serial.printf(" %02X", b[i]);
  Serial.println();
}

void nfcBegin() {
  Wire.begin(I2C_SDA, I2C_SCL);
}

bool nfcWriteSmsUrl(const char* url) {
  // 1) Capability Container — read block 0, only write it if the CC isn't set.
  uint8_t b0[16];
  if (!readBlock(0x00, b0)) {
    Serial.println("NFC ERROR: can't read block 0. Check wiring / that 0x55 shows on the scanner.");
    return false;
  }
  printBlock("block0 (before)", b0);

  bool ccOk = (b0[12]==CC[0] && b0[13]==CC[1] && b0[14]==CC[2] && b0[15]==CC[3]);
  if (!ccOk) {
    b0[0] = 0xAA;                              // re-assert addr 0x55; NEVER write back the 0x04 it reports
    b0[12]=CC[0]; b0[13]=CC[1]; b0[14]=CC[2]; b0[15]=CC[3];
    // Plain write, NOT writeBlock(): byte 0 always reads back as 0x04, so the
    // verify-retry would always "fail" on a good write. Single write + commit delay.
    Wire.beginTransmission(NTAG_ADDR);
    Wire.write(0x00);
    for (int i = 0; i < 16; i++) Wire.write(b0[i]);
    if (Wire.endTransmission() != 0) { Serial.println("NFC ERROR: CC write failed"); return false; }
    delay(10);                                 // EEPROM commit
    Serial.println("Capability Container written.");
  } else {
    Serial.println("Capability Container already OK.");
  }

  // 2) Build the NDEF message: a single short URI record wrapped in an NDEF TLV.
  uint8_t uriLen     = strlen(url);
  uint8_t payloadLen = 1 + uriLen;
  uint8_t ndefLen    = 4 + payloadLen;
  uint8_t total      = 2 + ndefLen + 1;

  uint8_t msg[256];
  memset(msg, 0, sizeof(msg));
  int i = 0;
  msg[i++] = 0x03;        // NDEF Message TLV tag
  msg[i++] = ndefLen;     // TLV length
  msg[i++] = 0xD1;        // NDEF rec: MB+ME+SR, TNF=Well-Known
  msg[i++] = 0x01;        // type length
  msg[i++] = payloadLen;  // payload length
  msg[i++] = 0x55;        // type = 'U' (URI record)
  msg[i++] = 0x00;        // URI prefix code: none (full URI follows)
  memcpy(&msg[i], url, uriLen); i += uriLen;
  msg[i++] = 0xFE;        // Terminator TLV

  // 3) Write to user memory, starting at block 1.
  uint8_t blocksNeeded = (total + 15) / 16;
  for (uint8_t blk = 0; blk < blocksNeeded; blk++) {
    uint8_t buf[16];
    memset(buf, 0, 16);
    for (int j = 0; j < 16; j++) {
      int idx = blk * 16 + j;
      if (idx < total) buf[j] = msg[idx];
    }
    if (!writeBlock(0x01 + blk, buf)) {
      Serial.printf("NFC ERROR: write to block %d failed\n", 1 + blk);
      return false;
    }
  }
  Serial.printf("Wrote NDEF (%d bytes) across %d block(s).\n", total, blocksNeeded);

  // 4) Read back so you can eyeball it.
  uint8_t rb[16];
  readBlock(0x00, rb); printBlock("block0 (after)", rb);
  for (uint8_t blk = 0; blk < blocksNeeded; blk++) {
    char lbl[16]; sprintf(lbl, "block%d", 1 + blk);
    readBlock(0x01 + blk, rb); printBlock(lbl, rb);
  }
  return true;
}
