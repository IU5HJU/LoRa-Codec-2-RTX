#include "reflector_config.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace lcr {

bool isValidClientName(const std::string& s) {
  if (s.empty() || s.size() > kMaxClientName) return false;
  for (char c : s) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (!(isalnum(u) || c == '/' || c == '-' || c == '_' || c == '.')) return false;
  }
  return true;
}

bool parseKeyHex(const std::string& s, uint8_t out[kNodeKeyBytes]) {
  if (s.size() != 2 * kNodeKeyBytes) return false;
  for (size_t i = 0; i < s.size(); i++) {
    const char c = s[i];
    const int v = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
    if (v < 0) return false;
    if (i % 2 == 0) out[i / 2] = static_cast<uint8_t>(v << 4);
    else out[i / 2] = static_cast<uint8_t>(out[i / 2] | v);
  }
  return true;
}

std::string keyToHex(const uint8_t key[kNodeKeyBytes]) {
  static const char* hex = "0123456789abcdef";
  std::string s;
  for (size_t i = 0; i < kNodeKeyBytes; i++) {
    s += hex[key[i] >> 4];
    s += hex[key[i] & 15];
  }
  return s;
}

bool isValidClientLabel(const std::string& s) {
  if (s.empty() || s.size() > kMaxClientLabel) return false;
  for (char c : s) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (!(isalnum(u) || c == '-' || c == '_' || c == '.')) return false;
  }
  return true;
}

static bool sameNameCI(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); i++) {
    if (toupper(static_cast<unsigned char>(a[i])) != toupper(static_cast<unsigned char>(b[i]))) return false;
  }
  return true;
}

int findEntryByName(const std::vector<ReflectorEntry>& entries, const std::string& name) {
  for (size_t i = 0; i < entries.size(); i++) {
    if (sameNameCI(entries[i].name, name)) return static_cast<int>(i);
  }
  return -1;
}

std::vector<int> findEntriesByName(const std::vector<ReflectorEntry>& entries, const std::string& name) {
  std::vector<int> v;
  for (size_t i = 0; i < entries.size(); i++) {
    if (sameNameCI(entries[i].name, name)) v.push_back(static_cast<int>(i));
  }
  return v;
}

int findEntryByNameLabel(const std::vector<ReflectorEntry>& entries, const std::string& name, const std::string& label) {
  for (size_t i = 0; i < entries.size(); i++) {
    if (sameNameCI(entries[i].name, name) && sameNameCI(entries[i].label, label)) return static_cast<int>(i);
  }
  return -1;
}

int findEntryByKey(const std::vector<ReflectorEntry>& entries, const uint8_t key[kNodeKeyBytes], int except) {
  for (size_t i = 0; i < entries.size(); i++) {
    if (static_cast<int>(i) != except && memcmp(entries[i].key, key, kNodeKeyBytes) == 0) return static_cast<int>(i);
  }
  return -1;
}

int findEntryById(const std::vector<ReflectorEntry>& entries, uint32_t id) {
  for (size_t i = 0; i < entries.size(); i++) {
    if (entries[i].id == id) return static_cast<int>(i);
  }
  return -1;
}

bool parseReflectorConfig(const std::string& text, uint32_t reflectorId, std::vector<ReflectorEntry>& result, std::string& err) {
  result.clear();
  std::vector<ReflectorEntry> out;  // si consegna solo se tutto il file e' valido
  std::istringstream in(text);
  std::string line;
  int lineNo = 0;
  while (std::getline(in, line)) {
    lineNo++;
    const size_t h = line.find('#');
    if (h != std::string::npos) line.resize(h);
    std::istringstream ss(line);
    std::string idText, keyText, name, label, extra;
    if (!(ss >> idText)) continue;
    ss >> keyText >> name >> label >> extra;
    char msg[160];
    char* end = nullptr;
    const unsigned long id = strtoul(idText.c_str(), &end, 10);
    ReflectorEntry e;
    if (end == idText.c_str() || *end != 0 || id == 0 || id > 0xFFFFFFFFul || id == reflectorId) {
      snprintf(msg, sizeof(msg), "riga %d: l'id deve essere un numero diverso da 0 e da quello del reflector", lineNo);
      err = msg;
      return false;
    }
    if (!parseKeyHex(keyText, e.key)) {
      snprintf(msg, sizeof(msg), "riga %d: la chiave deve avere 32 cifre esadecimali", lineNo);
      err = msg;
      return false;
    }
    if (!isValidClientName(name) || !extra.empty() || (!label.empty() && !isValidClientLabel(label))) {
      snprintf(msg, sizeof(msg), "riga %d: dopo la chiave vanno il nome (una parola: lettere cifre / - _ .) e, facoltativa, l'etichetta (una parola: lettere cifre - _ .)", lineNo);
      err = msg;
      return false;
    }
    e.id = static_cast<uint32_t>(id);
    e.name = name;
    e.label = label;
    if (findEntryById(out, e.id) >= 0) {
      snprintf(msg, sizeof(msg), "riga %d: l'id %lu e' ripetuto", lineNo, id);
      err = msg;
      return false;
    }
    if (findEntryByNameLabel(out, e.name, e.label) >= 0) {
      snprintf(msg, sizeof(msg), "riga %d: %s%s%s e' ripetuto (un nominativo con piu' nodi li distingue con l'etichetta)", lineNo, name.c_str(), label.empty() ? "" : " ", label.c_str());
      err = msg;
      return false;
    }
    if (findEntryByKey(out, e.key) >= 0) {
      snprintf(msg, sizeof(msg), "riga %d: la chiave e' gia' usata da un altro nodo (ogni nodo ha la sua)", lineNo);
      err = msg;
      return false;
    }
    out.push_back(e);
  }
  err.clear();
  result = out;
  return true;
}

std::string formatReflectorConfig(const std::vector<ReflectorEntry>& entries) {
  std::string s = "# id  chiave (32 cifre esadecimali)       nominativo [etichetta del nodo]\n";
  for (const auto& e : entries) {
    char b[24];
    snprintf(b, sizeof(b), "%-5u ", static_cast<unsigned>(e.id));
    s += b;
    s += keyToHex(e.key);
    s += "  ";
    s += e.name;
    if (!e.label.empty()) {
      s += " ";
      s += e.label;
    }
    s += "\n";
  }
  return s;
}

uint32_t nextFreeClientId(const std::vector<ReflectorEntry>& entries, uint32_t reflectorId) {
  for (uint32_t id = 2;; id++) {
    if (id != reflectorId && findEntryById(entries, id) < 0) return id;
  }
}

}  // namespace lcr
