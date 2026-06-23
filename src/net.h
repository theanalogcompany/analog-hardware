#pragma once
#include <Arduino.h>

// WiFi + the analog-guest device event feed.
//
//   GET {BACKEND_BASE_URL}/api/devices/{deviceId}/events?since=<ISO cursor>
//   Authorization: Bearer <raw device token>
//   -> { "cursor": <ISO|null>, "events": [ { txId, at, guest, tapToken, hasNotes } ] }
//
// The server filters events to those strictly newer than `since` and echoes the
// newest occurred_at back as `cursor`; the device persists that cursor (NVS) so
// reboots don't replay. main.cpp owns the state machine — this file only fetches
// and hands back the newest new event's tapToken.

// Connect to WiFi with a bounded timeout. Call once in setup().
void netBegin();

// Load device id / bearer token / cursor / primed-flag from NVS into RAM,
// falling back to the DEV_* config defaults when NVS is empty. Call once in
// setup() (after Serial is up). Logs the resolved id (token redacted).
void netLoadCreds();

// Persist new credentials to NVS and update the in-RAM copy. Resets the cursor
// and primed-flag so the new device re-establishes a fresh baseline. Serial:
// `prov <device_id> <token>`.
void netProvision(const char* deviceId, const char* token);

// Print current device id + backend URL + cursor (token redacted). Serial:
// `whoami`.
void netWhoami();

// Drop the persisted cursor + primed-flag so the next poll re-primes a baseline.
// Serial: `clearcursor`.
void netClearCursor();

// Poll the feed once. Returns true and sets `outTapToken` to the newest new
// transaction's tapToken when there's something to show. Returns false when:
// there's nothing new, this was the priming poll (baseline only, suppressed),
// we're inside a backoff window, WiFi is down, or any HTTP/JSON error occurred.
// Never blocks longer than the HTTP timeout — safe to call from loop().
bool pollFeed(String& outTapToken);
