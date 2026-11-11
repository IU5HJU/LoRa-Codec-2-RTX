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

}  // namespace lcr
