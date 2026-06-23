#include <unity.h>
#include <cstring>
#include "smsurl.h"
#include "qr.h"

static const char* VENUE   = "+15551234567";
static const char* GREET   = "we%20have%20thoughts%20on%20your%20order";
static const char* TOKEN   = "tt_ABCDEFGHIJKLMNOPQRSTUVWXYZ012345";

void test_build_with_token(void) {
  char url[256];
  buildSmsUrl(VENUE, GREET, TOKEN, url, sizeof(url));
  TEST_ASSERT_EQUAL_STRING(
    "sms:+15551234567&body=we%20have%20thoughts%20on%20your%20order"
    "%0Att_ABCDEFGHIJKLMNOPQRSTUVWXYZ012345",
    url);
}

void test_build_without_token_null(void) {
  char url[256];
  buildSmsUrl(VENUE, GREET, nullptr, url, sizeof(url));
  TEST_ASSERT_EQUAL_STRING(
    "sms:+15551234567&body=we%20have%20thoughts%20on%20your%20order", url);
}

void test_build_without_token_empty(void) {
  char url[256];
  buildSmsUrl(VENUE, GREET, "", url, sizeof(url));
  TEST_ASSERT_EQUAL_STRING(
    "sms:+15551234567&body=we%20have%20thoughts%20on%20your%20order", url);
}

void test_qr_rejects_oversized_url(void) {
  char big[QR_BYTE_CAPACITY + 10];
  memset(big, 'x', sizeof(big) - 1);
  big[sizeof(big) - 1] = '\0';                 // length = QR_BYTE_CAPACITY + 9
  static uint8_t buf[QR_BITMAP_BYTES];
  memset(buf, 0xAB, sizeof(buf));
  TEST_ASSERT_FALSE(qrEncodeToBitmap(big, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_HEX8(0xAB, buf[0]);        // buffer untouched
}

void test_qr_rejects_small_buffer(void) {
  static uint8_t tiny[QR_BITMAP_BYTES - 1];
  TEST_ASSERT_FALSE(qrEncodeToBitmap("sms:+1555&body=hi", tiny, sizeof(tiny)));
}

void test_qr_encodes_typical_url(void) {
  const char* url =
    "sms:+15551234567&body=we%20have%20thoughts%20on%20your%20order"
    "%0Att_ABCDEFGHIJKLMNOPQRSTUVWXYZ012345";
  static uint8_t buf[QR_BITMAP_BYTES];
  memset(buf, 0, sizeof(buf));
  TEST_ASSERT_TRUE(qrEncodeToBitmap(url, buf, sizeof(buf)));
  // Some module must be dark -> at least one non-zero byte.
  bool anyDark = false;
  for (size_t i = 0; i < sizeof(buf); i++) if (buf[i]) { anyDark = true; break; }
  TEST_ASSERT_TRUE(anyDark);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_build_with_token);
  RUN_TEST(test_build_without_token_null);
  RUN_TEST(test_build_without_token_empty);
  RUN_TEST(test_qr_rejects_oversized_url);
  RUN_TEST(test_qr_rejects_small_buffer);
  RUN_TEST(test_qr_encodes_typical_url);
  return UNITY_END();
}

void setUp(void) {}
void tearDown(void) {}
