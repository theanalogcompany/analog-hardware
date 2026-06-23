# On-Screen QR Deep Link Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the dead NTAG NFC tap with a centered QR code on the e-ink panel that encodes the *exact same* `sms:` deep link the tag carried, so a scan opens Messages prefilled with the `tt_` token intact.

**Architecture:** Extract the existing inline `sms:` string assembly from `writeTag` into a pure, host-testable `buildSmsUrl` builder. Add a portable QR encode-to-1bpp-bitmap module (`qr.cpp`, ricmoo/QRCode). Add a `displayRenderQr` sink that paints the QR centered on a blank white screen via a full-window paged refresh. Rewire `main.cpp` to build the string and render the QR instead of writing the tag. Delete the NFC path entirely. No artwork, no portrait, no label — blank white + centered QR. Inbound matching (`reconcile-tap.ts`) is untouched: the QR carries the byte-identical string, so the same `tt_` token round-trips.

**Tech Stack:** PlatformIO / Arduino (ESP32-S3, Seeed XIAO), GxEPD2 (GDEY075T7 / UC8179 mono), Adafruit GFX, ricmoo/QRCode, Unity (native unit tests).

## Global Constraints

- **String is verbatim.** `buildSmsUrl` reproduces the current `writeTag` `snprintf` output byte-for-byte. THOUGHTS variant: `sms:<venue>&body=<greeting>%0A<token>`. RESTING variant (null/empty token): `sms:<venue>&body=<greeting>`. No re-encoding, no new token scheme, no new column.
- **Config stays in `config.h`.** `VENUE_NUMBER` / `GREETING_BODY` remain firmware `#define`s; the caller passes them into `buildSmsUrl`. Node never touches this path.
- **QR version is a single named constant** (`QR_VERSION` in `qr.h`) with a comment stating the byte-capacity ceiling. Greeting growth past the ceiling is a visible one-line version bump, never a silent truncation.
- **Capacity guard: log-and-skip.** If `strlen(url) > QR_BYTE_CAPACITY`, never emit a truncated QR — log and leave the screen unchanged.
- **NFC fully removed**, no coexisting fallback: delete `nfcWriteSmsUrl` + `nfcBegin` calls, remove `src/nfc.cpp` / `src/nfc.h` and includes. Leave `tools/analog_nfc_write_sms.ino` untouched.
- **No artwork.** Blank white screen, QR centered on the 800×480 panel. `images.h` becomes dead and is removed.
- **No waveform/LUT/init changes.** Uses the existing `displayBegin()` init and standard GxEPD2 paged full-window refresh only. (No `[HUMAN-REVIEW-REQUIRED]` flag warranted.)
- **No flashing, no commit in this session.** Build verification only; flashing + UAT is the final manual task, gated on human go-ahead.

---

## File Structure

- **Create** `src/smsurl.h` / `src/smsurl.cpp` — pure `buildSmsUrl(...)`. No Arduino deps (host-testable).
- **Create** `src/qr.h` / `src/qr.cpp` — `qrEncodeToBitmap(...)` + QR geometry constants. Depends only on ricmoo/QRCode + `<cstring>` (no Arduino, host-testable).
- **Create** `test/test_native/test_main.cpp` — Unity unit tests for `buildSmsUrl` + capacity guard.
- **Modify** `src/display.h` / `src/display.cpp` — drop artwork show-functions, add `displayRenderQr(const char* url)`. Keep `displayBegin()` unchanged.
- **Modify** `src/main.cpp` — replace `writeTag` with `buildSmsUrl` + `displayRenderQr`; remove NFC; drop the partial/full re-fire branching.
- **Modify** `platformio.ini` — add `ricmoo/QRCode` to the device env (pending approval) and a `[env:native]` test env.
- **Delete** `src/nfc.cpp`, `src/nfc.h`, `src/images.h`.

---

### Task 1: Extract the pure `buildSmsUrl` builder + native test harness

**Files:**
- Create: `src/smsurl.h`, `src/smsurl.cpp`
- Create: `test/test_native/test_main.cpp`
- Modify: `platformio.ini` (add `[env:native]`)

**Interfaces:**
- Produces: `void buildSmsUrl(const char* venue, const char* greetingBody, const char* token, char* out, size_t cap)` — writes the `sms:` deep link into `out`. `token == nullptr` or `""` → RESTING variant (no `%0A<token>` line).

- [ ] **Step 1: Add the native test env to `platformio.ini`**

Append below the existing `[env:seeed_xiao_esp32s3]` block:

```ini
; Host unit tests for the pure, Arduino-free modules (smsurl, qr).
[env:native]
platform = native
test_framework = unity
build_src_filter = +<smsurl.cpp> +<qr.cpp>
lib_deps = ricmoo/QRCode
```

- [ ] **Step 2: Write the header**

`src/smsurl.h`:

```cpp
#pragma once
#include <cstddef>

// Assemble the sms: deep link, byte-identical to the legacy NFC writeTag()
// payload. token == nullptr/"" -> RESTING variant (greeting only, no token line).
//   THOUGHTS: sms:<venue>&body=<greeting>%0A<token>
//   RESTING : sms:<venue>&body=<greeting>
// venue/greetingBody come from config.h (caller passes the macros). Pure: no I/O.
void buildSmsUrl(const char* venue, const char* greetingBody,
                 const char* token, char* out, size_t cap);
```

- [ ] **Step 3: Write the failing test**

`test/test_native/test_main.cpp`:

```cpp
#include <unity.h>
#include <cstring>
#include "smsurl.h"

static const char* VENUE   = "+15551234567";
static const char* GREET   = "we%20have%20thoughts%20on%20your%20order";
static const char* TOKEN   = "tt_ABCDEFGHIJKLMNOPQRSTUVWXYZ012345";

void test_build_with_token(void) {
  char url[256];
  buildSmsUrl(VENUE, GREET, TOKEN, url, sizeof(url));
  TEST_ASSERT_EQUAL_STRING(
    "sms:+15551234567&body=we%20have%20thoughts%20on%20your%20order"
    "%0Att_ABCDEFGHIJKLMNOPQRSTUVWXYZ012345",
    url);
}

void test_build_without_token_null(void) {
  char url[256];
  buildSmsUrl(VENUE, GREET, nullptr, url, sizeof(url));
  TEST_ASSERT_EQUAL_STRING(
    "sms:+15551234567&body=we%20have%20thoughts%20on%20your%20order", url);
}

void test_build_without_token_empty(void) {
  char url[256];
  buildSmsUrl(VENUE, GREET, "", url, sizeof(url));
  TEST_ASSERT_EQUAL_STRING(
    "sms:+15551234567&body=we%20have%20thoughts%20on%20your%20order", url);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_build_with_token);
  RUN_TEST(test_build_without_token_null);
  RUN_TEST(test_build_without_token_empty);
  return UNITY_END();
}

void setUp(void) {}
void tearDown(void) {}
```

- [ ] **Step 4: Run the test to verify it fails**

Run: `pio test -e native`
Expected: FAIL — link/compile error, `buildSmsUrl` undefined (`src/smsurl.cpp` not yet written).

- [ ] **Step 5: Write the minimal implementation**

`src/smsurl.cpp`:

```cpp
#include "smsurl.h"
#include <cstdio>

void buildSmsUrl(const char* venue, const char* greetingBody,
                 const char* token, char* out, size_t cap) {
  if (token && token[0] != '\0')
    snprintf(out, cap, "sms:%s&body=%s%%0A%s", venue, greetingBody, token);
  else
    snprintf(out, cap, "sms:%s&body=%s", venue, greetingBody);
}
```

> Note: `build_src_filter` in `[env:native]` also pulls `qr.cpp`, which does not exist until Task 2. Until then, narrow the filter to `+<smsurl.cpp>` for this task's test run, then restore `+<smsurl.cpp> +<qr.cpp>` in Task 2 Step 1. (If executing tasks in order, simplest is to set the filter to `+<smsurl.cpp>` now and add `+<qr.cpp>` in Task 2.)

- [ ] **Step 6: Run the test to verify it passes**

Run: `pio test -e native`
Expected: PASS — 3 tests, 0 failures.

- [ ] **Step 7: Commit** *(do not run in this session — listed for the executor)*

```bash
git add src/smsurl.h src/smsurl.cpp test/test_native/test_main.cpp platformio.ini
git commit -m "feat: extract pure buildSmsUrl + native unit tests"
```

---

### Task 2: QR encode-to-bitmap module + capacity guard

**Files:**
- Create: `src/qr.h`, `src/qr.cpp`
- Modify: `platformio.ini` (restore `build_src_filter` to include `qr.cpp`)
- Modify: `test/test_native/test_main.cpp` (add guard tests)

**Interfaces:**
- Consumes: ricmoo/QRCode (`qrcode.h`: `QRCode`, `qrcode_getBufferSize`, `qrcode_initText`, `qrcode_getModule`, `ECC_MEDIUM`).
- Produces:
  - Constants in `qr.h`: `QR_VERSION`, `QR_BYTE_CAPACITY`, `QR_MODULES`, `QR_QUIET_MODULES`, `QR_MODULE_PX`, `QR_PX`, `QR_ROW_BYTES`, `QR_BITMAP_BYTES`.
  - `bool qrEncodeToBitmap(const char* url, uint8_t* outBuf, size_t outCap)` — fills `outBuf` with a packed 1-bpp QR (MSB-first, bit set = dark module), `QR_PX × QR_PX`, 4-module quiet zone, each module scaled to `QR_MODULE_PX`. Returns `false` (buffer untouched) when `strlen(url) > QR_BYTE_CAPACITY` or `outCap < QR_BITMAP_BYTES`. Does not log (caller logs).

- [ ] **Step 1: Restore the native build filter**

In `platformio.ini` `[env:native]`, ensure:

```ini
build_src_filter = +<smsurl.cpp> +<qr.cpp>
```

- [ ] **Step 2: Write the header**

`src/qr.h`:

```cpp
#pragma once
#include <cstdint>
#include <cstddef>

// --- QR symbol parameters --------------------------------------------------
// Version 6 = 41x41 modules, ECC level M. Byte-mode capacity at V6-M = 122
// bytes. The sms: deep link is ~100 bytes today. If GREETING_BODY grows past
// QR_BYTE_CAPACITY, bump QR_VERSION (a visible one-line change) — we never
// silently truncate (see qrEncodeToBitmap's guard).
#define QR_VERSION        6
#define QR_BYTE_CAPACITY  122                 // V6, ECC_MEDIUM, byte mode
#define QR_MODULES        41                  // 17 + 4*QR_VERSION
#define QR_QUIET_MODULES  4
#define QR_MODULE_PX      6
#define QR_PX             ((QR_MODULES + 2 * QR_QUIET_MODULES) * QR_MODULE_PX) // 294
#define QR_ROW_BYTES      ((QR_PX + 7) / 8)                                    // 37
#define QR_BITMAP_BYTES   (QR_PX * QR_ROW_BYTES)                               // 10878

// Encode `url` into a packed 1-bpp bitmap (MSB-first, 1 = dark module),
// QR_PX x QR_PX, with a QR_QUIET_MODULES quiet zone, each module scaled to
// QR_MODULE_PX. Returns false (buffer untouched) if the url exceeds
// QR_BYTE_CAPACITY or outCap < QR_BITMAP_BYTES. Caller logs + skips the refresh.
bool qrEncodeToBitmap(const char* url, uint8_t* outBuf, size_t outCap);
```

- [ ] **Step 3: Write the failing tests**

Add to `test/test_native/test_main.cpp` (new includes + tests + `RUN_TEST` lines):

```cpp
#include "qr.h"

void test_qr_rejects_oversized_url(void) {
  char big[QR_BYTE_CAPACITY + 10];
  memset(big, 'x', sizeof(big) - 1);
  big[sizeof(big) - 1] = '\0';                 // length = QR_BYTE_CAPACITY + 9
  static uint8_t buf[QR_BITMAP_BYTES];
  memset(buf, 0xAB, sizeof(buf));
  TEST_ASSERT_FALSE(qrEncodeToBitmap(big, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_HEX8(0xAB, buf[0]);        // buffer untouched
}

void test_qr_rejects_small_buffer(void) {
  static uint8_t tiny[QR_BITMAP_BYTES - 1];
  TEST_ASSERT_FALSE(qrEncodeToBitmap("sms:+1555&body=hi", tiny, sizeof(tiny)));
}

void test_qr_encodes_typical_url(void) {
  const char* url =
    "sms:+15551234567&body=we%20have%20thoughts%20on%20your%20order"
    "%0Att_ABCDEFGHIJKLMNOPQRSTUVWXYZ012345";
  static uint8_t buf[QR_BITMAP_BYTES];
  memset(buf, 0, sizeof(buf));
  TEST_ASSERT_TRUE(qrEncodeToBitmap(url, buf, sizeof(buf)));
  // Some module must be dark -> at least one non-zero byte.
  bool anyDark = false;
  for (size_t i = 0; i < sizeof(buf); i++) if (buf[i]) { anyDark = true; break; }
  TEST_ASSERT_TRUE(anyDark);
}
```

And register them in `main()`:

```cpp
  RUN_TEST(test_qr_rejects_oversized_url);
  RUN_TEST(test_qr_rejects_small_buffer);
  RUN_TEST(test_qr_encodes_typical_url);
```

- [ ] **Step 4: Run the tests to verify they fail**

Run: `pio test -e native`
Expected: FAIL — `qrEncodeToBitmap` undefined (`src/qr.cpp` not yet written).

- [ ] **Step 5: Write the implementation**

`src/qr.cpp`:

```cpp
#include "qr.h"
#include "qrcode.h"     // ricmoo/QRCode
#include <cstring>

bool qrEncodeToBitmap(const char* url, uint8_t* outBuf, size_t outCap) {
  if (std::strlen(url) > QR_BYTE_CAPACITY) return false;   // never truncate
  if (outCap < QR_BITMAP_BYTES) return false;

  QRCode qrcode;
  uint8_t scratch[qrcode_getBufferSize(QR_VERSION)];
  if (qrcode_initText(&qrcode, scratch, QR_VERSION, ECC_MEDIUM, url) != 0)
    return false;

  std::memset(outBuf, 0, QR_BITMAP_BYTES);     // 0 = white
  const int q = QR_QUIET_MODULES;
  for (uint8_t my = 0; my < qrcode.size; my++) {
    for (uint8_t mx = 0; mx < qrcode.size; mx++) {
      if (!qrcode_getModule(&qrcode, mx, my)) continue;    // paint dark only
      const int px0 = (q + mx) * QR_MODULE_PX;
      const int py0 = (q + my) * QR_MODULE_PX;
      for (int dy = 0; dy < QR_MODULE_PX; dy++) {
        uint8_t* row = outBuf + (py0 + dy) * QR_ROW_BYTES;
        for (int dx = 0; dx < QR_MODULE_PX; dx++) {
          const int col = px0 + dx;
          row[col >> 3] |= (0x80 >> (col & 7));            // MSB-first, set = dark
        }
      }
    }
  }
  return true;
}
```

- [ ] **Step 6: Run the tests to verify they pass**

Run: `pio test -e native`
Expected: PASS — 6 tests total, 0 failures.

- [ ] **Step 7: Commit** *(do not run in this session)*

```bash
git add src/qr.h src/qr.cpp test/test_native/test_main.cpp platformio.ini
git commit -m "feat: QR encode-to-bitmap module with log-and-skip capacity guard"
```

---

### Task 3: `displayRenderQr` sink (blank white + centered QR, full refresh)

**Files:**
- Modify: `src/display.h`
- Modify: `src/display.cpp`

**Interfaces:**
- Consumes: `qr.h` (`qrEncodeToBitmap`, `QR_PX`, `QR_BITMAP_BYTES`).
- Produces: `void displayRenderQr(const char* url)` — renders `url` as a centered QR on a blank white screen via a full-window paged refresh (~1.2–2.3 s), fully clearing any prior code. Logs and leaves the screen unchanged if `qrEncodeToBitmap` returns false.

> No HUMAN-REVIEW flag: `displayBegin()` (init) is unchanged; this uses only the standard GxEPD2 paged full-window path — no waveform/LUT edits.

- [ ] **Step 1: Update the header**

Replace the three artwork show-functions in `src/display.h` with `displayRenderQr`. New `src/display.h`:

```cpp
#pragma once
#include <Arduino.h>

// 7.5" mono e-ink (GDEY075T7 / UC8179) on the reTerminal E1001, write-only HSPI.

// Bring up the panel. Call once in setup().
void displayBegin();

// Render `url` as a QR centered on a blank white 800x480 screen (full-window
// paged refresh, ~1.2-2.3s). Fully clears any prior code. If `url` exceeds the
// QR capacity ceiling (see qr.h), logs and leaves the screen unchanged.
void displayRenderQr(const char* url);
```

- [ ] **Step 2: Rewrite `display.cpp`**

Replace `src/display.cpp` (drop the `images.h` include and the `showImageFull`/`showImagePartial`/`displayIdle`/`displayThoughts`/`displayThoughtsFull` functions; keep pins + `displayBegin` exactly as-is). New `src/display.cpp`:

```cpp
/*
 * Analog display — 7.5" mono e-ink (GDEY075T7 / UC8179) on the reTerminal E1001.
 *
 * Renders a single centered QR code on a blank white screen. The QR encodes the
 * sms: deep link (see smsurl.cpp); scanning it opens Messages prefilled. No
 * artwork — the whole screen is the QR's quiet zone.
 */

#include "display.h"
#include "qr.h"
#include <SPI.h>
#include <GxEPD2_BW.h>

// ePaper display pins (reTerminal E series — same on E1001 and E1002)
#define EPD_SCK_PIN  7
#define EPD_MOSI_PIN 9
#define EPD_CS_PIN   10
#define EPD_DC_PIN   11
#define EPD_RES_PIN  12
#define EPD_BUSY_PIN 13

#define MAX_DISPLAY_BUFFER_SIZE 16000
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) \
                         ? EPD::HEIGHT : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))

#define PANEL_W 800
#define PANEL_H 480
// Center the QR_PX x QR_PX symbol on the panel. drawBitmap takes arbitrary x/y
// (no byte-alignment constraint), so exact centering is fine.
#define QR_X ((PANEL_W - QR_PX) / 2)   // 253
#define QR_Y ((PANEL_H - QR_PX) / 2)   // 93

static SPIClass hspi(HSPI);
static GxEPD2_BW<GxEPD2_750_GDEY075T7, MAX_HEIGHT(GxEPD2_750_GDEY075T7)>
    display(GxEPD2_750_GDEY075T7(EPD_CS_PIN, EPD_DC_PIN, EPD_RES_PIN, EPD_BUSY_PIN));

static uint8_t qrbuf[QR_BITMAP_BYTES];

void displayBegin() {
  hspi.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, -1);   // write-only, no MISO
  display.epd2.selectSPI(hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  display.init(115200);
  display.setRotation(0);
}

void displayRenderQr(const char* url) {
  if (!qrEncodeToBitmap(url, qrbuf, sizeof(qrbuf))) {
    Serial.printf("[qr] '%s' (%u B) exceeds capacity %d — screen unchanged\n",
                  url, (unsigned)strlen(url), QR_BYTE_CAPACITY);
    return;
  }
  uint32_t t0 = millis();
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    // 1 = dark module -> black; 0 -> white. drawBitmap handles sub-byte x.
    display.drawBitmap(QR_X, QR_Y, qrbuf, QR_PX, QR_PX, GxEPD_BLACK, GxEPD_WHITE);
  } while (display.nextPage());
  Serial.printf("[qr] render(full): %lu ms\n", millis() - t0);
}
```

- [ ] **Step 3: Build the device firmware to verify it compiles**

Run: `pio run -e seeed_xiao_esp32s3`
Expected: build FAILS at link — `main.cpp` still references removed symbols (`displayIdle`, `nfc.h`, etc.). That is expected; `main.cpp` is rewired in Task 4. (To confirm `display.cpp` itself compiles in isolation, the clean signal comes after Task 4.)

> If you want an isolated compile check of `display.cpp` before Task 4, that is optional — the authoritative build gate is Task 4 Step 4.

- [ ] **Step 4: Commit** *(do not run in this session)*

```bash
git add src/display.h src/display.cpp
git commit -m "feat: displayRenderQr — centered QR on blank white, full refresh"
```

---

### Task 4: Rewire `main.cpp`, remove NFC + artwork, register the QR library

**Files:**
- Modify: `src/main.cpp`
- Modify: `platformio.ini` (add `ricmoo/QRCode` to the device env — **needs human approval to add the dependency**)
- Delete: `src/nfc.cpp`, `src/nfc.h`, `src/images.h`

**Interfaces:**
- Consumes: `buildSmsUrl` (smsurl.h), `displayRenderQr` + `displayBegin` (display.h), `VENUE_NUMBER` / `GREETING_BODY` (config.h), `pollFeed` / `netLoadCreds` / `netBegin` / `netWhoami` / `netProvision` / `netClearCursor` (net.h).

- [ ] **Step 1: Add the QR library to the device env** *(human approval gate — ricmoo/QRCode)*

In `platformio.ini`, under `[env:seeed_xiao_esp32s3]` `lib_deps`, add `ricmoo/QRCode`:

```ini
lib_deps =
    zinggjm/GxEPD2
    adafruit/Adafruit GFX Library
    bblanchon/ArduinoJson   ; HTTPClient is in the ESP32 core
    ricmoo/QRCode           ; string -> QR module matrix (MIT, ~single-file C)
```

- [ ] **Step 2: Delete the NFC and artwork files**

```bash
git rm src/nfc.cpp src/nfc.h src/images.h
```

- [ ] **Step 3: Rewrite `main.cpp`**

Replace `src/main.cpp`. The state machine and serial/poll coalescing loop are preserved verbatim; only the sink changes (`writeTag`+`nfc`+artwork → `buildSmsUrl`+`displayRenderQr`), and the RESTING→THOUGHTS partial / re-fire full branching collapses into a single QR render. New `src/main.cpp`:

```cpp
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
```

- [ ] **Step 4: Build the device firmware**

Run: `pio run -e seeed_xiao_esp32s3`
Expected: PASS — clean build, no references to `nfc.h` / `images.h` / `displayIdle`. (If the linker complains about `ricmoo/QRCode`, confirm Step 1 was applied.)

- [ ] **Step 5: Re-run native tests (regression)**

Run: `pio test -e native`
Expected: PASS — 6 tests, 0 failures.

- [ ] **Step 6: Commit** *(do not run in this session)*

```bash
git add src/main.cpp platformio.ini
git rm src/nfc.cpp src/nfc.h src/images.h
git commit -m "feat: render on-screen QR deep link, remove dead NFC path + artwork"
```

---

### Task 5: Manual UAT (human, on real hardware) — flash gate

> **Do not flash or commit without explicit human go-ahead.** This task is the manual acceptance pass; it is not automatable.

- [ ] **Step 1: Flash the device** *(human)*

Run: `pio run -e seeed_xiao_esp32s3 -t upload`

- [ ] **Step 2: RESTING QR** — on boot, confirm a centered QR on a blank white screen. Scan on a **real iPhone** and a **real Android**, **off the actual e-ink panel under shop lighting** (matte surface + ambient glare, not a bench monitor). Confirm Messages opens prefilled with the greeting body and **no** `tt_` token line.

- [ ] **Step 3: THOUGHTS QR (token)** — over serial, send `txn`. Confirm the screen updates to a new QR. Scan on **iPhone** and **Android** off the panel under shop lighting. Confirm Messages opens prefilled with the greeting **and** the `tt_<token>` on line 2, token intact.

- [ ] **Step 4: Token round-trip** — send the prefilled message from a test phone; confirm `reconcile-tap` matches it to the transaction (`match_method='tap_token'`) exactly as the NFC tap did. (Inbound path unchanged — this verifies the QR string is byte-identical end-to-end.)

- [ ] **Step 5: Auto-revert** — after `THOUGHTS_TIMEOUT_MS`, confirm the screen returns to the RESTING (no-token) QR with no ghosting of the prior code.

- [ ] **Step 6: Capacity guard (optional)** — temporarily set a `GREETING_BODY` long enough to exceed `QR_BYTE_CAPACITY`; confirm the serial log shows the `exceeds capacity` line and the screen stays unchanged (no garbage QR). Revert the greeting.

---

## Self-Review

**Spec coverage:**
- Reuse `url` buffer, both variants → Task 1 (`buildSmsUrl`, 3 tests). ✓
- Swap sink to QR render → Tasks 2–4. ✓
- Function shape: pure `buildSmsUrl` + `displayRenderQr` sink → Tasks 1, 3 (locked decision (a)). ✓
- C++ QR encoder + framebuffer blit, ricmoo/QRCode under `lib_deps` for approval → Task 2 + Task 4 Step 1 (locked (c), human gate). ✓
- QR version constant + capacity ceiling comment + log-and-skip guard → `qr.h` `QR_VERSION`/`QR_BYTE_CAPACITY`, `qrEncodeToBitmap` guard (Task 2), caller log (Task 3). ✓
- Centered, blank white, no artwork; full refresh to clear prior code → Task 3 (`QR_X=253`, `QR_Y=93`, full-window paged refresh). ✓
- NFC removal (files + calls), tools untouched → Task 4 Steps 2–3, deletions. ✓
- Inbound matching untouched (recorded) → Global Constraints + Task 5 Step 4. ✓
- UAT: iPhone + Android off-panel under shop lighting, token intact → Task 5. ✓
- No flashing/commit this session → commits flagged "do not run", Task 5 flash gate. ✓

**Geometry note (surfaced, not silent):** `QR_MODULE_PX=6` → 294×294 symbol → 1-bpp buffer 10,878 B, under the 16,000 B `MAX_DISPLAY_BUFFER_SIZE`. A larger module (e.g. 8 px → 392×392 → 19,208 B) would overflow that buffer, so 6 px is the deliberate choice; 294 px ≈ 49 mm at the panel's ~124 DPI, well above the practical scannable floor. Rendering via `drawBitmap` on a full-window paged refresh avoids any partial-window byte-alignment constraint, so exact centering (253/93) is used as specified.

**Type consistency:** `buildSmsUrl(venue, greetingBody, token, out, cap)` — same signature in `smsurl.h`, tests, and `main.cpp`. `qrEncodeToBitmap(url, outBuf, outCap)` — same in `qr.h`, tests, `display.cpp`. `displayRenderQr(url)` — same in `display.h`, `display.cpp`, `main.cpp`. QR constants referenced consistently from `qr.h`.

**Placeholder scan:** none — every code step contains complete, real content.
