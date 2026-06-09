#pragma once
#include <Arduino.h>

// NTAG I2C Plus (NT3H2111) at I2C addr 0x55 on the reTerminal J2 bus.
// SCL = GPIO20, SDA = GPIO19.

// Bring up the I2C bus. Call once in setup().
void nfcBegin();

// Rewrite the tag with a single NDEF URI record (typically an sms: URL).
// Re-asserts the Capability Container if needed. Returns true on success.
// Fast (~tens of ms) — call this BEFORE the blocking e-ink refresh.
bool nfcWriteSmsUrl(const char* url);
