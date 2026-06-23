#pragma once
#include <cstdint>
#include <cstddef>

// --- QR symbol parameters --------------------------------------------------
// Version 6 = 41x41 modules, ECC level M. Byte-mode capacity at V6-M = 122
// bytes. The sms: deep link is ~100 bytes today. If GREETING_BODY grows past
// QR_BYTE_CAPACITY, bump QR_VERSION (a visible one-line change) — we never
// silently truncate (see qrEncodeToBitmap's guard).
#define QR_VERSION        6
#define QR_BYTE_CAPACITY  122                 // V6, ECC_MEDIUM, byte mode
#define QR_MODULES        41                  // 17 + 4*QR_VERSION
#define QR_QUIET_MODULES  4
#define QR_MODULE_PX      6
#define QR_PX             ((QR_MODULES + 2 * QR_QUIET_MODULES) * QR_MODULE_PX) // 294
#define QR_ROW_BYTES      ((QR_PX + 7) / 8)                                    // 37
#define QR_BITMAP_BYTES   (QR_PX * QR_ROW_BYTES)                               // 10878
// ricmoo/QRCode working-buffer size for QR_VERSION (qrcode_getBufferSize formula).
// Derived from QR_VERSION so a version bump stays a single one-line change.
#define QR_SCRATCH_BYTES (((4 * QR_VERSION + 17) * (4 * QR_VERSION + 17) + 7) / 8)  // V6 -> 211

// Encode `url` into a packed 1-bpp bitmap (MSB-first, 1 = dark module),
// QR_PX x QR_PX, with a QR_QUIET_MODULES quiet zone, each module scaled to
// QR_MODULE_PX. Returns false (buffer untouched) if the url exceeds
// QR_BYTE_CAPACITY or outCap < QR_BITMAP_BYTES. Caller logs + skips the refresh.
bool qrEncodeToBitmap(const char* url, uint8_t* outBuf, size_t outCap);
