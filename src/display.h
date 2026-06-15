#pragma once
#include <Arduino.h>

// 7.5" mono e-ink (GDEY075T7 / UC8179) on the reTerminal E1001, write-only HSPI.

// Bring up the panel. Call once in setup().
void displayBegin();

// Resting state — full refresh (~2.3s). Also clears partial-update ghosting.
void displayIdle();

// Thoughts state — fast PARTIAL refresh (~1.3s). Use for the RESTING->THOUGHTS flip.
void displayThoughts();

// Thoughts state — FULL refresh (~2.3s). Use to re-show thoughts on a re-fire so
// the flash is a visible "it updated for you" cue (a partial of identical content
// would be invisible).
void displayThoughtsFull();
