#pragma once

// ---- Stage 1 placeholders — edit per device / venue ----

// sms: target (Sendblue/iMessage number). Keep the +1 country code.
#define VENUE_NUMBER  "+16452067656"

// Greeting shown on the e-ink AND sent as line 1 of the iMessage body.
// GREETING       = human-readable (screen)
// GREETING_BODY  = same text, spaces pre-encoded as %20 for the sms: URL
#define GREETING      "we have thoughts on your order"
#define GREETING_BODY "we%20have%20thoughts%20on%20your%20order"

// Identifies this unit (unused in stage 1, here for the upcoming network stage).
#define DEVICE_ID     "analog-001"

// ---- Stage 2 TODO: WiFi creds for polling / Square ----
// #define WIFI_SSID "..."
// #define WIFI_PASS "..."
