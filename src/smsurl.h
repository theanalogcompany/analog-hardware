#pragma once
#include <cstddef>

// Assemble the sms: deep link, byte-identical to the legacy NFC writeTag()
// payload. token == nullptr/"" -> RESTING variant (greeting only, no token line).
//   THOUGHTS: sms:<venue>&body=<greeting>%0A<token>
//   RESTING : sms:<venue>&body=<greeting>
// venue/greetingBody come from config.h (caller passes the macros). Pure: no I/O.
void buildSmsUrl(const char* venue, const char* greetingBody,
                 const char* token, char* out, size_t cap);
