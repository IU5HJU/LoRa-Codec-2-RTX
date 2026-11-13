#include "admin_auth.h"

#include <cstring>

#include "http_mini.h"

namespace lcr {

static std::string hex(const uint8_t* b, size_t n) {
  static const char* d = "0123456789abcdef";
  std::string s;
  for (size_t i = 0; i < n; i++) {
    s += d[b[i] >> 4];
    s += d[b[i] & 15];
  }
  return s;
}

static bool unhex(const std::string& s, uint8_t* out, size_t n) {
  if (s.size() != n * 2) return false;
  for (size_t i = 0; i < n; i++) {
    int v[2];
    for (int k = 0; k < 2; k++) {
      const char c = s[i * 2 + static_cast<size_t>(k)];
      if (c >= '0' && c <= '9') v[k] = c - '0';
      else if (c >= 'a' && c <= 'f') v[k] = c - 'a' + 10;
      else if (c >= 'A' && c <= 'F') v[k] = c - 'A' + 10;
      else return false;
    }
    out[i] = static_cast<uint8_t>(v[0] * 16 + v[1]);
  }
  return true;
}

static void hashOf(const uint8_t salt[kAdminSaltBytes], const std::string& pw, uint8_t out[kSha256Bytes]) {
  std::string m(reinterpret_cast<const char*>(salt), kAdminSaltBytes);
  m += pw;
  sha256(reinterpret_cast<const uint8_t*>(m.data()), m.size(), out);
}

bool isValidAdminPassword(const std::string& pw, std::string& err) {
  if (pw.size() < kAdminPasswordMin) {
    err = "la password deve avere almeno 8 caratteri";
    return false;
  }
  if (pw.size() > kAdminPasswordMax) {
    err = "la password puo' avere al massimo 64 caratteri";
    return false;
  }
  for (char c : pw) {
    if (c < 0x20 || c > 0x7E) {
      err = "la password puo' avere solo caratteri ASCII stampabili (niente lettere accentate)";
      return false;
    }
  }
  if (pw.front() == ' ' || pw.back() == ' ') {
    err = "la password non puo' cominciare o finire con uno spazio";
    return false;
  }
  return true;
}

std::string makePasswordLine(const std::string& pw, const uint8_t salt[kAdminSaltBytes]) {
  uint8_t h[kSha256Bytes];
  hashOf(salt, pw, h);
  return "pw1:" + hex(salt, kAdminSaltBytes) + ":" + hex(h, kSha256Bytes) + "\n";
}

bool parseAdminAuth(const std::string& fileText, AdminAuth& out) {
  std::string t = fileText;
  while (!t.empty() && (t.back() == '\n' || t.back() == '\r' || t.back() == ' ')) t.pop_back();
  AdminAuth a;
  if (t.compare(0, 4, "pw1:") == 0) {
    const size_t sep = t.find(':', 4);
    if (sep == std::string::npos) return false;
    if (!unhex(t.substr(4, sep - 4), a.salt, kAdminSaltBytes) || !unhex(t.substr(sep + 1), a.hash, kSha256Bytes)) return false;
    a.password = true;
  } else {
    if (t.size() < 16 || t.find('\n') != std::string::npos) return false;
    a.token = t;
  }
  out = a;
  return true;
}

bool checkAdminSecret(const AdminAuth& a, const std::string& given) {
  if (given.empty()) return false;
  if (!a.password) return !a.token.empty() && constantTimeEquals(given, a.token);
  uint8_t h[kSha256Bytes];
  hashOf(a.salt, given, h);
  return constantTimeEquals(hex(h, kSha256Bytes), hex(a.hash, kSha256Bytes));
}

}  // namespace lcr
