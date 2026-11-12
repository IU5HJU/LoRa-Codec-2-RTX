#include "radio_test.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "morse.h"

namespace lcr {

namespace {

void fail(char* err, size_t cap, const char* msg) {
  if (err && cap) snprintf(err, cap, "%s", msg);
}

bool parseNumber(const char* s, float& v) {
  char* end = nullptr;
  v = strtof(s, &end);
  return end != s && *end == 0 && std::isfinite(v);
}

// "chiave=valore" in minuscolo: true se la chiave e' key
bool keyIs(const char* tok, const char* key, const char** val) {
  const size_t n = strlen(key);
  if (strncasecmp(tok, key, n) != 0 || tok[n] != '=') return false;
  *val = tok + n + 1;
  return true;
}

bool parseOption(const char* tok, RadioTestRequest& r, char* err, size_t cap) {
  const char* v = nullptr;
  float f = 0;
  if (keyIs(tok, "wpm", &v)) {
    if (!parseNumber(v, f) || f < 5 || f > 30 || f != std::floor(f)) return fail(err, cap, "wpm da 5 a 30"), false;
    r.wpm = static_cast<int>(f);
  } else if (keyIs(tok, "hz", &v)) {
    if (!parseNumber(v, f) || f < 300 || f > 2500) return fail(err, cap, "hz da 300 a 2500"), false;
    r.toneHz = f;
  } else if (keyIs(tok, "dev", &v)) {
    if (!parseNumber(v, f) || f < 0.6f || f > 10.0f) return fail(err, cap, "dev da 0.6 a 10 (kHz)"), false;
    r.devKhz = f;
  } else if (keyIs(tok, "ramp", &v)) {
    if (!parseNumber(v, f) || f < 0 || f > 15 || f != std::floor(f)) return fail(err, cap, "ramp da 0 a 15"), false;
    r.ramp = static_cast<int>(f);
  } else if (keyIs(tok, "shape", &v)) {
    if (!parseNumber(v, f)) return fail(err, cap, "shape: 0, 0.3, 0.5 o 1.0"), false;
    if (f == 0.0f) r.shape = 0;
    else if (std::fabs(f - 0.3f) < 0.01f) r.shape = 3;
    else if (std::fabs(f - 0.5f) < 0.01f) r.shape = 5;
    else if (std::fabs(f - 1.0f) < 0.01f) r.shape = 10;
    else return fail(err, cap, "shape: 0, 0.3, 0.5 o 1.0"), false;
  } else {
    return fail(err, cap, "parametro sconosciuto (wpm, hz, dev, ramp, shape)"), false;
  }
  return true;
}

bool parseText(const char* t, RadioTestRequest& r, char* err, size_t cap) {
  const size_t n = strlen(t);
  if (n == 0 || n > sizeof(r.text) - 1) return fail(err, cap, "testo da 1 a 16 caratteri"), false;
  for (size_t i = 0; i < n; i++) {
    const char c = t[i];
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return fail(err, cap, "testo: solo lettere, cifre e _ (spazio)"), false;
    r.text[i] = c == '_' ? ' ' : static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }
  r.text[n] = 0;
  return true;
}

}  // namespace

bool parseRadioTest(const char* args, RadioTestRequest& out, char* err, size_t cap) {
  RadioTestRequest r;
  char buf[96];
  if (!args || strlen(args) >= sizeof(buf)) return fail(err, cap, "comando troppo lungo"), false;
  strcpy(buf, args);
  char* tok[12];
  size_t n = 0;
  for (char* p = strtok(buf, " \t"); p && n < 12; p = strtok(nullptr, " \t")) tok[n++] = p;
  if (n == 0) return fail(err, cap, "usa MORSE, BEACON, CARRIER oppure TONE"), false;

  size_t i = 1;
  if (strcasecmp(tok[0], "MORSE") == 0 || strcasecmp(tok[0], "BEACON") == 0) {
    r.kind = strcasecmp(tok[0], "MORSE") == 0 ? RadioTestKind::Morse : RadioTestKind::Beacon;
    if (n < 3) return fail(err, cap, "usa MORSE|BEACON FM|CW TESTO ..."), false;
    if (strcasecmp(tok[1], "FM") == 0) r.fm = true;
    else if (strcasecmp(tok[1], "CW") == 0) r.fm = false;
    else return fail(err, cap, "il modo e' FM oppure CW"), false;
    if (!parseText(tok[2], r, err, cap)) return false;
    i = 3;
    if (r.kind == RadioTestKind::Beacon) {
      float f = 0;
      if (n < 4 || !parseNumber(tok[3], f) || f < 10 || f > 3600 || f != std::floor(f)) return fail(err, cap, "BEACON: periodo da 10 a 3600 secondi"), false;
      r.periodS = static_cast<int>(f);
      i = 4;
    }
  } else if (strcasecmp(tok[0], "CARRIER") == 0 || strcasecmp(tok[0], "TONE") == 0) {
    r.kind = strcasecmp(tok[0], "CARRIER") == 0 ? RadioTestKind::Carrier : RadioTestKind::Tone;
    r.fm = r.kind == RadioTestKind::Tone;
    if (n >= 2 && std::isdigit(static_cast<unsigned char>(tok[1][0]))) {
      float f = 0;
      if (!parseNumber(tok[1], f) || f < 1 || f > 30 || f != std::floor(f)) return fail(err, cap, "durata da 1 a 30 secondi"), false;
      r.seconds = static_cast<int>(f);
      i = 2;
    }
  } else {
    return fail(err, cap, "usa MORSE, BEACON, CARRIER oppure TONE"), false;
  }
  for (; i < n; i++)
    if (!parseOption(tok[i], r, err, cap)) return false;
  out = r;
  return true;
}

float radioTestBitRateKbps(const RadioTestRequest& r) {
  const bool fm = (r.kind == RadioTestKind::Tone) || ((r.kind == RadioTestKind::Morse || r.kind == RadioTestKind::Beacon) && r.fm);
  return fm ? 2.0f * r.toneHz / 1000.0f : kCwBitRateKbps;
}

float burstAirMs(uint16_t bytes, float kbps) { return static_cast<float>(bytes) * 8.0f / kbps; }

size_t planMorseBursts(const RadioTestRequest& r, MorseBurst* out, size_t cap, bool* clamped) {
  if (clamped) *clamped = false;
  if (!out || cap == 0) return 0;
  const uint16_t unitMs = static_cast<uint16_t>(1200 / (r.wpm > 0 ? r.wpm : 15));
  MorseElement el[160];
  const size_t n = morseElements(r.text, unitMs, el, sizeof(el) / sizeof(el[0]));
  const float kbps = radioTestBitRateKbps(r);
  size_t k = 0;
  for (size_t i = 0; i < n && k < cap; i++) {
    if (!el[i].tone) {
      out[k++] = {false, el[i].ms, 0};
      continue;
    }
    long bytes = std::lround(static_cast<double>(el[i].ms) * kbps / 8.0);
    if (bytes < 1) bytes = 1;
    if (bytes > 255) {
      bytes = 255;
      if (clamped) *clamped = true;
    }
    out[k++] = {true, el[i].ms, static_cast<uint16_t>(bytes)};
  }
  return k;
}

}  // namespace lcr
