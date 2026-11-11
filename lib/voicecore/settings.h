// Impostazioni dell'operatore modificabili senza ricompilare (nominativo, frequenza, potenza).
//
// I valori vengono salvati nella memoria non volatile della scheda (NVS) e letti all'avvio;
// se mancano o non sono validi si usano quelli compilati in src/user_config.h. Una modifica
// vale dal riavvio successivo: la radio non cambia parametri mentre e' in uso.
//
// La console e' un protocollo a righe sulla seriale, pensato per la pagina web di
// configurazione (Web Serial) ma usabile anche a mano:
//
//   CFG VERSION                  -> CFG VERSION fw=v0.1.0-beta.1 board=t3 proto=2   (versione del firmware e del protocollo)
//   CFG GET                      -> CFG callsign=IU5HJU freq=433.900 freqmsg=433.900 freqqual=433.900 power=10 volume=5 micfloor=-60 profile=std board=t3 reboot=0 test=0 pos=off qth=none gps=off role=off freqout=433.900
//   CFG SET CALLSIGN IU5HJU      -> CFG OK callsign=IU5HJU     (oppure CFG ERR callsign <motivo>)
//   CFG SET FREQ 433.900         -> CFG OK freq=433.900        (MHz, 430.000-440.000; frequenza del profilo Standard)
//   CFG SET FREQMSG 433.900      -> CFG OK freqmsg=433.900     (come FREQ, per il profilo Messaggio; puo' essere uguale a FREQ)
//   CFG SET POWER 10             -> CFG OK power=10            (dBm, 2-17)
//   CFG SET MICFLOOR -60         -> CFG OK micfloor=-60        (fondo del microfono in dBFS, da -80 a -36)
//   CFG MIC MEASURE              -> misura il fondo per 3 s (stare in silenzio), poi: CFG MIC fondo=-61 min=-63 max=-58 frames=75
//   CFG SET VOLUME 7             -> CFG OK volume=7            (0 = muto, 1-10 in passi da 3 dB; 5 = 0 dB, 6-10 = guadagno fino a +15 dB)
//   CFG SET FREQQUAL 433.900     -> CFG OK freqqual=433.900    (come FREQ, per il profilo Qualita')
//   CFG SET ROLE RPTRX           -> CFG OK role=rptrx          (OFF = scheda normale, RPTRX/RPTTX = scheda RX/TX del ripetitore; vale dal riavvio)
//   CFG SET FREQOUT 434.400      -> CFG OK freqout=434.400     (frequenza di uscita del ripetitore, usata dalla scheda TX)
//   CFG SET PROFILE MSG          -> CFG OK profile=msg         (STD = Standard, MSG = Messaggio SF12/BW125, QUA = Qualita' 3200 SF7; vale dal riavvio)
//   CFG SET POS ON               -> CFG OK pos=on             (trasmette la posizione nel pacchetto START; di default OFF; serve il QTH o un GPS)
//   CFG SET QTH 43.7696,11.2558  -> CFG OK qth=43.76960,11.25580   (posizione fissa in gradi decimali, lat,lon; NONE la cancella)
//   CFG SET TEXT1 Arrivo tra 10 minuti -> CFG OK text1=Arrivo tra 10 minuti   (testi predefiniti da inviare, TEXT1..TEXT5; ASCII senza '{' e '}', 1-150
//                                   caratteri, anche con spazi fino a fine riga; NONE svuota il posto; valgono subito, senza riavvio)
//   CFG GET TEXT1                -> CFG TEXT1=Arrivo tra 10 minuti   (vuoto: CFG TEXT1=)
//   CFG SET GPS ON               -> CFG OK gps=on             (accende il GPS della T-Beam; di default OFF; vale dal riavvio; sulle altre schede errore)
//   CFG TEST ON                  -> CFG OK test=on             (prova di collegamento, solo fino al riavvio; OFF la spegne)
//   CFG TEST AUDIO               -> suona 500, 1000 e 2000 Hz e misura il microfono: CFG AUDIO f=500 base=-90.1 tone=-52.3 ... CFG AUDIO fine
//   CFG RESET                    -> CFG OK reset               (torna ai valori compilati)
//   CFG REBOOT                   -> riavvia la scheda
//
// Indipendente dalla scheda e testata sul PC (tests/test_settings.cpp).
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "position.h"
#include "profile.h"
#include "protocol.h"

namespace lcr {

constexpr uint32_t kMinFreqKhz = 430000;  // banda dei 70 cm, IARU Regione 1: 430-440 MHz
constexpr uint32_t kMaxFreqKhz = 440000;
constexpr int kMinPowerDbm = 2;   // limiti dell'SX1278 senza PA_BOOST
constexpr int kMaxPowerDbm = 17;
constexpr int kMinVolume = 0;  // muto
constexpr int kMaxVolume = 10;  // +15 dB (il passo 5 e' 0 dB; vedi AudioBackEnd)
constexpr int kDefaultVolume = 5;  // 0 dB

// Ruolo della scheda: normale (voce) oppure una delle due schede del ripetitore (docs/studio-ripetitore.md).
enum Role : uint8_t { kRoleNormal = 0, kRoleRepeaterRx = 1, kRoleRepeaterTx = 2 };
inline const char* roleCode(uint8_t r) { return r == kRoleRepeaterRx ? "rptrx" : (r == kRoleRepeaterTx ? "rpttx" : "off"); }

// Fino al 10/11/2026 il passo 10 era 0 dB e i passi scendevano di 3 dB; ora il passo 5 e' 0 dB. Un volume salvato con la
// scala vecchia (v) si porta al passo che dava lo stesso livello: v-5, almeno 1 (0 = muto resta muto).
inline int migrateOldVolume(int v) { return v <= 0 ? 0 : (v - 5 < 1 ? 1 : (v - 5 > 5 ? 5 : v - 5)); }

// Fondo del microfono: livello dell'ingresso dell'AGC (dopo il passa-alto) in silenzio. Sopra il fondo + kMicGateMarginDb
// l'AGC lavora (e amplifica); sotto, il guadagno scende e il rumore non viene amplificato.
// Standard -60 dBFS: stanza silenziosa con l'INMP441 (misurato -58...-61). Ognuno lo misura e lo corregge per il suo hardware.
constexpr int kMinMicFloorDb = -80;
constexpr int kMaxMicFloorDb = -36;
constexpr int kDefaultMicFloorDb = -60;
constexpr int kMicGateMarginDb = 6;
inline float micGateFromFloor(int floorDb) { return static_cast<float>(floorDb + kMicGateMarginDb); }

// Testi predefiniti da inviare (messaggi di testo stile SMS, vedi protocol.h kModeText): cinque posti, ognuno vuoto oppure un
// testo ASCII di 1-150 caratteri. All'inizio contengono i testi di esempio qui sotto.
constexpr size_t kTextSlots = 5;
constexpr const char* kDefaultTexts[kTextSlots] = {"QRV, sono in ascolto", "Arrivo tra 10 minuti", "Chiamami appena puoi",
                                                   "Tutto OK", "73 e buona giornata"};

struct Settings {
  Settings() {
    for (size_t i = 0; i < kTextSlots; i++) {
      memset(texts[i], 0, sizeof(texts[i]));
      strncpy(texts[i], kDefaultTexts[i], kMaxTextChars);
    }
  }
  char texts[kTextSlots][kMaxTextChars + 1];  // testi predefiniti; NON contano per "reboot" (valgono subito)
  char callsign[kMaxCallsign + 1] = {0};
  uint32_t freqKhz = 0;     // frequenza del profilo Standard
  uint32_t freqMsgKhz = 0;  // frequenza del profilo Messaggio: puo' essere uguale a quella dello Standard (mai bloccante)
  uint32_t freqQualKhz = 0;  // frequenza del profilo Qualita' (0 = come lo Standard)
  uint8_t role = kRoleNormal;  // ruolo: normale o scheda RX/TX del ripetitore (vale dal riavvio)
  uint32_t freqOutKhz = 0;   // frequenza di uscita del ripetitore, usata dalla scheda TX (0 = come la frequenza del profilo)
  int powerDbm = 0;
  int volume = kDefaultVolume;  // volume iniziale dell'altoparlante
  int micFloorDb = kDefaultMicFloorDb;  // fondo del microfono, dBFS
  uint8_t profile = kProfileStandard;   // profilo di trasmissione (profile.h); vale dal riavvio
  bool gpsOn = false;       // GPS acceso (solo T-Beam); spento di default: consuma decine di mA. Vale dal riavvio
  bool posTx = false;       // trasmette la posizione nel pacchetto START (spento di default: dato personale)
  bool qthSet = false;      // posizione fissa (QTH) impostata
  int32_t qthLatE7 = 0;     // posizione fissa, decimilionesimi di grado
  int32_t qthLonE7 = 0;

  // Posizione fissa dell'operatore (non valida se non impostata). Serve anche per calcolare la distanza dalle stazioni
  // ricevute, indipendentemente da posTx.
  Position qth() const {
    Position p;
    p.valid = qthSet;
    p.fromGps = false;
    p.latE7 = qthLatE7;
    p.lonE7 = qthLonE7;
    return p;
  }

  // Frequenza del profilo in uso (quella con cui lavora la radio).
  uint32_t qualFreqKhz() const { return freqQualKhz ? freqQualKhz : freqKhz; }
  // Frequenza di uscita del ripetitore: quella impostata, altrimenti quella del profilo.
  uint32_t outFreqKhz() const { return freqOutKhz ? freqOutKhz : profileFreqKhz(); }
  // Copia con la frequenza del profilo in uso sostituita da khz (la scheda TX del ripetitore la usa per la radio).
  Settings withProfileFreqKhz(uint32_t khz) const {
    Settings s = *this;
    if (profile == kProfileMessage) s.freqMsgKhz = khz;
    else if (profile == kProfileQuality) s.freqQualKhz = khz;
    else s.freqKhz = khz;
    return s;
  }
  uint32_t profileFreqKhz() const {
    return profile == kProfileMessage ? freqMsgKhz : profile == kProfileQuality ? qualFreqKhz() : freqKhz;
  }
  float freqMhz() const { return static_cast<float>(profileFreqKhz()) / 1000.0f; }
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
  CodecTest,   // risposta pronta; il chiamante deve registrare 3 s e riprodurli senza e con Codec2
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
  // Modo "registra e poi trasmetti" (CFG REC ON/OFF): anche questo non viene salvato.
  bool recordMode() const { return recMode_; }
  // Allinea lo stato quando la prova viene attivata o disattivata da tre clic sul PTT.
  void setTestMode(bool on) { testMode_ = on; }

 private:
  ConsoleResult get(char* reply, size_t cap);
  ConsoleResult versionInfo(char* reply, size_t cap);
  ConsoleResult set(const char* key, const char* value, char* reply, size_t cap);
  ConsoleResult reset(char* reply, size_t cap);
  ConsoleResult mic(const char* value, char* reply, size_t cap);
  ConsoleResult setText(int slot, const char* text, size_t len, char* reply, size_t cap);
  ConsoleResult getText(int slot, char* reply, size_t cap);

  ConsoleResult test(const char* value, char* reply, size_t cap);
  ConsoleResult rec(const char* value, char* reply, size_t cap);

  bool testMode_ = false;
  bool recMode_ = false;
  Settings active_;
  Settings defaults_;
  Settings stored_;
  ISettingsStorage& storage_;
  const char* board_;
  const char* version_;
};

}  // namespace lcr
