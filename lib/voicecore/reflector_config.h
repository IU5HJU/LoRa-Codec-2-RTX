// Il file dei client del reflector (docs/reflector.md): una riga "id chiave nome [etichetta]" per client, righe vuote e commenti (#) ignorati.
// Un radioamatore puo' avere piu' nodi (casa, mare, hotspot): lo stesso nominativo su piu' righe, ciascuna con il suo id, la sua chiave (una chiave
// non si puo' usare due volte) e un'etichetta che li distingue. Qui il leggere,
// lo scrivere e il controllo dei valori, senza file ne' rete (tests/test_reflector_config.cpp): lo usa tools/vlreflector.cpp.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "node_link.h"

namespace lcr {

struct ReflectorEntry {
  uint32_t id = 0;
  uint8_t key[kNodeKeyBytes] = {0};
  std::string name;   // di solito il nominativo del radioamatore (puo' ripetersi su piu' nodi)
  std::string label;  // facoltativa: distingue i nodi dello stesso nominativo ("casa", "mare", "hotspot")
  // Come si mostra nei registri: "IZ5IOF" oppure "IZ5IOF casa".
  std::string display() const { return label.empty() ? name : name + " " + label; }
};

constexpr size_t kMaxClientName = 24;
constexpr size_t kMaxClientLabel = 16;
// Etichetta: lettere, cifre, '_', '-' e '.', da 1 a kMaxClientLabel caratteri (vuota = nessuna).
bool isValidClientLabel(const std::string& s);

// Nome di un client: lettere, cifre, '/', '-', '_' e '.', da 1 a kMaxClientName caratteri (nel file ci sono anche nomi come "ripetitore-rx").
bool isValidClientName(const std::string& s);
// Chiave: 32 cifre esadecimali.
bool parseKeyHex(const std::string& s, uint8_t out[kNodeKeyBytes]);
std::string keyToHex(const uint8_t key[kNodeKeyBytes]);

// Legge il testo del file. false con 'err' (messaggio con il numero di riga) se una riga non e' valida, un id o una chiave e' ripetuto, o la stessa
// coppia nome + etichetta compare due volte (i nomi si confrontano senza distinguere maiuscole). reflectorId non puo' essere un id di client.
bool parseReflectorConfig(const std::string& text, uint32_t reflectorId, std::vector<ReflectorEntry>& out, std::string& err);
// Il testo del file (con un'intestazione).
std::string formatReflectorConfig(const std::vector<ReflectorEntry>& entries);
// Il piu' piccolo id libero da 2 in su (diverso da quello del reflector).
uint32_t nextFreeClientId(const std::vector<ReflectorEntry>& entries, uint32_t reflectorId);
// Posizione del primo client con quel nome (senza distinguere maiuscole), -1 se non c'e'.
int findEntryByName(const std::vector<ReflectorEntry>& entries, const std::string& name);
// Tutti i client di un nominativo (le posizioni).
std::vector<int> findEntriesByName(const std::vector<ReflectorEntry>& entries, const std::string& name);
// Il client con quel nome e quell'etichetta (vuota = senza etichetta), -1 se non c'e'.
int findEntryByNameLabel(const std::vector<ReflectorEntry>& entries, const std::string& name, const std::string& label);
// Posizione del client che usa quella chiave, -1 se nessuno ('except' = una posizione da non contare, o -1).
int findEntryByKey(const std::vector<ReflectorEntry>& entries, const uint8_t key[kNodeKeyBytes], int except = -1);
int findEntryById(const std::vector<ReflectorEntry>& entries, uint32_t id);

}  // namespace lcr
