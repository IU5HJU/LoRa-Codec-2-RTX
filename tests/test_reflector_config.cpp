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
  CHECK(!parseReflectorConfig(std::string("2 ") + kK1 + " a b c\n", 1000, v, err));  // dopo la chiave: il nome e al massimo l'etichetta
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
  // piu' nodi dello stesso nominativo, ciascuno con la sua chiave e un'etichetta
  const std::string multi = std::string("2 ") + kK1 + " IZ5IOF casa\n3 " + kK2 + " IZ5IOF mare\n4 00112233445566778899aabbccddeeff iz5iof hotspot\n5 aabbccddeeff00112233445566778899 IU5HJU\n";
  CHECK(parseReflectorConfig(multi, 1000, v, err) && v.size() == 4 && v[0].label == "casa" && v[2].name == "iz5iof" && v[3].label.empty());
  CHECK(v[0].display() == "IZ5IOF casa" && v[3].display() == "IU5HJU");
  CHECK(findEntriesByName(v, "IZ5IOF").size() == 3 && findEntriesByName(v, "iu5hju").size() == 1 && findEntriesByName(v, "X").empty());
  CHECK(findEntryByNameLabel(v, "iz5iof", "MARE") == 1 && findEntryByNameLabel(v, "IZ5IOF", "") == -1 && findEntryByNameLabel(v, "IU5HJU", "") == 3);
  const std::string multiText = formatReflectorConfig(v);
  CHECK(multiText.find("IZ5IOF casa") != std::string::npos && multiText.find("IU5HJU\n") != std::string::npos);
  std::vector<ReflectorEntry> back;
  CHECK(parseReflectorConfig(multiText, 1000, back, err) && back.size() == 4 && back[1].label == "mare" && back[3].label.empty());
  // la stessa coppia nominativo + etichetta non puo' ripetersi; senza etichetta nemmeno
  CHECK(!parseReflectorConfig(std::string("2 ") + kK1 + " IZ5IOF casa\n3 " + kK2 + " iz5iof CASA\n", 1000, v, err) && err.find("riga 2") != std::string::npos);
  // una chiave non si usa due volte, nemmeno per nominativi diversi
  CHECK(!parseReflectorConfig(std::string("2 ") + kK1 + " IZ5IOF casa\n3 " + kK1 + " IZ5IOF mare\n", 1000, v, err) && err.find("chiave") != std::string::npos && err.find("riga 2") != std::string::npos);
  CHECK(!parseReflectorConfig(std::string("2 ") + kK1 + " AA\n3 " + "000102030405060708090A0B0C0D0E0F" + " BB\n", 1000, v, err));  // maiuscole indifferenti nella chiave
  // etichetta non valida o parole di troppo
  CHECK(!parseReflectorConfig(std::string("2 ") + kK1 + " IZ5IOF ca$a\n", 1000, v, err));
  CHECK(!parseReflectorConfig(std::string("2 ") + kK1 + " IZ5IOF casa altro\n", 1000, v, err));
  CHECK(isValidClientLabel("casa") && isValidClientLabel("mare-2") && isValidClientLabel("hot_spot.1") && !isValidClientLabel("") && !isValidClientLabel("a b") &&
        !isValidClientLabel(std::string(17, 'a')) && isValidClientLabel(std::string(16, 'a')) && !isValidClientLabel("a/b"));
  // chi usa una chiave
  uint8_t kk[kNodeKeyBytes];
  CHECK(parseReflectorConfig(multi, 1000, v, err) && parseKeyHex(kK2, kk) && findEntryByKey(v, kk) == 1 && findEntryByKey(v, kk, 1) == -1);
  TEST_MAIN_END();
}
