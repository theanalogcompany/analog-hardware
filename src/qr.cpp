#include "qr.h"
#include "qrcode.h"     // ricmoo/QRCode
#include <cstring>

// Scratch buffer size for QR version 6: ((4*6+17)*(4*6+17)+7)/8 = (41*41+7)/8 = 211 bytes.
// Using a compile-time constant to avoid a VLA (avoids potential -Werror=vla rejection
// under both native clang and ESP32 gcc builds).
#define QR_SCRATCH_BYTES 211

bool qrEncodeToBitmap(const char* url, uint8_t* outBuf, size_t outCap) {
  if (std::strlen(url) > QR_BYTE_CAPACITY) return false;   // never truncate
  if (outCap < QR_BITMAP_BYTES) return false;

  QRCode qrcode;
  uint8_t scratch[QR_SCRATCH_BYTES];
  if (qrcode_initText(&qrcode, scratch, QR_VERSION, ECC_MEDIUM, url) != 0)
    return false;

  std::memset(outBuf, 0, QR_BITMAP_BYTES);     // 0 = white
  const int q = QR_QUIET_MODULES;
  for (uint8_t my = 0; my < qrcode.size; my++) {
    for (uint8_t mx = 0; mx < qrcode.size; mx++) {
      if (!qrcode_getModule(&qrcode, mx, my)) continue;    // paint dark only
      const int px0 = (q + mx) * QR_MODULE_PX;
      const int py0 = (q + my) * QR_MODULE_PX;
      for (int dy = 0; dy < QR_MODULE_PX; dy++) {
        uint8_t* row = outBuf + (py0 + dy) * QR_ROW_BYTES;
        for (int dx = 0; dx < QR_MODULE_PX; dx++) {
          const int col = px0 + dx;
          row[col >> 3] |= (0x80 >> (col & 7));            // MSB-first, set = dark
        }
      }
    }
  }
  return true;
}
