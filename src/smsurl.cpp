#include "smsurl.h"
#include <cstdio>

void buildSmsUrl(const char* venue, const char* greetingBody,
                 const char* token, char* out, size_t cap) {
  if (token && token[0] != '\0')
    snprintf(out, cap, "sms:%s&body=%s%%0A%s", venue, greetingBody, token);
  else
    snprintf(out, cap, "sms:%s&body=%s", venue, greetingBody);
}
