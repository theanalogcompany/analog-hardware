/*
 * analog-hardware — Stage 1: serial-triggered transaction loop.
 *
 * Type `txn` in the serial monitor to fake a transaction. Each one mints a fresh
 * token, rewrites the NFC tag with an sms: URL carrying it, then flips the e-ink
 * to the "we have thoughts" state. Proves the whole physical loop end-to-end,
 * no network. Network/Square/real-art come in later stages.
 */

#include <Arduino.h>   // PlatformIO needs this explicitly; Arduino IDE added it for you
#include "config.h"
#include "nfc.h"
#include "display.h"

static uint16_t tokenCounter = 0;

// Fake transaction: build the sms: URL with this token, rewrite the tag, update
// the screen. NFC write FIRST (instant) — the e-ink refresh blocks ~15s, so it
// must come last or it would stall the tag rewrite.
static void onTransaction(const char* token) {
  char url[256];
  snprintf(url, sizeof(url), "sms:%s&body=%s%%0A%s", VENUE_NUMBER, GREETING_BODY, token);

  Serial.printf("\n[txn] token=%s\n", token);
  Serial.printf("[txn] url=%s\n", url);

  if (nfcWriteSmsUrl(url)) {
    Serial.println("[txn] tag rewritten — refreshing screen (~15s)...");
  } else {
    Serial.println("[txn] NFC write FAILED — refreshing screen anyway.");
  }

  displayThoughts();
  Serial.println("[txn] done. Type txn for the next one.");
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  nfcBegin();
  displayBegin();
  displayIdle();

  Serial.println("\n--- analog-hardware: Stage 1 ready ---");
  Serial.printf("Device %s -> %s\n", DEVICE_ID, VENUE_NUMBER);
  Serial.println("Type `txn` and hit enter to fake a transaction.");
}

void loop() {
  static char line[64];
  static uint8_t len = 0;

  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (len == 0) continue;             // ignore blank lines / CRLF pairs
      line[len] = '\0';
      len = 0;
      if (strcmp(line, "txn") == 0) {
        char token[8];
        snprintf(token, sizeof(token), "t%03u", ++tokenCounter);
        onTransaction(token);
      } else {
        Serial.printf("unknown command: '%s' (try `txn`)\n", line);
      }
    } else if (len < sizeof(line) - 1) {
      line[len++] = c;
    }
  }
}
