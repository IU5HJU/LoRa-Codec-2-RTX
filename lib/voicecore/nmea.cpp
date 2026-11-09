#include "nmea.h"

#include <cstdlib>
#include <cstring>

namespace lcr {

namespace {

int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

// Spezza body in campi separati da virgole (sostituisce le virgole con 0). Ritorna il numero di campi.
size_t splitFields(char* body, char** fields, size_t cap) {
  size_t n = 0;
  char* p = body;
  while (n < cap) {
    fields[n++] = p;
    char* comma = strchr(p, ',');
    if (!comma) break;
    *comma = 0;
    p = comma + 1;
  }
  return n;
}

// "ddmm.mmmm" (latitudine, 2 cifre di gradi) o "dddmm.mmmm" (longitudine, 3 cifre) -> decimilionesimi di grado.
bool parseCoord(const char* s, size_t degDigits, char hemi, int32_t& e7) {
  const size_t n = strlen(s);
  if (n < degDigits + 3) return false;  // almeno gradi, due cifre di minuti e il punto
  int64_t deg = 0;
  for (size_t i = 0; i < degDigits; i++) {
    if (s[i] < '0' || s[i] > '9') return false;
    deg = deg * 10 + (s[i] - '0');
  }
  const char* m = s + degDigits;
  if (m[0] < '0' || m[0] > '9' || m[1] < '0' || m[1] > '9') return false;
  int64_t minE6 = ((m[0] - '0') * 10 + (m[1] - '0')) * 1000000LL;
  if (m[2] == '.') {
    int64_t scale = 100000;
    for (const char* f = m + 3; *f; f++) {
      if (*f < '0' || *f > '9') return false;
      minE6 += (*f - '0') * scale;
      scale /= 10;
      if (scale == 0) break;
    }
  } else if (m[2] != 0) {
    return false;
  }
  if (minE6 >= 60 * 1000000LL) return false;
  int64_t v = deg * 10000000LL + (minE6 + 3) / 6;  // minuti (1e-6) / 6 = decimilionesimi di grado, arrotondato
  if (hemi == 'S' || hemi == 'W') v = -v;
  else if (hemi != 'N' && hemi != 'E') return false;
  e7 = static_cast<int32_t>(v);
  return true;
}

int parseUint(const char* s) {
  if (!*s) return -1;
  int v = 0;
  for (; *s; s++) {
    if (*s < '0' || *s > '9') return -1;
    v = v * 10 + (*s - '0');
    if (v > 1000) return -1;
  }
  return v;
}

}  // namespace

bool NmeaParser::feed(char c) {
  if (c == '$') {  // inizio frase (anche se la precedente non era finita)
    inSentence_ = true;
    overflow_ = false;
    len_ = 0;
    return false;
  }
  if (!inSentence_) return false;
  if (c == '\r' || c == '\n') {
    inSentence_ = false;
    if (overflow_ || len_ < 4) {
      if (len_ > 0 || overflow_) badChecksum_++;
      len_ = 0;
      return false;
    }
    buf_[len_] = 0;
    // body*HH
    char* star = strrchr(buf_, '*');
    if (!star || strlen(star) != 3) {
      badChecksum_++;
      return false;
    }
    const int h1 = hexVal(star[1]), h2 = hexVal(star[2]);
    if (h1 < 0 || h2 < 0) {
      badChecksum_++;
      return false;
    }
    uint8_t x = 0;
    for (const char* p = buf_; p < star; p++) x ^= static_cast<uint8_t>(*p);
    if (x != static_cast<uint8_t>(h1 * 16 + h2)) {
      badChecksum_++;
      return false;
    }
    *star = 0;
    sentences_++;
    return handle(buf_);
  }
  if (len_ >= kMaxSentence) {
    overflow_ = true;
    return false;
  }
  buf_[len_++] = c;
  return false;
}

bool NmeaParser::handle(char* body) {
  char* f[16];
  const size_t n = splitFields(body, f, 16);
  if (n < 1 || strlen(f[0]) != 5) return false;
  const char* type = f[0] + 2;  // dopo la sigla del sistema (GP, GN, GL...)

  if (strcmp(type, "GGA") == 0) {
    // 0 tipo, 1 ora, 2 lat, 3 N/S, 4 lon, 5 E/W, 6 qualita', 7 satelliti
    if (n < 8) return false;
    const int quality = parseUint(f[6]);
    const int sats = parseUint(f[7]);
    if (quality <= 0) {  // 0 = nessun fix; ogni altro caso non valido e' trattato come nessun fix
      fix_.valid = false;
      fix_.sats = sats > 0 && sats < 100 ? static_cast<uint8_t>(sats) : 0;
      return true;
    }
    int32_t la, lo;
    if (f[3][0] == 0 || f[5][0] == 0 || !parseCoord(f[2], 2, f[3][0], la) || !parseCoord(f[4], 3, f[5][0], lo)) return false;
    if (!isValidCoordinate(la, lo)) return false;
    fix_.valid = true;
    fix_.latE7 = la;
    fix_.lonE7 = lo;
    fix_.sats = sats > 0 && sats < 100 ? static_cast<uint8_t>(sats) : 0;
    return true;
  }
  if (strcmp(type, "RMC") == 0) {
    // 0 tipo, 1 ora, 2 stato (A valido, V non valido), 3 lat, 4 N/S, 5 lon, 6 E/W
    if (n < 7) return false;
    if (f[2][0] != 'A') {
      fix_.valid = false;
      return true;
    }
    int32_t la, lo;
    if (f[4][0] == 0 || f[6][0] == 0 || !parseCoord(f[3], 2, f[4][0], la) || !parseCoord(f[5], 3, f[6][0], lo)) return false;
    if (!isValidCoordinate(la, lo)) return false;
    fix_.valid = true;
    fix_.latE7 = la;
    fix_.lonE7 = lo;
    return true;
  }
  return false;
}

Position choosePosition(const GpsFix& fix, uint32_t fixAgeMs, const Position& qth) {
  if (fix.valid && fixAgeMs <= kGpsMaxAgeMs) {
    Position p;
    p.valid = true;
    p.fromGps = true;
    p.latE7 = fix.latE7;
    p.lonE7 = fix.lonE7;
    return p;
  }
  return qth;  // posizione fissa, o non valida se non impostata
}

}  // namespace lcr
