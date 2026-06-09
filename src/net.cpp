/*
 * Analog net — Stage 2: WiFi bring-up + polling a mock token endpoint.
 *
 * pollToken() does a plain HTTP GET on TOKEN_URL and pulls "token" out of the
 * JSON body. main.cpp owns the change-detection (only fires onTransaction when
 * the token differs from the last one) — this file just fetches.
 */

#include "net.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define WIFI_CONNECT_TIMEOUT_MS 15000
#define HTTP_TIMEOUT_MS         3000

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

bool pollToken(String& out) {
  if (WiFi.status() != WL_CONNECTED) return false;   // no link, nothing to poll

  HTTPClient http;
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin(TOKEN_URL)) {
    Serial.println("[net] http.begin() failed");
    return false;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[net] GET %s -> HTTP %d\n", TOKEN_URL, code);
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("[net] JSON parse error: %s\n", err.c_str());
    return false;
  }

  const char* tok = doc["token"];
  if (!tok) {
    Serial.println("[net] response had no \"token\" field");
    return false;
  }

  out = tok;
  return true;
}
