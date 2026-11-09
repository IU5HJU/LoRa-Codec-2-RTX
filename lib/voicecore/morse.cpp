#include "morse.h"

#include <cctype>
#include <cstring>

namespace lcr {

namespace {
// '.' punto, '-' linea
const char* const kLetters[26] = {".-",   "-...", "-.-.", "-..",  ".",   "..-.", "--.",  "....", "..",
                                  ".---", "-.-",  ".-..", "--",   "-.",  "---",  ".--.", "--.-", ".-.",
                                  "...",  "-",    "..-",  "...-", ".--", "-..-", "-.--", "--.."};
const char* const kDigits[10] = {"-----", ".----", "..---", "...--", "....-", ".....", "-....", "--...", "---..", "----."};

const char* codeFor(char c) {
  if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
  if (c >= 'A' && c <= 'Z') return kLetters[c - 'A'];
  if (c >= '0' && c <= '9') return kDigits[c - '0'];
  return nullptr;
}

struct Writer {
  MorseElement* out;
  size_t cap;
  size_t n = 0;
  bool put(bool tone, uint16_t ms) {
    if (n >= cap) return false;
    out[n++] = {tone, ms};
    return true;
  }
};
}  // namespace

size_t morseElements(const char* text, uint16_t unitMs, MorseElement* out, size_t cap) {
  if (!text || !out) return 0;
  Writer w{out, cap};
  uint16_t gapUnits = 0;  // 7 dopo uno spazio tra parole, altrimenti 3 tra le lettere
  bool first = true;
  for (const char* p = text; *p; p++) {
    if (*p == ' ') {
      if (!first) gapUnits = 7;
      continue;
    }
    const char* code = codeFor(*p);
    if (!code) continue;
    if (!first) {  // tra le lettere 3 unita', tra le parole 7
      if (!w.put(false, static_cast<uint16_t>((gapUnits ? gapUnits : 3) * unitMs))) return w.n;
    }
    gapUnits = 0;
    for (const char* e = code; *e; e++) {
      if (e != code && !w.put(false, unitMs)) return w.n;
      if (!w.put(true, static_cast<uint16_t>((*e == '-' ? 3 : 1) * unitMs))) return w.n;
    }
    first = false;
  }
  return w.n;
}

uint32_t morseDurationMs(const MorseElement* e, size_t n) {
  uint32_t t = 0;
  for (size_t i = 0; i < n; i++) t += e[i].ms;
  return t;
}

}  // namespace lcr
