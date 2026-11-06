// Impostazioni dell'operatore modificabili senza ricompilare (nominativo, frequenza, potenza).
//
// I valori vengono salvati nella memoria non volatile della scheda (NVS) e letti all'avvio;
// se mancano o non sono validi si usano quelli compilati in src/user_config.h. Una modifica
// vale dal riavvio successivo: la radio non cambia parametri mentre e' in uso.
//
// La console e' un protocollo a righe sulla seriale, pensato per la pagina web di
// configurazione (Web Serial) ma usabile anche a mano:
//
//   CFG VERSION                  -> CFG VERSION fw=abc1234 board=t3
//   CFG GET                      -> CFG callsign=IU5HJU freq=433.900 power=10 volume=7 micfloor=-60 board=t3 reboot=0 test=0
//   CFG SET CALLSIGN IU5HJU      -> CFG OK callsign=IU5HJU     (oppure CFG ERR callsign <motivo>)
//   CFG SET FREQ 433.900         -> CFG OK freq=433.900        (MHz, 430.000-440.000)
//   CFG SET POWER 10             -> CFG OK power=10            (dBm, 2-17)
//   CFG SET MICFLOOR -60         -> CFG OK micfloor=-60        (fondo del microfono in dBFS, da -80 a -36)
//   CFG MIC MEASURE              -> misura il fondo per 3 s (stare in silenzio), poi: CFG MIC fondo=-61 min=-63 max=-58 frames=75
//   CFG SET VOLUME 7             -> CFG OK volume=7            (0 = muto, 1-10 in passi da 3 dB)
//   CFG TEST ON                  -> CFG OK test=on             (prova di collegamento, solo fino al riavvio; OFF la spegne)
//   CFG TEST AUDIO               -> suona 500, 1000 e 2000 Hz e misura il microfono: CFG AUDIO f=500 base=-90.1 tone=-52.3 ... CFG AUDIO fine
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
constexpr int kMinVolume = 0;  // muto
constexpr int kMaxVolume = 10;  // 0 dB
constexpr int kDefaultVolume = 7;

// Fondo del microfono: livello dell'ingresso dell'AGC (dopo il passa-alto) in silenzio. Sopra il fondo + kMicGateMarginDb
// l'AGC lavora (e amplifica); sotto, il guadagno scende e il rumore non viene amplificato.
// Standard -60 dBFS: stanza silenziosa con l'INMP441 (misurato -58...-61). Ognuno lo misura e lo corregge per il suo hardware.
constexpr int kMinMicFloorDb = -80;
constexpr int kMaxMicFloorDb = -36;
constexpr int kDefaultMicFloorDb = -60;
constexpr int kMicGateMarginDb = 6;
inline float micGateFromFloor(int floorDb) { return static_cast<float>(floorDb + kMicGateMarginDb); }

struct Settings {
  char callsign[kMaxCallsign + 1] = {0};
  uint32_t freqKhz = 0;
  int powerDbm = 0;
  int volume = kDefaultVolume;  // volume iniziale dell'altoparlante
  int micFloorDb = kDefaultMicFloorDb;  // fondo del microfono, dBFS

  float freqMhz() const { return static_cast<float>(freqKhz) / 1000.0f; }
};

bool operator==(const Settings& a, const Settings& b);
inline bool operator!=(const Settings& a, const Settings& b) { return !(a == b); }

// true se nominativo, frequenza, potenza e volume sono nei limiti.
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
  MicMeasure,  // risposta pronta; il chiamante deve misurare il fondo per 3 s e stampare il risultato
  AudioTest,   // risposta pronta; il chiamante deve suonare i toni di prova e stampare i livelli del microfono
};

class SettingsConsole {
 public:
  // active: valori in uso (letti all'avvio). board: nome breve per CFG GET.
  // version: versione del firmware (git describe), mostrata da CFG VERSION.
  SettingsConsole(const Settings& active, const Settings& defaults, ISettingsStorage& storage, const char* board,
                  const char* version = "dev");

  // Elabora una riga (senza il fine riga). In reply scrive la risposta, senza fine riga.
  ConsoleResult handleLine(const char* line, char* reply, size_t cap);

  // Valori salvati (quelli che saranno attivi al prossimo avvio).
  const Settings& stored() const { return stored_; }

  // Prova di collegamento: NON viene salvata, all'accensione e' sempre spenta.
  bool testMode() const { return testMode_; }
  // Allinea lo stato quando la prova viene attivata o disattivata da tre clic sul PTT.
  void setTestMode(bool on) { testMode_ = on; }

 private:
  ConsoleResult get(char* reply, size_t cap);
  ConsoleResult versionInfo(char* reply, size_t cap);
  ConsoleResult set(const char* key, const char* value, char* reply, size_t cap);
  ConsoleResult reset(char* reply, size_t cap);
  ConsoleResult mic(const char* value, char* reply, size_t cap);

  ConsoleResult test(const char* value, char* reply, size_t cap);

  bool testMode_ = false;
  Settings active_;
  Settings defaults_;
  Settings stored_;
  ISettingsStorage& storage_;
  const char* board_;
  const char* version_;
};

}  // namespace lcr
