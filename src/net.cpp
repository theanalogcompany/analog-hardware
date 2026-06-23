/*
 * Analog net — WiFi bring-up + the analog-guest device event feed.
 *
 * Credentials and the poll cursor live in NVS (Preferences, namespace "analog")
 * so the raw bearer token is never compiled into the image and reboots don't
 * replay old transactions. See net.h for the endpoint contract.
 *
 * Priming: on a freshly provisioned device the first poll (empty `since`) would
 * return the venue's whole transaction backlog. We record that as a baseline and
 * suppress dispatch (the `primed` flag), so a power-up never flashes a stale
 * order — only transactions seen AFTER priming drive the screen.
 */

#include "net.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#define WIFI_CONNECT_TIMEOUT_MS 15000
#define HTTP_TIMEOUT_MS         5000
#define NVS_NS                  "analog"

#define BACKOFF_401_MS   60000UL    // wrong/missing token: long, quiet backoff
#define BACKOFF_MIN_MS   10000UL    // network/parse error: exponential 10s..160s
#define BACKOFF_MAX_MS   160000UL

static Preferences prefs;

// In-RAM mirror of NVS state.
static String gDeviceId;
static String gToken;
static String gCursor;          // ISO8601, echoed back as ?since=
static bool   gPrimed = false;  // false until a baseline poll has run
static String gLastTxId;        // belt-and-suspenders dedupe across polls

// Backoff state.
static uint32_t gBackoffUntilMs = 0;
static uint32_t gBackoffMs      = 0;
static bool     g401Logged      = false;

void netBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.printf("[net] connecting to \"%s\"", WIFI_SSID);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[net] connected. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("[net] WiFi connect TIMED OUT — continuing without network (serial txn still works).");
  }
}

void netLoadCreds() {
  prefs.begin(NVS_NS, false);   // read/write, kept open for the session
  gDeviceId = prefs.getString("deviceId", DEV_DEVICE_ID);
  gToken    = prefs.getString("deviceTok", DEV_DEVICE_TOKEN);
  gCursor   = prefs.getString("cursor", "");
  gPrimed   = prefs.getBool("primed", false);
  Serial.printf("[net] device=%s backend=%s cursor=\"%s\" primed=%d token=%s\n",
                gDeviceId.c_str(), BACKEND_BASE_URL, gCursor.c_str(), gPrimed,
                gToken.length() ? "(set)" : "(UNSET)");
}

void netProvision(const char* deviceId, const char* token) {
  gDeviceId = deviceId;
  gToken    = token;
  gCursor   = "";
  gPrimed   = false;
  gLastTxId = "";
  prefs.putString("deviceId", gDeviceId);
  prefs.putString("deviceTok", gToken);
  prefs.putString("cursor", gCursor);
  prefs.putBool("primed", gPrimed);
  g401Logged = false;
  gBackoffUntilMs = 0;
  gBackoffMs = 0;
  Serial.printf("[net] provisioned device=%s (cursor reset — will re-prime next poll)\n",
                gDeviceId.c_str());
}

void netWhoami() {
  Serial.printf("[net] device=%s backend=%s cursor=\"%s\" primed=%d token=%s\n",
                gDeviceId.c_str(), BACKEND_BASE_URL, gCursor.c_str(), gPrimed,
                gToken.length() ? "(set)" : "(UNSET)");
}

void netClearCursor() {
  gCursor   = "";
  gPrimed   = false;
  gLastTxId = "";
  prefs.putString("cursor", gCursor);
  prefs.putBool("primed", gPrimed);
  Serial.println("[net] cursor cleared — will re-prime next poll.");
}

// Percent-encode everything outside the URL unreserved set. The cursor is an ISO
// timestamp whose ':' and (for +00:00 offsets) '+' MUST be encoded or the query
// string corrupts ('+' would decode to a space).
static String urlEncode(const String& s) {
  static const char* hex = "0123456789ABCDEF";
  String out;
  out.reserve(s.length() * 3);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += c;
    } else {
      out += '%';
      out += hex[(c >> 4) & 0xF];
      out += hex[c & 0xF];
    }
  }
  return out;
}

// Double the current backoff, capped — or start at the floor if none yet.
static uint32_t nextBackoffMs() {
  if (gBackoffMs == 0) return BACKOFF_MIN_MS;
  uint32_t doubled = gBackoffMs * 2;
  return doubled > BACKOFF_MAX_MS ? (uint32_t)BACKOFF_MAX_MS : doubled;
}

static void enterBackoff(uint32_t ms) {
  gBackoffMs = ms;
  gBackoffUntilMs = millis() + ms;
  if (gBackoffUntilMs == 0) gBackoffUntilMs = 1;   // 0 means "no backoff"
}

static void clearBackoff() {
  gBackoffUntilMs = 0;
  gBackoffMs = 0;
  g401Logged = false;
}

bool pollFeed(String& outTapToken) {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (gDeviceId.isEmpty() || gToken.isEmpty()) return false;
  if (gBackoffUntilMs && (int32_t)(millis() - gBackoffUntilMs) < 0) return false;

  String url = String(BACKEND_BASE_URL) + "/api/devices/" + gDeviceId + "/events";
  if (gCursor.length()) url += "?since=" + urlEncode(gCursor);

  bool isHttps = url.startsWith("https:");
  WiFiClientSecure secure;
  WiFiClient plain;
  if (isHttps) secure.setInsecure();   // MVP: no cert validation (flagged)

  HTTPClient http;
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  bool ok = isHttps ? http.begin(secure, url) : http.begin(plain, url);
  if (!ok) {
    Serial.println("[net] http.begin() failed");
    enterBackoff(BACKOFF_MIN_MS);
    return false;
  }
  http.addHeader("Authorization", "Bearer " + gToken);

  int code = http.GET();

  if (code == HTTP_CODE_UNAUTHORIZED) {
    http.end();
    if (!g401Logged) {
      Serial.println("[net] 401 unauthorized — check device id/token (or device not provisioned in backend). Backing off 60s.");
      g401Logged = true;
    }
    enterBackoff(BACKOFF_401_MS);
    return false;
  }
  if (code != HTTP_CODE_OK) {
    Serial.printf("[net] GET feed -> HTTP %d\n", code);
    http.end();
    enterBackoff(nextBackoffMs());
    return false;
  }

  String body = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("[net] JSON parse error: %s\n", err.c_str());
    enterBackoff(nextBackoffMs());
    return false;
  }

  clearBackoff();   // a clean 200 resets any prior backoff

  JsonArray events = doc["events"].as<JsonArray>();
  size_t n = events.size();

  for (JsonObject e : events) {
    Serial.printf("[feed] tx=%s guest=%s notes=%d token=%s\n",
                  e["txId"] | "?", e["guest"] | "?", (int)(e["hasNotes"] | false),
                  e["tapToken"] | "?");
  }

  // Advance + persist the cursor. Prefer the server's cursor; fall back to the
  // newest event's `at`. A null cursor with no events leaves it unchanged.
  const char* cursor = doc["cursor"];
  if (cursor && cursor[0]) {
    gCursor = cursor;
    prefs.putString("cursor", gCursor);
  } else if (n > 0) {
    gCursor = events[n - 1]["at"] | "";
    prefs.putString("cursor", gCursor);
  }

  // First poll after provisioning: record the baseline, suppress dispatch.
  if (!gPrimed) {
    gPrimed = true;
    prefs.putBool("primed", true);
    Serial.printf("[feed] primed baseline (cursor=\"%s\"); %u backlog event(s) suppressed.\n",
                  gCursor.c_str(), (unsigned)n);
    return false;
  }

  if (n == 0) return false;

  // Newest event drives the screen. main.cpp coalesces anyway, and the cursor
  // makes re-fetches strictly-newer, so showing the latest is correct.
  JsonObject newest = events[n - 1];
  const char* tok  = newest["tapToken"];
  const char* txId = newest["txId"] | "";
  if (!tok || !tok[0]) return false;
  if (gLastTxId == txId) return false;   // already dispatched (cursor safety net)

  gLastTxId = txId;
  outTapToken = tok;
  return true;
}
