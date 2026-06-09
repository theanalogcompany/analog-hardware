/*
 * Analog — reTerminal E1002 image swap (images baked into flash, no SD)
 * --------------------------------------------------------------------
 * Boots, connects to WiFi, shows image A.
 * Hitting http://<device-ip>/a or /b swaps the image on the e-paper.
 * The "signal" is just an HTTP request. Later your backend hits these endpoints.
 *
 * Board:  XIAO_ESP32S3  |  PSRAM: OPI PSRAM  |  Upload speed: 115200
 * USB CDC On Boot: Enabled  (so the Serial Monitor shows the IP)
 * Libraries: GxEPD2 + Adafruit GFX
 *
 * Put images.h (from make_images.py) in this same folder before compiling.
 * Do NOT have an SD card in the slot for this sketch — if the screen won't
 * refresh, pull the card out and re-upload.
 */

#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <GxEPD2_7C.h>
#include "images.h"          // imageA[], imageB[], IMG_W, IMG_H

// ====== EDIT THESE TWO LINES ======
const char* WIFI_SSID = "Pinkdeer";
const char* WIFI_PASS = "Violet4877";
// ==================================

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

SPIClass hspi(HSPI);
GxEPD2_7C<GxEPD2_730c_GDEP073E01, MAX_HEIGHT(GxEPD2_730c_GDEP073E01)>
    display(GxEPD2_730c_GDEP073E01(EPD_CS_PIN, EPD_DC_PIN, EPD_RES_PIN, EPD_BUSY_PIN));

WebServer server(80);

// palette index (from images.h) -> GxEPD2 color. Order must match make_images.py.
const uint16_t epaper_colors[] = {
  GxEPD_BLACK, GxEPD_WHITE, GxEPD_GREEN, GxEPD_BLUE, GxEPD_RED, GxEPD_YELLOW
};

volatile char pendingImage = 0;   // 'a' / 'b' / 0 = nothing pending

void drawImage(const uint8_t* img) {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    for (int32_t y = 0; y < IMG_H; y++) {
      for (int32_t x = 0; x < IMG_W; x++) {
        display.drawPixel(x, y, epaper_colors[img[y * IMG_W + x]]);
      }
    }
  } while (display.nextPage());
  Serial.println("Draw done.");
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\nAnalog display swap (flash images) — booting");

  // display only — e-paper is write-only, so no MISO needed
  hspi.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, -1);
  display.epd2.selectSPI(hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  display.init(115200);
  display.setRotation(0);

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.print("\n>>> Device IP: http://");
  Serial.println(WiFi.localIP());

  // routes — these are your "signals"
  server.on("/", []() {
    server.send(200, "text/html",
      "<h2>Analog display</h2>"
      "<p><a href='/a'><button>Show A</button></a> "
      "<a href='/b'><button>Show B</button></a></p>");
  });
  server.on("/a", []() { pendingImage = 'a'; server.send(200, "text/plain", "OK - drawing A (~15-30s)"); });
  server.on("/b", []() { pendingImage = 'b'; server.send(200, "text/plain", "OK - drawing B (~15-30s)"); });
  server.begin();
  Serial.println("Web server up. Open the IP above, or hit /a and /b.");

  pendingImage = 'a';   // show A on first boot
}

void loop() {
  server.handleClient();
  if (pendingImage) {                       // draw outside the handler so HTTP returns fast
    char img = pendingImage; pendingImage = 0;
    drawImage(img == 'b' ? imageB : imageA);
  }
}
