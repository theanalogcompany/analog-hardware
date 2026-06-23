#pragma once
#include <Arduino.h>

// 7.5" mono e-ink (GDEY075T7 / UC8179) on the reTerminal E1001, write-only HSPI.

// Bring up the panel. Call once in setup().
void displayBegin();

// Render `url` as a QR centered on a blank white 800x480 screen (full-window
// paged refresh, ~1.2-2.3s). Fully clears any prior code. If `url` exceeds the
// QR capacity ceiling (see qr.h), logs and leaves the screen unchanged.
void displayRenderQr(const char* url);
