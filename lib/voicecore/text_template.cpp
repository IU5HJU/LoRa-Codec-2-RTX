#include "text_template.h"

#include <cstdio>
#include <cstring>

#include "packet.h"
#include "protocol.h"

namespace lcr {

size_t expandTextTemplate(const char* tmpl, const char* callsign, char modeLetter, uint32_t freqKhz, char* out, size_t cap) {
  if (!tmpl || !out || cap < 2) return 0;
  char freq[12];
  snprintf(freq, sizeof(freq), "%u.%03u", static_cast<unsigned>(freqKhz / 1000), static_cast<unsigned>(freqKhz % 1000));
  size_t n = 0;
  auto put = [&](const char* s) {
    for (; *s; s++) {
      if (n + 1 >= cap) return false;
      out[n++] = *s;
    }
    return true;
  };
  for (size_t i = 0; tmpl[i]; i++) {
    bool ok = true;
    if (tmpl[i] == '%' && tmpl[i + 1]) {
      const char c = tmpl[i + 1];
      if (c == 'C') { ok = put(callsign ? callsign : ""); i++; }
      else if (c == 'M') { const char m[2] = {modeLetter, 0}; ok = put(m); i++; }
      else if (c == 'F') { ok = put(freq); i++; }
      else if (c == '%') { ok = put("%"); i++; }
      else ok = put("%");
    } else {
      const char one[2] = {tmpl[i], 0};
      ok = put(one);
    }
    if (!ok) {
      out[0] = 0;
      return 0;
    }
  }
  out[n] = 0;
  if (n == 0 || n > kMaxTextChars || !isValidText(out, n)) {
    out[0] = 0;
    return 0;
  }
  return n;
}

size_t signText(const char* text, char* out, size_t cap) {
  if (!text || !out || cap < 2) return 0;
  const size_t sigLen = strlen(kVoLoRaSignature);
  size_t len = strlen(text);
  while (len > 0 && text[len - 1] == ' ') len--;
  if (len == 0) return 0;
  // gia' firmato?
  if (len >= sigLen) {
    bool same = true;
    for (size_t i = 0; i < sigLen && same; i++) {
      char a = text[len - sigLen + i], b = kVoLoRaSignature[i];
      if (a >= 'a' && a <= 'z') a = static_cast<char>(a - 32);
      if (b >= 'a' && b <= 'z') b = static_cast<char>(b - 32);
      same = a == b;
    }
    if (same) {
      if (len + 1 > cap || len > kMaxTextChars) return 0;
      memcpy(out, text, len);
      out[len] = 0;
      return len;
    }
  }
  const size_t room = kMaxTextChars - (sigLen + 1);  // spazio per il testo
  if (len > room) len = room;
  while (len > 0 && text[len - 1] == ' ') len--;
  if (len == 0 || len + 1 + sigLen + 1 > cap) return 0;
  memcpy(out, text, len);
  out[len] = ' ';
  memcpy(out + len + 1, kVoLoRaSignature, sigLen);
  out[len + 1 + sigLen] = 0;
  return len + 1 + sigLen;
}

}  // namespace lcr
