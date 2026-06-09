/*
 * Analog display — 7.3" 7-colour e-ink (GDEP073E01) on the reTerminal E1002.
 *
 * Init lifted verbatim from the proven image sketch (tools/analog_display_swap.ino):
 * write-only HSPI, no MISO. Stage 1 renders the two states as plain centred GFX
 * text. Real artwork (make_images.py -> images.h) slots in later without touching
 * the public displayIdle()/displayThoughts() interface.
 */

#include "display.h"
#include <SPI.h>
#include <GxEPD2_7C.h>

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

// Word-wrap `text` to the panel width and render it centred (both axes),
// black on white, at the given GFX text size. Classic GFX font = ASCII only.
static void drawCenteredText(const char* text, uint8_t textSize) {
  const int16_t W = display.width();
  const int16_t H = display.height();
  const int16_t margin = 24;
  const int16_t maxW = W - 2 * margin;

  display.setTextSize(textSize);
  display.setTextColor(GxEPD_BLACK);
  display.setTextWrap(false);

  // Greedy word-wrap into up to 8 lines.
  char lines[8][64];
  int  lineCount = 0;
  char word[64];
  int  wi = 0;
  int16_t x1, y1; uint16_t w, h;

  // tokenize on spaces; flush words into lines that fit maxW
  const char* p = text;
  char cur[64]; cur[0] = '\0';
  while (true) {
    char c = *p;
    if (c != ' ' && c != '\0') {
      if (wi < 63) word[wi++] = c;
      p++;
      continue;
    }
    word[wi] = '\0';
    if (word[0] != '\0' && lineCount < 8) {
      char trial[64];
      if (cur[0] == '\0') snprintf(trial, sizeof(trial), "%s", word);
      else                snprintf(trial, sizeof(trial), "%s %s", cur, word);
      display.getTextBounds(trial, 0, 0, &x1, &y1, &w, &h);
      if (w <= maxW || cur[0] == '\0') {
        snprintf(cur, sizeof(cur), "%s", trial);
      } else {
        snprintf(lines[lineCount++], 64, "%s", cur);
        snprintf(cur, sizeof(cur), "%s", word);
      }
    }
    wi = 0;
    if (c == '\0') break;
    p++;
  }
  if (cur[0] != '\0' && lineCount < 8) snprintf(lines[lineCount++], 64, "%s", cur);

  // Measure line height from a sample, stack lines, centre the block vertically.
  display.getTextBounds("Ag", 0, 0, &x1, &y1, &w, &h);
  const int16_t lineH = h + 8;
  const int16_t blockH = lineH * lineCount;
  int16_t y = (H - blockH) / 2;

  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    for (int i = 0; i < lineCount; i++) {
      display.getTextBounds(lines[i], 0, 0, &x1, &y1, &w, &h);
      int16_t x = (W - w) / 2 - x1;
      display.setCursor(x, y + i * lineH - y1);
      display.print(lines[i]);
    }
  } while (display.nextPage());
}

void displayIdle() {
  drawCenteredText("Analog", 8);
}

void displayThoughts() {
  drawCenteredText("we have thoughts on your order - tap", 5);
}
