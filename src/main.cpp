/*
 * analog-hardware — RESTING/THOUGHTS state machine, on-screen QR deep link.
 *
 * The panel ALWAYS shows a scannable QR of a valid sms: deep link:
 *   RESTING : sms:+VENUE&body=GREETING_BODY            (no token line)
 *   THOUGHTS: sms:+VENUE&body=GREETING_BODY%0A<token>  (token on line 2)
 *
 * onTransaction() (serial `txn` or a changed polled token) renders the THOUGHTS
 * QR. After THOUGHTS_TIMEOUT_MS the loop reverts to RESTING (no-token QR). The
 * QR encodes the byte-identical string the NFC tag used to carry; inbound
 * matching (reconcile-tap) is unchanged. Every render is a full refresh, which
 * fully clears the prior code (no module ghosting).
 */

#include <Arduino.h>
#include "config.h"
#include "smsurl.h"
#include "display.h"
#include "net.h"

enum DisplayState { RESTING, THOUGHTS };
static DisplayState state = RESTING;
static uint32_t lastTxnAt = 0;         // millis() of the last transaction (arms the timeout)
static uint16_t tokenCounter = 0;

// Build the sms: deep link and render it as a centered QR. token == null/empty
// -> RESTING payload (no token line). Reuses buildSmsUrl verbatim; only the
// render surface differs from the old NFC path.
static void showQr(const char* token) {
  char url[256];
  buildSmsUrl(VENUE_NUMBER, GREETING_BODY, token, url, sizeof(url));
  Serial.printf("[qr] %s\n", url);
  displayRenderQr(url);
}

// Revert to RESTING: no-token QR.
static void goResting() {
  showQr(nullptr);
  state = RESTING;
  Serial.println("[state] RESTING");
}

// A transaction landed (serial or poll): render the token QR and (re)arm the
// timeout. A new token produces a visibly different QR; the full refresh clears
// the prior code so there is never a ghost of two codes.
static void onTransaction(const char* token) {
  Serial.printf("\n[txn] token=%s\n", token);
  showQr(token);
  state = THOUGHTS;
  lastTxnAt = millis();
  Serial.println("[state] THOUGHTS");
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  displayBegin();

  // Boot into RESTING: no-token QR on screen.
  showQr(nullptr);
  state = RESTING;

  netLoadCreds();
  netBegin();

  Serial.println("\n--- analog-hardware: on-screen QR state machine ready ---");
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
