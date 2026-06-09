/*
 * analog-hardware — Stage 2: polled token source.
 *
 * Connects to WiFi and polls a mock endpoint for the current token. When the
 * token changes, fires onTransaction() — the same path Stage 1 used: rewrite the
 * NFC tag with an sms: URL carrying the token, then flip the e-ink. Typing `txn`
 * in the serial monitor still works as a manual override.
 */

#include <Arduino.h>   // PlatformIO needs this explicitly; Arduino IDE added it for you
#include "config.h"
#include "nfc.h"
#include "display.h"
#include "net.h"

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
  netBegin();

  Serial.println("\n--- analog-hardware: Stage 2 ready ---");
  Serial.printf("Device %s -> %s\n", DEVICE_ID, VENUE_NUMBER);
  Serial.printf("Polling %s every %d ms.\n", TOKEN_URL, POLL_INTERVAL_MS);
  Serial.println("Type `txn` and hit enter to fake a transaction (manual override).");
}

void loop() {
  // --- network poll (millis()-based, never delay()) ---
  static uint32_t lastPoll = 0;
  static String lastToken = "";
  if (millis() - lastPoll >= POLL_INTERVAL_MS) {
    lastPoll = millis();
    String token;
    if (pollToken(token) && token != lastToken) {
      // Fire ONLY on change — firing every poll would thrash the panel with a
      // ~30s refresh each time.
      Serial.printf("[poll] token changed: '%s' -> '%s'\n", lastToken.c_str(), token.c_str());
      onTransaction(token.c_str());
      lastToken = token;
    }
  }

  // --- manual serial override ---
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
