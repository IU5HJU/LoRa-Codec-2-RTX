// Il file dei client del reflector (docs/reflector.md): una riga "id chiave nome" per client, righe vuote e commenti (#) ignorati. Qui il leggere,
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
  std::string name;  // di solito il nominativo del radioamatore
};

constexpr size_t kMaxClientName = 24;

// Nome di un client: lettere, cifre, '/', '-', '_' e '.', da 1 a kMaxClientName caratteri (nel file ci sono anche nomi come "ripetitore-rx").
bool isValidClientName(const std::string& s);
// Chiave: 32 cifre esadecimali.
bool parseKeyHex(const std::string& s, uint8_t out[kNodeKeyBytes]);
std::string keyToHex(const uint8_t key[kNodeKeyBytes]);

// Legge il testo del file. false con 'err' (messaggio con il numero di riga) se una riga non e' valida o un id o un nome e' ripetuto (i nomi si
// confrontano senza distinguere maiuscole: un nominativo ha una sola riga). reflectorId non puo' essere un id di client.
bool parseReflectorConfig(const std::string& text, uint32_t reflectorId, std::vector<ReflectorEntry>& out, std::string& err);
// Il testo del file (con un'intestazione).
std::string formatReflectorConfig(const std::vector<ReflectorEntry>& entries);
// Il piu' piccolo id libero da 2 in su (diverso da quello del reflector).
uint32_t nextFreeClientId(const std::vector<ReflectorEntry>& entries, uint32_t reflectorId);
// Posizione del client con quel nome (senza distinguere maiuscole), -1 se non c'e'.
int findEntryByName(const std::vector<ReflectorEntry>& entries, const std::string& name);
int findEntryById(const std::vector<ReflectorEntry>& entries, uint32_t id);

}  // namespace lcr
