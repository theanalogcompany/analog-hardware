/*
 * Analog display — 7.3" 7-colour e-ink (GDEP073E01) on the reTerminal E1002.
 *
 * Init lifted verbatim from the proven image sketch (tools/analog_display_swap.ino):
 * write-only HSPI, no MISO. The two states render full-screen artwork baked into
 * flash by make_images.py -> images.h (IMG_RESTING / IMG_THOUGHTS).
 */

#include "display.h"
#include <SPI.h>
#include <GxEPD2_7C.h>
#include "images.h"            // IMG_RESTING[], IMG_THOUGHTS[], IMG_W, IMG_H

// ePaper display pins (reTerminal E series)
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
static GxEPD2_7C<GxEPD2_730c_GDEP073E01, MAX_HEIGHT(GxEPD2_730c_GDEP073E01)>
    display(GxEPD2_730c_GDEP073E01(EPD_CS_PIN, EPD_DC_PIN, EPD_RES_PIN, EPD_BUSY_PIN));

void displayBegin() {
  hspi.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, -1);   // write-only, no MISO
  display.epd2.selectSPI(hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  display.init(115200);
  display.setRotation(0);
}

// palette index (from images.h) -> GxEPD2 colour. Order MUST mirror the PURE[]
// table in make_images.py: BLACK, WHITE, GREEN, BLUE, RED, YELLOW.
static const uint16_t epaper_colors[] = {
  GxEPD_BLACK, GxEPD_WHITE, GxEPD_GREEN, GxEPD_BLUE, GxEPD_RED, GxEPD_YELLOW
};

// Proven paged draw: blit a full-screen palette-indexed image to the panel.
static void drawImage(const uint8_t* img) {
  // Pilot telemetry: the 7C refresh waveform dominates this wall-clock and is
  // temperature-dependent, so we log it per unit/venue. ~29s near the panel floor.
  uint32_t t0 = millis();
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    for (int32_t y = 0; y < IMG_H; y++)
      for (int32_t x = 0; x < IMG_W; x++)
        display.drawPixel(x, y, epaper_colors[img[y * IMG_W + x]]);
  } while (display.nextPage());
  Serial.printf("[disp] refresh: %lu ms\n", millis() - t0);
}

void displayIdle() {
  drawImage(IMG_RESTING);
}

void displayThoughts() {
  drawImage(IMG_THOUGHTS);
}
