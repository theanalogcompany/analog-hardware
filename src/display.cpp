/*
 * Analog display — 7.5" mono e-ink (GDEY075T7 / UC8179) on the reTerminal E1001.
 *
 * Renders a single centered QR code on a blank white screen. The QR encodes the
 * sms: deep link (see smsurl.cpp); scanning it opens Messages prefilled. No
 * artwork — the whole screen is the QR's quiet zone.
 */

#include "display.h"
#include "qr.h"
#include "images.h"          // IMG_RESTING[], IMG_W, IMG_H (packed 1-bpp, 1=white 0=black)
#include <SPI.h>
#include <GxEPD2_BW.h>

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

#define PANEL_W 800
#define PANEL_H 480
// Center the QR_PX x QR_PX symbol on the panel. drawBitmap takes arbitrary x/y
// (no byte-alignment constraint), so exact centering is fine.
#define QR_X ((PANEL_W - QR_PX) / 2)   // 253
#define QR_Y ((PANEL_H - QR_PX) / 2)   // 93

static SPIClass hspi(HSPI);
static GxEPD2_BW<GxEPD2_750_GDEY075T7, MAX_HEIGHT(GxEPD2_750_GDEY075T7)>
    display(GxEPD2_750_GDEY075T7(EPD_CS_PIN, EPD_DC_PIN, EPD_RES_PIN, EPD_BUSY_PIN));

static uint8_t qrbuf[QR_BITMAP_BYTES];

void displayBegin() {
  hspi.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, -1);   // write-only, no MISO
  display.epd2.selectSPI(hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  display.init(115200);
  display.setRotation(0);
}

// Full-refresh blit of the resting café artwork. writeImage treats 1=white,
// 0=black (GxEPD2 convention, the byte order make_images.py emits). refresh(false)
// runs the full waveform and auto powers the panel off.
void displayResting() {
  uint32_t t0 = millis();
  display.setFullWindow();
  display.writeImage(IMG_RESTING, 0, 0, IMG_W, IMG_H, false /*invert*/, false /*mirror_y*/, false /*pgm*/);
  display.refresh(false);
  Serial.printf("[disp] resting(full): %lu ms\n", millis() - t0);
}

void displayRenderQr(const char* url) {
  if (!qrEncodeToBitmap(url, qrbuf, sizeof(qrbuf))) {
    Serial.printf("[qr] '%s' (%u B) exceeds capacity %d — screen unchanged\n",
                  url, (unsigned)strlen(url), QR_BYTE_CAPACITY);
    return;
  }
  uint32_t t0 = millis();
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    // 1 = dark module -> black; 0 -> white. drawBitmap handles sub-byte x.
    display.drawBitmap(QR_X, QR_Y, qrbuf, QR_PX, QR_PX, GxEPD_BLACK, GxEPD_WHITE);
  } while (display.nextPage());
  Serial.printf("[qr] render(full): %lu ms\n", millis() - t0);
}
