#include "admin_auth.h"
#include "testing.h"

using namespace lcr;

int main() {
  std::string e;
  CHECK(isValidAdminPassword("ReflectorIU5!", e));
  CHECK(!isValidAdminPassword("corta", e) && e.find("almeno 8") != std::string::npos);
  CHECK(!isValidAdminPassword(std::string(65, 'a'), e) && isValidAdminPassword(std::string(64, 'a'), e));
  CHECK(!isValidAdminPassword("password con \xC3\xA8 accentata", e));
  CHECK(!isValidAdminPassword(" spazio iniziale", e) && !isValidAdminPassword("spazio finale ", e));
  CHECK(isValidAdminPassword("con spazi dentro 1", e));

  const uint8_t salt[kAdminSaltBytes] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  const std::string line = makePasswordLine("ReflectorIU5!", salt);
  CHECK(line.compare(0, 4, "pw1:") == 0 && line.back() == '\n' && line.size() == 4 + 32 + 1 + 64 + 1);
  CHECK(line.find("ReflectorIU5!") == std::string::npos);  // la password non e' nel file
  AdminAuth a;
  CHECK(parseAdminAuth(line, a) && a.password && a.valid());
  CHECK(checkAdminSecret(a, "ReflectorIU5!"));
  CHECK(!checkAdminSecret(a, "reflectoriu5!") && !checkAdminSecret(a, "") && !checkAdminSecret(a, "ReflectorIU5") && !checkAdminSecret(a, line));
  // sale diverso = riga diversa per la stessa password
  uint8_t salt2[kAdminSaltBytes] = {9};
  CHECK(makePasswordLine("ReflectorIU5!", salt2) != line);
  // righe rovinate
  CHECK(!parseAdminAuth("pw1:00:00", a) && !parseAdminAuth("pw1:", a) && !parseAdminAuth("pw1:zz", a) && !parseAdminAuth("", a) && !parseAdminAuth("corto", a));
  // il vecchio token continua a funzionare
  AdminAuth t;
  CHECK(parseAdminAuth("00112233445566778899aabbccddeeff\n", t) && !t.password && t.valid());
  CHECK(checkAdminSecret(t, "00112233445566778899aabbccddeeff") && !checkAdminSecret(t, "00112233445566778899aabbccddeefe") && !checkAdminSecret(t, ""));
  TEST_MAIN_END();
}
