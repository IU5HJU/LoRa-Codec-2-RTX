// Impostazioni dell'operatore modificabili senza ricompilare (nominativo, frequenza, potenza).
//
// I valori vengono salvati nella memoria non volatile della scheda (NVS) e letti all'avvio;
// se mancano o non sono validi si usano quelli compilati in src/user_config.h. Una modifica
// vale dal riavvio successivo: la radio non cambia parametri mentre e' in uso.
//
// La console e' un protocollo a righe sulla seriale, pensato per la pagina web di
// configurazione (Web Serial) ma usabile anche a mano:
//
//   CFG VERSION                  -> CFG VERSION fw=v0.1.0-beta.1 board=t3 proto=2   (versione del firmware e del protocollo; sulla T-Beam con SX1262 in coda radio=SX1262)
//   CFG GET                      -> CFG callsign=IU5HJU freq=433.900 freqmsg=433.900 freqrange=433.900 power=10 volume=5 micfloor=-60 profile=std board=t3 reboot=0 test=0 pos=off qth=none gps=off role=off freqout=433.900
//   CFG SET CALLSIGN IU5HJU      -> CFG OK callsign=IU5HJU     (oppure CFG ERR callsign <motivo>)
//   CFG SET FREQ 433.900         -> CFG OK freq=433.900        (MHz, 430.000-440.000; frequenza del profilo Standard, Codec2 3200; le frequenze di Range e Messaggio che erano uguali la seguono)
//   CFG SET FREQMSG 433.900      -> CFG OK freqmsg=433.900     (come FREQ, per il profilo Messaggio; puo' essere uguale a FREQ)
//   CFG SET POWER 10             -> CFG OK power=10            (dBm, 2-17)
//   CFG SET MICFLOOR -60         -> CFG OK micfloor=-60        (fondo del microfono in dBFS, da -80 a -36)
//   CFG MIC MEASURE              -> misura il fondo per 3 s (stare in silenzio), poi: CFG MIC fondo=-61 min=-63 max=-58 frames=75
//   CFG SET VOLUME 7             -> CFG OK volume=7            (0 = muto, 1-10 in passi da 3 dB; 5 = 0 dB, 6-10 = guadagno fino a +15 dB)
//   CFG SET FREQRANGE 433.900    -> CFG OK freqrange=433.900   (come FREQ, per il profilo Range, Codec2 1200; se manca vale come FREQ)
//   CFG SET ROLE RPTRX           -> CFG OK role=rptrx          (OFF = scheda normale, RPTRX/RPTTX = scheda RX/TX del ripetitore; vale dal riavvio)
//   CFG SET FREQOUT 434.400      -> CFG OK freqout=434.400     (frequenza di uscita del ripetitore, usata dalla scheda TX)
//   CFG SET PROFILE MSG          -> CFG OK profile=msg         (STD = Standard 3200 SF7, RNG = Range 1200 SF8, MSG = Messaggio SF12/BW125; QUA vale come STD; vale dal riavvio)
//   CFG SET POS ON               -> CFG OK pos=on             (trasmette la posizione nel pacchetto START; di default OFF; serve il QTH o un GPS)
//   CFG SET QTH 43.7696,11.2558  -> CFG OK qth=43.76960,11.25580   (posizione fissa in gradi decimali, lat,lon; NONE la cancella)
//   CFG SET TEXT1 Arrivo tra 10 minuti -> CFG OK text1=Arrivo tra 10 minuti   (testi predefiniti da inviare, TEXT1..TEXT5; ASCII senza '{' e '}', 1-150
//                                   caratteri, anche con spazi fino a fine riga; NONE svuota il posto; valgono subito, senza riavvio)
//   CFG GET TEXT1                -> CFG TEXT1=Arrivo tra 10 minuti   (vuoto: CFG TEXT1=)
//   CFG SET MONITOR ON           -> CFG OK monitor=on          (solo scheda RX del ripetitore: altoparlante per ascoltare il traffico voce; cambia i fili della seriale RX-TX, vedi docs/firmware.md; vale dal riavvio)
//   CFG GET MONITOR              -> CFG MONITOR=on
//   CFG SET FREQMC 433.175       -> CFG OK freqmc=433.175      (frequenza delle reti di testo, solo per l'escursione; come FREQ)
//   CFG SET MCDEST1 22251        -> CFG OK mcdest1=22251       (MCDEST1..MCDEST5: destinatari tra cui scegliere nell'escursione MeshCom: un gruppo
//                                   numerico (22251, 222...), un nominativo-SSID (N0CALL-9) o * per tutti; NONE svuota il posto; valgono subito)
//   CFG SET SYSOP IU5HJU         -> CFG OK sysop=IU5HJU        (nominativo del sysop: il solo che puo' comandare il ripetitore; NONE lo toglie; vale dal riavvio)
//   CFG SET SYSOPKEY <32 cifre esadecimali> -> CFG OK sysopkey=set  (chiave condivisa di 16 byte per firmare i comandi; NONE la toglie; mai mostrata)
//   CFG SET SYSOPMODE ON         -> CFG OK sysopmode=on        (modo Sysop: nel menu "Comandi Sysop" al posto di "Invia testo"; OFF lo spegne; vale dal riavvio)
//   CFG SET SHIFT -10000..+10000 -> CFG OK shift=+7600       (stazione: si ascolta su FREQ e si trasmette su FREQ + shift, per operare via ripetitore; 0 = simplex;
//                                                          la frequenza spostata deve restare in 430-440 MHz; CFG GET SHIFT; vale dal riavvio)
//   CFG SET IDINT 10             -> CFG OK idint=10            (ripetitore e nodo: ogni quanti minuti, da 1 a 10, la scheda TX manda l'identificativo; di base 10; vale dal riavvio)
//   CFG SET IDMORSE ON           -> CFG OK idmorse=on          (ripetitore e nodo: dopo l'identificativo in testo anche il Morse in FM, per chi non ha le schede LoRa; di base ON; vale dal riavvio)
//   CFG GET IDMORSE              -> CFG IDMORSE=on
//   CFG GET IDINT                -> CFG IDINT=10
//   CFG RADIOTEST MORSE FM N0CALL -> prova radio del Morse (solo prove: MORSE, BEACON, CARRIER, TONE; vedi radio_test.h e docs/studio-identificativo-fm.md)
//   CFG SET ROLE NODE            -> CFG OK role=node           (nodo su Wi-Fi: T3 o T-Beam; i parametri sotto; vale dal riavvio)
//   CFG SET WIFISSID <nome della rete> / CFG SET WIFIPASS <password>   (il resto della riga: possono avere spazi; NONE toglie la password)
//   CFG SET PEER 192.168.1.51:5400  -> CFG OK peer=192.168.1.51:5400  (indirizzo e porta dell'altro nodo; NONE lo toglie)
//   CFG SET NODEPORT 5400 / NODEID 1 / PEERID 2 / NODEKEY <32 cifre esadecimali>   (porta di ascolto, id di questo nodo, id atteso dell'altro (0 = qualunque), chiave condivisa)
//   CFG GET NODE                 -> CFG NODE ssid=casa pass=set ip=192.168.1.51 port=5400 listen=5400 id=1 peerid=2 key=set
//   CFG GET SYSOP                -> CFG SYSOP call=IU5HJU key=set mode=off
//   CFG GET MCDEST1              -> CFG MCDEST1=22251
//   CFG SET MCWINDOW 300         -> CFG OK mcwindow=300        (secondi di ascolto dopo l'invio, 30-1800)
//   CFG SET MCSSID 99            -> CFG OK mcssid=99           (SSID del tuo nominativo in rete, 0-99; 0 = nessuno)
//   CFG GET MC                   -> CFG MC freqmc=433.175 ssid=99 window=300
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
#include <cstdio>
#include <cstring>

#include "position.h"
#include "profile.h"
#include "radio_test.h"
#include "protocol.h"

namespace lcr {

constexpr uint32_t kMinFreqKhz = 430000;  // banda dei 70 cm, IARU Regione 1: 430-440 MHz
constexpr uint32_t kMaxFreqKhz = 440000;
// Shift per operare via ripetitore (solo stazione normale): la stazione ascolta su FREQ (l'uscita del ripetitore) e trasmette su FREQ + shift
// (l'ingresso). Valori ammessi: 0 (di base: simplex), +10000 e -10000 kHz (+-10 MHz). La frequenza spostata deve restare in 430-440 MHz.
constexpr int32_t kMaxShiftKhz = 10000;  // lo shift e' libero da -10000 a +10000 kHz (da 0 a 10 MHz, in avanti o indietro), a passi di 1 kHz
constexpr size_t kMaxPeerHost = 63;  // lunghezza massima del nome o indirizzo dell'altro nodo / del reflector
constexpr int kMinIdIntervalMin = 1;
constexpr int kMaxIdIntervalMin = 10;
constexpr int kMinPowerDbm = 2;   // limiti dell'SX1278 senza PA_BOOST
constexpr int kMaxPowerDbm = 17;
constexpr int kMinVolume = 0;  // muto
constexpr int kMaxVolume = 10;  // +15 dB (il passo 5 e' 0 dB; vedi AudioBackEnd)
constexpr int kDefaultVolume = 5;  // 0 dB

// Ruolo della scheda: normale (voce) oppure una delle due schede del ripetitore (docs/studio-ripetitore.md).
enum Role : uint8_t { kRoleNormal = 0, kRoleRepeaterRx = 1, kRoleRepeaterTx = 2, kRoleNode = 3 };
inline const char* roleCode(uint8_t r) { return r == kRoleRepeaterRx ? "rptrx" : (r == kRoleRepeaterTx ? "rpttx" : (r == kRoleNode ? "node" : "off")); }

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
// testo ASCII di 1-150 caratteri. All'inizio contengono i testi di esempio qui sotto. Nel testo valgono i segnaposti di
// text_template.h (%C nominativo, %M modo, %F frequenza), sostituiti all'invio; il primo e' "%C QRV su VoLoRa modo %M %F".
constexpr size_t kTextSlots = 5;
constexpr const char* kDefaultTexts[kTextSlots] = {"%C QRV su VoLoRa modo %M %F", "Arrivo tra 10 minuti", "Chiamami appena puoi",
                                                   "Tutto OK", "73 e buona giornata"};

constexpr size_t kMcDestSlots = 5;
constexpr const char* kDefaultMcDests[kMcDestSlots] = {"22251", "222", "*", "", ""};
constexpr uint16_t kMinMcWindowS = 30, kMaxMcWindowS = 1800;

struct Settings {
  Settings() {
    for (size_t i = 0; i < kMcDestSlots; i++) {
      memset(mcDests[i], 0, sizeof(mcDests[i]));
      strncpy(mcDests[i], kDefaultMcDests[i], sizeof(mcDests[i]) - 1);
    }
    for (size_t i = 0; i < kTextSlots; i++) {
      memset(texts[i], 0, sizeof(texts[i]));
      strncpy(texts[i], kDefaultTexts[i], kMaxTextChars);
    }
  }
  char texts[kTextSlots][kMaxTextChars + 1];  // testi predefiniti; NON contano per "reboot" (valgono subito)
  char callsign[kMaxCallsign + 1] = {0};
  uint32_t freqKhz = 0;     // frequenza del profilo Standard (Codec2 3200)
  uint32_t freqMsgKhz = 0;  // frequenza del profilo Messaggio: puo' essere uguale a quella dello Standard (mai bloccante)
  // Escursione sulle reti di testo (MeshCom, docs/meshcom-testo.md): frequenza, SSID, durata dell'ascolto e cinque destinatari tra
  // cui scegliere da menu (un gruppo numerico, un nominativo-SSID o "*" per tutti); un posto vuoto non si usa.
  uint32_t freqMcKhz = 433175;
  uint8_t mcSsid = 99;          // SSID del nominativo in rete (0 = nessuno), per esempio IU5HJU-99
  uint16_t mcWindowS = 300;     // secondi di ascolto dopo l'invio (e prima di tornare al profilo voce)
  char mcDests[kMcDestSlots][16];
  int32_t shiftKhz = 0;       // shift in trasmissione, in kHz, da -10000 a +10000 (vale dal riavvio; ignorato dai ruoli ripetitore e nodo)
  uint32_t freqRangeKhz = 0;  // frequenza del profilo Range (Codec2 1200; 0 = come lo Standard)
  uint8_t role = kRoleNormal;  // ruolo: normale o scheda RX/TX del ripetitore (vale dal riavvio)
  // Comandi del sysop al ripetitore (sysop.h): nominativo del sysop e chiave condivisa di 16 byte (mai mostrata da CFG GET); vale dal riavvio.
  char sysopCall[kMaxCallsign + 1] = {0};
  uint8_t sysopKey[16] = {0};
  bool sysopKeySet = false;
  // Modo Sysop (scheda del sysop): il menu "Invia testo" diventa "Comandi Sysop" (elenco dei comandi al posto dei testi). Resta attivo
  // finche' il sysop non lo spegne; vale dal riavvio.
  bool sysopMode = false;
  bool idMorse = true;         // dopo l'identificativo in testo, il nominativo in Morse con tono in FM stretta (docs/studio-identificativo-fm.md); vale dal riavvio
  uint8_t idIntervalMin = 10;  // identificativo periodico del ripetitore (scheda TX), in minuti: 1..10, di base 10; vale dal riavvio
  // Nodo su Wi-Fi (ruolo NODE, docs/studio-nodo-internet.md): rete, indirizzo dell'altro nodo, id e chiave condivisa; valgono dal riavvio.
  char wifiSsid[33] = {0};
  char wifiPass[65] = {0};
  char peerIp[kMaxPeerHost + 1] = {0};  // l'altro nodo o il reflector: indirizzo IPv4 oppure nome (reflector.example.org), "" = non impostato
  uint16_t peerPort = 5400;
  uint16_t nodePort = 5400;    // porta UDP su cui si ascolta
  uint32_t nodeId = 0;         // 0 = non impostato
  uint32_t peerId = 0;         // 0 = qualunque nodo con la chiave giusta
  uint8_t nodeKey[16] = {0};
  bool nodeKeySet = false;
  bool rxMonitor = false;    // scheda RX del ripetitore con altoparlante: ascolta la voce che riceve (vale dal riavvio)
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
  uint32_t rangeFreqKhz() const { return freqRangeKhz ? freqRangeKhz : freqKhz; }
  // Frequenza di uscita del ripetitore: quella impostata, altrimenti quella del profilo.
  uint32_t outFreqKhz() const { return freqOutKhz ? freqOutKhz : profileFreqKhz(); }
  // Copia con la frequenza del profilo in uso sostituita da khz (la scheda TX del ripetitore la usa per la radio).
  Settings withProfileFreqKhz(uint32_t khz) const {
    Settings s = *this;
    if (profile == kProfileMessage) s.freqMsgKhz = khz;
    else if (profile == kProfileRange) s.freqRangeKhz = khz;
    else s.freqKhz = khz;
    return s;
  }
  uint32_t profileFreqKhz() const {
    return profile == kProfileMessage ? freqMsgKhz : profile == kProfileRange ? rangeFreqKhz() : freqKhz;
  }
  // Nominativo-SSID usato nell'escursione MeshCom ("IU5HJU-99"); senza SSID se mcSsid e' 0. cap almeno 16.
  void mcCallsign(char* out, size_t cap) const {
    if (cap == 0) return;
    if (mcSsid == 0) snprintf(out, cap, "%s", callsign);
    else snprintf(out, cap, "%s-%u", callsign, static_cast<unsigned>(mcSsid));
  }
  // Frequenza di trasmissione della stazione: quella del profilo in uso piu' lo shift (con shift 0 e' la stessa di ascolto).
  uint32_t txFreqKhz() const { return static_cast<uint32_t>(static_cast<int64_t>(profileFreqKhz()) + shiftKhz); }
  float freqMhz() const { return static_cast<float>(profileFreqKhz()) / 1000.0f; }
};

bool operator==(const Settings& a, const Settings& b);
inline bool operator!=(const Settings& a, const Settings& b) { return !(a == b); }

// Destinatario valido nell'escursione MeshCom: "*" oppure 1-15 caratteri A-Z, 0-9, '/' e '-' (nominativo-SSID o gruppo). Vuoto: false.
bool isValidMcDest(const char* d);
// "a.b.c.d" con quattro numeri da 0 a 255 (indirizzo dell'altro nodo).
bool isValidIpv4(const char* s);
// Nome di un computer in rete: lettere, cifre, "-" e "." (etichette non vuote, che non iniziano ne' finiscono con "-"), da 1 a kMaxPeerHost caratteri,
// con almeno una lettera (cosi' "300.1.1.1" non passa per un nome). Un indirizzo IPv4 valido e' valido anche per isValidPeerHost.
bool isValidHostName(const char* s);
bool isValidPeerHost(const char* s);

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
// Esito della lettura: Loaded = impostazioni salvate usate; Empty = niente di salvato (o memoria non leggibile); Invalid = qualcosa e'
// salvato ma non e' valido, quindi si usano i valori predefiniti (la memoria NON viene cancellata).
enum class LoadStatus : uint8_t { Loaded, Empty, Invalid };
Settings loadSettings(ISettingsStorage& storage, const Settings& defaults, LoadStatus* status = nullptr);

enum class ConsoleResult {
  NotCommand,  // la riga non e' per la console (nessuna risposta)
  Ok,
  Error,
  Reboot,  // risposta pronta; il chiamante deve riavviare la scheda
  MicMeasure,  // risposta pronta; il chiamante deve misurare il fondo per 3 s e stampare il risultato
  CodecTest,   // risposta pronta; il chiamante deve registrare 3 s e riprodurli senza e con Codec2
  RadioTest,   // risposta pronta; il chiamante deve fare la prova radio del Morse (radioTestRequest(), docs/studio-identificativo-fm.md)
  AudioTest,   // risposta pronta; il chiamante deve suonare i toni di prova e stampare i livelli del microfono
};

class SettingsConsole {
 public:
  // active: valori in uso (letti all'avvio). board: nome breve per CFG GET.
  // version: versione del firmware (git describe), mostrata da CFG VERSION.
  SettingsConsole(const Settings& active, const Settings& defaults, ISettingsStorage& storage, const char* board,
                  const char* version = "dev", const char* radio = "");

  // Elabora una riga (senza il fine riga). In reply scrive la risposta, senza fine riga.
  ConsoleResult handleLine(const char* line, char* reply, size_t cap);

  // Valori salvati (quelli che saranno attivi al prossimo avvio).
  const Settings& stored() const { return stored_; }

  // Prova di collegamento: NON viene salvata, all'accensione e' sempre spenta.
  bool testMode() const { return testMode_; }
  // Modo "registra e poi trasmetti" (CFG REC ON/OFF): anche questo non viene salvato.
  bool recordMode() const { return recMode_; }
  // Ultima richiesta di CFG RADIOTEST (valida dopo ConsoleResult::RadioTest).
  const RadioTestRequest& radioTestRequest() const { return radioTest_; }
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
  RadioTestRequest radioTest_;
  Settings active_;
  Settings defaults_;
  Settings stored_;
  ISettingsStorage& storage_;
  const char* board_;
  const char* radio_;  // chip radio ("SX1278", "SX1262"), vuoto = non indicato
  const char* version_;
};

}  // namespace lcr
