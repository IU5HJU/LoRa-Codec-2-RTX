// Accesso alla pagina di amministrazione del reflector (tools/vlreflector.cpp): una password scelta dal sysop.
// Nel file (admin.token, permessi riservati) la password non si scrive in chiaro: c'e' una riga "pw1:<sale>:<hash>" con
// hash = SHA-256(sale + password). Per compatibilita' resta valido il vecchio token casuale (almeno 16 caratteri, scritto in chiaro).
// Indipendente dal sistema e provato sul PC (tests/test_admin_auth.cpp).
#pragma once

#include <cstdint>
#include <string>

#include "sha256.h"

namespace lcr {

constexpr size_t kAdminSaltBytes = 16;
constexpr size_t kAdminPasswordMin = 8;
constexpr size_t kAdminPasswordMax = 64;

struct AdminAuth {
  bool password = false;  // true: riga pw1 (sale + hash); false: vecchio token in chiaro
  std::string token;      // il vecchio token
  uint8_t salt[kAdminSaltBytes] = {0};
  uint8_t hash[kSha256Bytes] = {0};
  bool valid() const { return password || !token.empty(); }
};

// Password ammessa: da 8 a 64 caratteri ASCII stampabili, senza spazi all'inizio o alla fine (viaggia in un'intestazione HTTP).
bool isValidAdminPassword(const std::string& pw, std::string& err);
// La riga da scrivere nel file ("pw1:<sale in esadecimale>:<hash in esadecimale>\n").
std::string makePasswordLine(const std::string& pw, const uint8_t salt[kAdminSaltBytes]);
// Legge il contenuto del file: una riga pw1 oppure un vecchio token di almeno 16 caratteri. false se non e' ne' l'una ne' l'altro.
bool parseAdminAuth(const std::string& fileText, AdminAuth& out);
// La password (o il token) scritta nella pagina e' quella giusta? Confronto a tempo costante.
bool checkAdminSecret(const AdminAuth& a, const std::string& given);

}  // namespace lcr
