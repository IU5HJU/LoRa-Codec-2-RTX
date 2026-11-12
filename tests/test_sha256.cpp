// SHA-256 e HMAC-SHA256 contro i vettori ufficiali (FIPS 180-4, RFC 4231).
#include <cstdio>
#include <cstring>
#include <string>

#include "sha256.h"
#include "testing.h"

using namespace lcr;

static std::string hex(const uint8_t* d, size_t n) {
  std::string s;
  char b[3];
  for (size_t i = 0; i < n; i++) {
    snprintf(b, sizeof(b), "%02x", d[i]);
    s += b;
  }
  return s;
}

int main() {
  uint8_t o[32];
  sha256(reinterpret_cast<const uint8_t*>(""), 0, o);
  CHECK(hex(o, 32) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  sha256(reinterpret_cast<const uint8_t*>("abc"), 3, o);
  CHECK(hex(o, 32) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  const char* two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  sha256(reinterpret_cast<const uint8_t*>(two), strlen(two), o);
  CHECK(hex(o, 32) == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  std::string million(1000000, 'a');
  sha256(reinterpret_cast<const uint8_t*>(million.data()), million.size(), o);
  CHECK(hex(o, 32) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
  // RFC 4231, caso 1 e 2, e caso 6 (chiave piu' lunga del blocco)
  uint8_t key1[20];
  memset(key1, 0x0b, sizeof(key1));
  hmacSha256(key1, 20, reinterpret_cast<const uint8_t*>("Hi There"), 8, o);
  CHECK(hex(o, 32) == "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
  hmacSha256(reinterpret_cast<const uint8_t*>("Jefe"), 4, reinterpret_cast<const uint8_t*>("what do ya want for nothing?"), 28, o);
  CHECK(hex(o, 32) == "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
  uint8_t key6[131];
  memset(key6, 0xaa, sizeof(key6));
  const char* m6 = "Test Using Larger Than Block-Size Key - Hash Key First";
  hmacSha256(key6, 131, reinterpret_cast<const uint8_t*>(m6), strlen(m6), o);
  CHECK(hex(o, 32) == "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
  TEST_MAIN_END();
}
