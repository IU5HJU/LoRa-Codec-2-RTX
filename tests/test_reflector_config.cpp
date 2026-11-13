#include <cstring>

#include "reflector_config.h"
#include "testing.h"

using namespace lcr;

static const char* kK1 = "000102030405060708090a0b0c0d0e0f";
static const char* kK2 = "101112131415161718191A1B1C1D1E1F";

int main() {
  std::vector<ReflectorEntry> v;
  std::string err;
  // un file normale: commenti, righe vuote, nomi con trattino, chiavi maiuscole o minuscole
  const std::string ok = std::string("# commento\n\n2   ") + kK1 + "  nodo   # nota\n3\t" + kK2 + "\tripetitore-rx\n";
  CHECK(parseReflectorConfig(ok, 1000, v, err) && v.size() == 2 && err.empty());
  CHECK(v[0].id == 2 && v[0].name == "nodo" && v[0].key[15] == 0x0f && v[1].id == 3 && v[1].name == "ripetitore-rx" && v[1].key[0] == 0x10 && v[1].key[10] == 0x1a);
  // scrivere e rileggere da' lo stesso
  const std::string text = formatReflectorConfig(v);
  std::vector<ReflectorEntry> w;
  CHECK(parseReflectorConfig(text, 1000, w, err) && w.size() == 2 && w[1].name == "ripetitore-rx" && memcmp(w[1].key, v[1].key, kNodeKeyBytes) == 0);
  CHECK(text.find("# id") == 0 && text.find(kK1) != std::string::npos);  // la chiave si scrive in minuscolo
  CHECK(keyToHex(v[1].key) == "101112131415161718191a1b1c1d1e1f");
  // errori, con il numero di riga
  CHECK(!parseReflectorConfig(std::string("2 ") + kK1 + " a\nx " + kK1 + " b\n", 1000, v, err) && err.find("riga 2") != std::string::npos && v.empty());
  CHECK(!parseReflectorConfig(std::string("0 ") + kK1 + " a\n", 1000, v, err) && err.find("riga 1") != std::string::npos);
  CHECK(!parseReflectorConfig(std::string("1000 ") + kK1 + " a\n", 1000, v, err));  // l'id del reflector
  CHECK(!parseReflectorConfig(std::string("2 ") + "00112233" + " a\n", 1000, v, err) && err.find("32 cifre") != std::string::npos);
  CHECK(!parseReflectorConfig(std::string("2 ") + "zz0102030405060708090a0b0c0d0e0f" + " a\n", 1000, v, err));
  CHECK(!parseReflectorConfig(std::string("2 ") + kK1 + "\n", 1000, v, err) && err.find("nome") != std::string::npos);
  CHECK(!parseReflectorConfig(std::string("2 ") + kK1 + " a b\n", 1000, v, err));  // una parola sola
  CHECK(!parseReflectorConfig(std::string("2 ") + kK1 + " a\n2 " + kK2 + " b\n", 1000, v, err) && err.find("ripetuto") != std::string::npos);
  CHECK(!parseReflectorConfig(std::string("2 ") + kK1 + " IZ5IOF\n3 " + kK2 + " iz5iof\n", 1000, v, err));  // lo stesso nominativo, maiuscole indifferenti
  CHECK(!parseReflectorConfig(std::string("99999999999 ") + kK1 + " a\n", 1000, v, err));
  CHECK(parseReflectorConfig("", 1000, v, err) && v.empty());
  // nomi e chiavi
  CHECK(isValidClientName("IZ5IOF") && isValidClientName("IU5HJU/P") && isValidClientName("nodo-1_a.b") && !isValidClientName("") && !isValidClientName("a b") &&
        !isValidClientName("a\"b") && !isValidClientName(std::string(25, 'a')) && isValidClientName(std::string(24, 'a')) && !isValidClientName("<x>"));
  uint8_t k[kNodeKeyBytes];
  CHECK(parseKeyHex(kK1, k) && !parseKeyHex("", k) && !parseKeyHex(std::string(31, 'a'), k) && !parseKeyHex(std::string(33, 'a'), k));
  // primo id libero
  CHECK(nextFreeClientId(v, 1000) == 2);
  CHECK(parseReflectorConfig(ok, 1000, v, err) && nextFreeClientId(v, 1000) == 4);
  std::vector<ReflectorEntry> g;
  for (uint32_t id : {2u, 3u, 5u}) {
    ReflectorEntry e;
    e.id = id;
    e.name = "n" + std::to_string(id);
    g.push_back(e);
  }
  CHECK(nextFreeClientId(g, 1000) == 4);  // un buco si riempie
  CHECK(nextFreeClientId(g, 4) == 6);     // l'id del reflector si salta
  CHECK(findEntryByName(g, "N3") == 1 && findEntryByName(g, "x") == -1 && findEntryById(g, 5) == 2 && findEntryById(g, 9) == -1);
  TEST_MAIN_END();
}
