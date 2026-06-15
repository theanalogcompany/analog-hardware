/*
 * Analog display — 7.5" mono e-ink (GDEY075T7 / UC8179) on the reTerminal E1001.
 *
 * Confirmed via bring-up: class GxEPD2_750_GDEY075T7, full refresh ~1.2-2.3s,
 * hasFastPartialUpdate. Write-only HSPI, no MISO. The two states render full-screen
 * 1-bpp artwork baked into flash by make_images.py (mono path) -> images.h.
 */

#include "display.h"
#include <SPI.h>
#include <GxEPD2_BW.h>
#include "images.h"            // IMG_RESTING[], IMG_THOUGHTS[], IMG_W, IMG_H (packed 1-bpp)

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

static SPIClass hspi(HSPI);
static GxEPD2_BW<GxEPD2_750_GDEY075T7, MAX_HEIGHT(GxEPD2_750_GDEY075T7)>
    display(GxEPD2_750_GDEY075T7(EPD_CS_PIN, EPD_DC_PIN, EPD_RES_PIN, EPD_BUSY_PIN));

void displayBegin() {
  hspi.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, -1);   // write-only, no MISO
  display.epd2.selectSPI(hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  display.init(115200);
  display.setRotation(0);
}

// Full-refresh blit of a packed 1-bpp full-screen bitmap (1=white, 0=black).
// refresh(false) does the full waveform (~1.6s) and auto powers the panel off.
static void showImageFull(const uint8_t* bitmap) {
  uint32_t t0 = millis();
  display.setFullWindow();
  display.writeImage(bitmap, 0, 0, IMG_W, IMG_H, false /*invert*/, false /*mirror_y*/, false /*pgm*/);
  display.refresh(false);
  Serial.printf("[disp] refresh(full): %lu ms\n", millis() - t0);
}

// Fast differential full-screen flip (~0.45s). refresh(true) uses the partial
// waveform and does NOT auto power-off, so we do it. Partial updates accumulate
// slight ghosting over many flips; displayIdle()'s FULL refresh clears it.
static void showImagePartial(const uint8_t* bitmap) {
  uint32_t t0 = millis();
  display.setPartialWindow(0, 0, IMG_W, IMG_H);
  display.writeImage(bitmap, 0, 0, IMG_W, IMG_H, false /*invert*/, false /*mirror_y*/, false /*pgm*/);
  display.refresh(true);
  display.powerOff();
  Serial.printf("[disp] refresh(partial): %lu ms\n", millis() - t0);
}

void displayIdle() {
  showImageFull(IMG_RESTING);       // FULL — also clears partial-update ghosting on return to idle
}

void displayThoughts() {
  showImagePartial(IMG_THOUGHTS);   // PARTIAL — fast RESTING->THOUGHTS flip
}

void displayThoughtsFull() {
  showImageFull(IMG_THOUGHTS);      // FULL — visible re-show flash on a THOUGHTS re-fire
}
