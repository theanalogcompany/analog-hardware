#pragma once
#include <Arduino.h>

// 7.3" 7-colour e-ink (GDEP073E01) on the reTerminal E1002, write-only HSPI.

// Bring up the panel. Call once in setup().
void displayBegin();

// Idle state — "Analog". Quick to call but the panel refresh blocks ~15s.
void displayIdle();

// Post-transaction state — the "we have thoughts" prompt. Blocks ~15s on the
// colour refresh, so call it AFTER the NFC tag rewrite.
void displayThoughts();
