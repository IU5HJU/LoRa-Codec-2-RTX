// Impostazioni dell'operatore modificabili senza ricompilare (nominativo, frequenza, potenza).
//
// I valori vengono salvati nella memoria non volatile della scheda (NVS) e letti all'avvio;
// se mancano o non sono validi si usano quelli compilati in src/user_config.h. Una modifica
// vale dal riavvio successivo: la radio non cambia parametri mentre e' in uso.
//
// La console e' un protocollo a righe sulla seriale, pensato per la pagina web di
// configurazione (Web Serial) ma usabile anche a mano:
//
//   CFG GET                      -> CFG callsign=IU5HJU freq=433.900 power=10 board=T3 reboot=0
//   CFG SET CALLSIGN IU5HJU      -> CFG OK callsign=IU5HJU     (oppure CFG ERR callsign <motivo>)
//   CFG SET FREQ 433.900         -> CFG OK freq=433.900        (MHz, 430.000-440.000)
//   CFG SET POWER 10             -> CFG OK power=10            (dBm, 2-17)
//   CFG RESET                    -> CFG OK reset               (torna ai valori compilati)
//   CFG REBOOT                   -> riavvia la scheda
//
// Indipendente dalla scheda e testata sul PC (tests/test_settings.cpp).
#pragma once

#include <cstddef>
#include <cstdint>

#include "protocol.h"

namespace lcr {

constexpr uint32_t kMinFreqKhz = 430000;  // banda dei 70 cm, IARU Regione 1: 430-440 MHz
constexpr uint32_t kMaxFreqKhz = 440000;
constexpr int kMinPowerDbm = 2;   // limiti dell'SX1278 senza PA_BOOST
constexpr int kMaxPowerDbm = 17;

struct Settings {
  char callsign[kMaxCallsign + 1] = {0};
  uint32_t freqKhz = 0;
  int powerDbm = 0;

  float freqMhz() const { return static_cast<float>(freqKhz) / 1000.0f; }
};

bool operator==(const Settings& a, const Settings& b);
inline bool operator!=(const Settings& a, const Settings& b) { return !(a == b); }

// true se nominativo, frequenza e potenza sono nei limiti.
bool isValidSettings(const Settings& s);

// Memoria delle impostazioni (NVS sulla scheda, finta nei test).
class ISettingsStorage {
 public:
  virtual ~ISettingsStorage() {}
  // false se non c'e' nulla di salvato o i dati sono illeggibili.
  virtual bool load(Settings& out) = 0;
  virtual bool save(const Settings& s) = 0;
  virtual bool erase() = 0;
};

// Valori da usare all'avvio: quelli salvati se validi, altrimenti i predefiniti.
Settings loadSettings(ISettingsStorage& storage, const Settings& defaults);

enum class ConsoleResult {
  NotCommand,  // la riga non e' per la console (nessuna risposta)
  Ok,
  Error,
  Reboot,  // risposta pronta; il chiamante deve riavviare la scheda
};

class SettingsConsole {
 public:
  // active: valori in uso (letti all'avvio). board: nome breve per CFG GET.
  SettingsConsole(const Settings& active, const Settings& defaults, ISettingsStorage& storage, const char* board);

  // Elabora una riga (senza il fine riga). In reply scrive la risposta, senza fine riga.
  ConsoleResult handleLine(const char* line, char* reply, size_t cap);

  // Valori salvati (quelli che saranno attivi al prossimo avvio).
  const Settings& stored() const { return stored_; }

 private:
  ConsoleResult get(char* reply, size_t cap);
  ConsoleResult set(const char* key, const char* value, char* reply, size_t cap);
  ConsoleResult reset(char* reply, size_t cap);

  Settings active_;
  Settings defaults_;
  Settings stored_;
  ISettingsStorage& storage_;
  const char* board_;
};

}  // namespace lcr
