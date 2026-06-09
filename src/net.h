#pragma once
#include <Arduino.h>

// Stage 2: WiFi + polled token source.

// Connect to WiFi with a bounded timeout so a bad network can't hang boot.
// Logs status and the assigned IP. Call once in setup().
void netBegin();

// GET TOKEN_URL, parse {"token":"..."} and write it to `out`. Returns true on
// success; on any HTTP or parse failure, logs and returns false.
bool pollToken(String& out);
