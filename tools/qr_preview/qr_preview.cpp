// Host preview of the on-screen QR — reuses the EXACT firmware code path
// (config.h values -> buildSmsUrl -> qrEncodeToBitmap -> centered on an
// 800x480 white canvas) so what this renders matches what the e-ink panel
// shows. Outputs a 1-bpp PBM (P4) to stdout; a token may be passed as argv[1].
//
// Build (from repo root):
//   g++ -std=c++11 -Isrc -I.pio/libdeps/native/QRCode/src \
//       tools/qr_preview/qr_preview.cpp src/smsurl.cpp src/qr.cpp \
//       .pio/libdeps/native/QRCode/src/qrcode.c -o /tmp/qr_preview
#include <cstdio>
#include <cstring>
#include "config.h"     // VENUE_NUMBER, GREETING_BODY (real device values)
#include "smsurl.h"
#include "qr.h"

// Panel + placement — identical to src/display.cpp.
#define PANEL_W 800
#define PANEL_H 480
#define QR_X ((PANEL_W - QR_PX) / 2)   // 253
#define QR_Y ((PANEL_H - QR_PX) / 2)   // 93

int main(int argc, char** argv) {
  const char* token = (argc > 1) ? argv[1] : "tt_AbCdEfGhIjKlMnOpQrStUvWxYz012345";

  char url[256];
  buildSmsUrl(VENUE_NUMBER, GREETING_BODY, token, url, sizeof(url));
  fprintf(stderr, "deep link (%zu bytes): %s\n", strlen(url), url);

  static uint8_t qrbuf[QR_BITMAP_BYTES];
  if (!qrEncodeToBitmap(url, qrbuf, sizeof(qrbuf))) {
    fprintf(stderr, "ERROR: url exceeds QR capacity (%d) — device would log and skip\n",
            QR_BYTE_CAPACITY);
    return 1;
  }

  // Compose the 800x480 framebuffer and emit as PBM P4 (bit set = black).
  // White canvas; a QR dark module (1 in qrbuf, MSB-first) -> black pixel.
  const int rowBytes = (PANEL_W + 7) / 8;   // 100
  printf("P4\n%d %d\n", PANEL_W, PANEL_H);
  static uint8_t out[PANEL_H * ((PANEL_W + 7) / 8)];
  memset(out, 0, sizeof(out));              // 0 = white in PBM
  for (int y = 0; y < PANEL_H; y++) {
    for (int x = 0; x < PANEL_W; x++) {
      int xx = x - QR_X, yy = y - QR_Y;
      bool dark = false;
      if (xx >= 0 && xx < QR_PX && yy >= 0 && yy < QR_PX) {
        const uint8_t* row = qrbuf + yy * QR_ROW_BYTES;
        dark = (row[xx >> 3] & (0x80 >> (xx & 7))) != 0;
      }
      if (dark) out[y * rowBytes + (x >> 3)] |= (0x80 >> (x & 7));
    }
  }
  fwrite(out, 1, (size_t)PANEL_H * rowBytes, stdout);
  return 0;
}
