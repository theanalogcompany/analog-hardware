/*
 * analog-hardware — RESTING/THOUGHTS state machine, always-tappable tag.
 *
 * The NFC tag ALWAYS carries a valid sms: payload:
 *   RESTING : sms:+VENUE&body=GREETING_BODY            (no token line)
 *   THOUGHTS: sms:+VENUE&body=GREETING_BODY%0A<token>  (token on line 2)
 *
 * onTransaction() (serial `txn` or a changed polled token) writes the THOUGHTS
 * payload and flips the screen (partial). After THOUGHTS_TIMEOUT_MS the loop
 * reverts to RESTING (full refresh, no ghost) and rewrites the no-token tag.
 * The NDEF write path (nfc.cpp) is reused exactly — only the payload varies.
 */

#include <Arduino.h>   // PlatformIO needs this explicitly; Arduino IDE added it for you
#include "config.h"
#include "nfc.h"
#include "display.h"
#include "net.h"

enum DisplayState { RESTING, THOUGHTS };
static DisplayState state = RESTING;
static uint32_t lastTxnAt = 0;         // millis() of the last transaction (arms the timeout)
static uint16_t tokenCounter = 0;

// Build the sms: payload and write the tag. token == null/empty -> no token line
// (RESTING payload). Reuses the existing NDEF write path exactly; only the
// payload string varies.
static bool writeTag(const char* token) {
  char url[256];
  if (token && token[0] != '\0')
    snprintf(url, sizeof(url), "sms:%s&body=%s%%0A%s", VENUE_NUMBER, GREETING_BODY, token);
  else
    snprintf(url, sizeof(url), "sms:%s&body=%s", VENUE_NUMBER, GREETING_BODY);
  Serial.printf("[tag] %s\n", url);
  return nfcWriteSmsUrl(url);
}

// Revert to RESTING: no-token tag + resting image (FULL refresh clears any
// partial-update ghosting).
static void goResting() {
  writeTag(nullptr);
  displayIdle();
  state = RESTING;
  Serial.println("[state] RESTING");
}

// A transaction landed (serial or poll). Always rewrite the tag with the new
// token first (I2C, instant), then refresh the screen:
//   RESTING  -> THOUGHTS : PARTIAL refresh (fast, reads as responsive).
//   THOUGHTS (re-fire)   : FULL refresh re-show — a partial of identical content
//                          would be invisible; the flash is the "updated for you"
//                          cue for the next guest. (Intentional, do NOT skip.)
// Either way the timeout window restarts.
static void onTransaction(const char* token) {
  Serial.printf("\n[txn] token=%s\n", token);
  writeTag(token);
  if (state == RESTING) {
    displayThoughts();           // partial flip
    state = THOUGHTS;
    Serial.println("[state] RESTING -> THOUGHTS (partial)");
  } else {
    displayThoughtsFull();       // visible full-refresh re-show
    Serial.println("[state] THOUGHTS re-fire (full re-show)");
  }
  lastTxnAt = millis();
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  nfcBegin();
  displayBegin();

  // Boot into RESTING: tag tappable immediately, resting image on screen.
  writeTag(nullptr);
  displayIdle();
  state = RESTING;

  netLoadCreds();
  netBegin();

  Serial.println("\n--- analog-hardware: RESTING/THOUGHTS state machine ready ---");
  Serial.printf("Venue %s\n", VENUE_NUMBER);
  Serial.printf("Polling backend feed every %d ms.\n", POLL_INTERVAL_MS);
  Serial.printf("THOUGHTS auto-reverts to RESTING after %d ms.\n", THOUGHTS_TIMEOUT_MS);
  Serial.println("Commands: `txn` (fake txn) | `prov <id> <token>` | `whoami` | `clearcursor`");
}

void loop() {
  // Coalesce all transactions seen this pass (poll + any buffered serial lines)
  // into a single most-recent token -> one refresh. The GxEPD2 refresh blocks
  // ~2.3s; anything that arrives during it is drained on the NEXT pass and again
  // coalesced, so we never run back-to-back refreshes.
  char pendingToken[64];
  bool haveTxn = false;

  // --- network poll (millis()-based, never delay()) ---
  static uint32_t lastPoll = 0;
  static String lastToken = "";
  if (millis() - lastPoll >= POLL_INTERVAL_MS) {
    lastPoll = millis();
    String token;
    if (pollFeed(token) && token != lastToken) {
      Serial.printf("[poll] new event token: '%s' -> '%s'\n", lastToken.c_str(), token.c_str());
      lastToken = token;
      snprintf(pendingToken, sizeof(pendingToken), "%s", token.c_str());
      haveTxn = true;
    }
  }

  // --- manual serial override: drain ALL buffered input; if several `txn`s
  //     queued (e.g. during the last refresh), keep only the most recent ---
  static char line[128];
  static uint8_t len = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (len == 0) continue;             // ignore blank lines / CRLF pairs
      line[len] = '\0';
      len = 0;
      if (strcmp(line, "txn") == 0) {
        snprintf(pendingToken, sizeof(pendingToken), "t%03u", ++tokenCounter);
        haveTxn = true;
      } else if (strcmp(line, "whoami") == 0) {
        netWhoami();
      } else if (strcmp(line, "clearcursor") == 0) {
        netClearCursor();
      } else if (strncmp(line, "prov ", 5) == 0) {
        char* id = line + 5;
        char* sp = strchr(id, ' ');
        if (sp) {
          *sp = '\0';
          netProvision(id, sp + 1);
        } else {
          Serial.println("usage: prov <device_id> <token>");
        }
      } else {
        Serial.printf("unknown command: '%s' (try `txn`, `prov <id> <token>`, `whoami`, `clearcursor`)\n", line);
      }
    } else if (len < sizeof(line) - 1) {
      line[len++] = c;
    }
  }

  // One coalesced transaction -> one refresh.
  if (haveTxn) onTransaction(pendingToken);

  // --- auto-revert THOUGHTS -> RESTING after the timeout (millis-based, no delay).
  //     A txn just handled above re-armed lastTxnAt, so this won't double-fire. ---
  if (state == THOUGHTS && millis() - lastTxnAt >= THOUGHTS_TIMEOUT_MS) {
    Serial.println("[state] thoughts timed out");
    goResting();
  }
}
