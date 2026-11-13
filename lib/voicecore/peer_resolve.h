// Quando risolvere il nome del reflector (o dell'altro nodo) in un indirizzo: la logica del "quando", senza rete ne' orologio (tests/test_peer_resolve.cpp).
// Un indirizzo IPv4 scritto in cifre non si risolve mai. Un nome si risolve appena c'e' il Wi-Fi, poi ogni kRefreshMs (il reflector puo' cambiare
// indirizzo: DDNS), e prima se il collegamento non sale (almeno ogni kLinkDownMs); dopo un errore si riprova ogni kRetryMs.
#pragma once

#include <cstdint>

#include "settings.h"

namespace lcr {

class PeerResolveSchedule {
 public:
  static constexpr uint32_t kRefreshMs = 300000;  // 5 minuti
  static constexpr uint32_t kLinkDownMs = 30000;  // collegamento non attivo: si riprova ogni 30 s
  static constexpr uint32_t kRetryMs = 10000;     // l'ultima risoluzione e' fallita

  explicit PeerResolveSchedule(const char* host) : literal_(host && isValidIpv4(host)) {}

  bool literal() const { return literal_; }
  // true: il chiamante risolve adesso e poi dice com'e' andata con done().
  bool due(uint32_t nowMs, bool wifiUp, bool linkUp) const {
    if (literal_ || !wifiUp) return false;
    if (!tried_) return true;
    const uint32_t age = nowMs - lastMs_;
    if (!lastOk_) return age >= kRetryMs;
    return age >= kRefreshMs || (!linkUp && age >= kLinkDownMs);
  }
  void done(bool ok, uint32_t nowMs) {
    tried_ = true;
    lastOk_ = ok;
    lastMs_ = nowMs;
  }
  bool resolvedOnce() const { return tried_ && lastOk_; }

 private:
  bool literal_;
  bool tried_ = false;
  bool lastOk_ = false;
  uint32_t lastMs_ = 0;
};

}  // namespace lcr
