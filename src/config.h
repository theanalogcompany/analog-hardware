#pragma once

// ---- Stage 1 placeholders — edit per device / venue ----

// sms: target (Sendblue/iMessage number). Keep the +1 country code.
#define VENUE_NUMBER  "+16452067656"

// Greeting shown on the e-ink AND sent as line 1 of the iMessage body.
// GREETING       = human-readable (screen)
// GREETING_BODY  = same text, spaces pre-encoded as %20 for the sms: URL
#define GREETING      "we have thoughts on your order"
#define GREETING_BODY "we%20have%20thoughts%20on%20your%20order"

// Identifies this unit.
#define DEVICE_ID     "analog-001"

// ---- Stage 2: WiFi + polled token source ----

// WiFi credentials.
#define WIFI_SSID "Jaipal's iPhone"
#define WIFI_PASS "jaipal123"

// Mock token endpoint. Returns {"token":"..."}. Poll for the current token;
// onTransaction() fires when it changes.
#define TOKEN_URL "http://192.0.0.2:8000/token.json"

// How often to poll TOKEN_URL.
#define POLL_INTERVAL_MS 4000
