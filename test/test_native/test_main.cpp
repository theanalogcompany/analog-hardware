#include <unity.h>
#include <cstring>
#include "smsurl.h"

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

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_build_with_token);
  RUN_TEST(test_build_without_token_null);
  RUN_TEST(test_build_without_token_empty);
  return UNITY_END();
}

void setUp(void) {}
void tearDown(void) {}
