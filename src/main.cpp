// VoLoRa: messaggi vocali broadcast a turni su LoRa, con Codec2.
// Schede: LILYGO LoRa32 T3 V1.6.1 (predefinita) e T-Beam V1.2 (-DBOARD_TBEAM_V12).
// Vedi README.md e docs/.
//
// ATTENZIONE: collegare l'antenna a 433 MHz prima di alimentare la scheda.
#include <Arduino.h>
#include <Wire.h>
#include <esp_system.h>
#include <freertos/task.h>

#include "audio_io.h"
#include "battery.h"
#include "board_pins.h"
#include "button_gesture.h"
#include "codec2_adapter.h"
#include "gps.h"
#include "menu.h"
#include "choice_picker.h"
#include "text_picker.h"
#include "text_template.h"
#include "packet.h"
#include "pmu.h"
#include "ptt_button.h"
#include "radio_lora.h"
#include "repeater_node.h"
#include "radio_test.h"
#include "settings.h"
#include "sysop.h"
#include "settings_nvs.h"
#include "ui.h"
#include "user_config.h"
#include "voice_node.h"
#include <WiFi.h>

#include "wifi_config.h"

// Codec2 e il resto dell'applicazione girano nel task principale. Lo stack
// predefinito (8 kB) non basta: "stack overflow in task loopTask" e riavvio
// continuo. 32 kB come nel progetto di riferimento per Codec2.
SET_LOOP_TASK_STACK_SIZE(32 * 1024);

#if defined(BOARD_TBEAM_V12)
#if defined(RADIO_SX1262)
static const char* const kBoardName = "T-Beam V1.2 SX1262";  // stessa scheda col modulo LoRa SX1262: il nome breve resta "tbeam" (GPS, niente ripetitore)
#else
static const char* const kBoardName = "T-Beam V1.2";
#endif
static const char* const kBoardShort = "tbeam";
#else
static const char* const kBoardName = "LoRa32 T3 V1.6.1";
static const char* const kBoardShort = "t3";
#endif

static Pmu pmu;
static Gps gps;
static Battery battery;
static RadioLora radio;
static Codec2Adapter codec;
static AudioIo audio;
static UiNotifier ui(audio);
static lcr::VoiceNode* node = nullptr;
static NvsSettingsStorage settingsStorage;
static lcr::Settings settings;                // in uso: NVS se valida, altrimenti user_config.h
static WifiConfig wifiConfig;  // configurazione dalla pagina interna (voce "Wi-Fi" del menu)
static void runWifiConfigMode();
static lcr::SettingsConsole* console = nullptr;  // comandi CFG sulla seriale
static lcr::PttButton ptt(LCR_PTT_DEBOUNCE_MS);
static lcr::ButtonGesture volumeButton(30, 2000);  // volume: breve = alza, lunga = menu delle modalita'
static lcr::Menu* menu = nullptr;
static lcr::MenuFlow* menuFlow = nullptr;
static lcr::TextPicker textPicker;  // voce "Invia testo" del menu
// Voce "MeshCom" del menu (escursione sulla rete di testo): prima si sceglie il testo (o il solo ascolto), poi il destinatario.
enum class McStage { None, Text, Dest, Sysop };
static lcr::ChoicePicker mcPicker;
static McStage mcStage = McStage::None;
static const char* mcItems[lcr::ChoicePicker::kMaxItems];
static int mcSlots[lcr::ChoicePicker::kMaxItems];  // posto dell'elemento scelto (testo 0-4 o destinatario 0-4), -1 = solo ascolto
static char mcText[lcr::kMaxTextChars + 1];        // testo scelto, con i segnaposti gia' sostituiti
static bool volumeArmed = false;  // come il PTT: un volume gia' premuto all'accensione non apre il menu

static bool pttUsable = true;   // false se il pin PTT risulta instabile all'avvio
static bool pttArmed = false;   // il PTT vale solo dopo essere stato rilasciato almeno una volta

static uint16_t newSessionId() { return static_cast<uint16_t>(esp_random()); }
static void meshLog(const char* m) { Serial.printf("[meshcom] %s\n", m); }  // una riga per ogni frame ricevuto durante l'escursione
static uint32_t newMsgId() { return esp_random(); }  // ID a 32 bit di un testo MeshCom

static void fatal(const char* what, int code = 0) {
  Serial.printf("ERRORE: %s (%d)\n", what, code);
  ui.showMessage("ERRORE", what);
  for (;;) delay(1000);
}

// Pin PTT: se la scheda non ha un pull-up interno sul pin (GPIO34-39, come il tasto IO38 della
// T-Beam) si conta su quello esterno.
static int pttPinMode() {
  if (!pins::kPttHasInternalPullup) return INPUT;
  return LCR_PTT_ACTIVE_LOW ? INPUT_PULLUP : INPUT_PULLDOWN;
}

static bool pttRawPressed() { return (digitalRead(pins::kPtt) == LOW) == (LCR_PTT_ACTIVE_LOW != 0); }

// Un pin PTT che cambia livello da solo (flottante, per esempio senza pull-up) potrebbe far
// partire trasmissioni a caso: in quel caso il PTT viene disattivato.
static bool pttPinUnstable() {
  int pressed = 0;
  for (int i = 0; i < 32; i++) {
    if (pttRawPressed()) pressed++;
    delayMicroseconds(500);
  }
  return pressed > 0 && pressed < 32;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("[avvio] scheda %s, firmware %s\n", kBoardName, LCR_VERSION);

  // Impostazioni dell'operatore: salvate nella scheda, altrimenti quelle di user_config.h.
  const lcr::Settings defaults = compileTimeSettings();
  settings = lcr::loadSettings(settingsStorage, defaults);
  console = new lcr::SettingsConsole(settings, defaults, settingsStorage, kBoardShort, LCR_VERSION, RadioLora::chipName());
  ui.setSettings(settings);
  ui.setProfile(lcr::profileFor(settings.profile));
  ui.setOwnPosition(settings.qth());
  {
    size_t n = 0;
    const lcr::MenuItem* items = lcr::defaultMenuItems(n, settings.sysopMode);
    menu = new lcr::Menu(items, n);
    menuFlow = new lcr::MenuFlow(*menu);
    ui.setMenu(menu, settings.profile);
  }

  // L'alimentazione va per prima: sulla T-Beam senza il PMU la radio non e' alimentata.
  const bool pmuOk = pmu.begin();
  battery.begin(pmu);
  gps.begin(settings.gpsOn, pmu);  // solo T-Beam e solo se acceso con CFG SET GPS ON

  pinMode(pins::kPtt, pttPinMode());
  pinMode(pins::kVolume, INPUT_PULLUP);
  pttUsable = !pttPinUnstable();

  if (!ui.begin()) Serial.println("OLED non trovato: si continua senza display");
  {  // dispositivi sul bus I2C (IO21 SDA, IO22 SCL): sulla T-Beam 0x34 e' l'AXP2101, il display sta a 0x3C o 0x3D
    Serial.print("[avvio] dispositivi I2C trovati:");
    int found = 0;
    for (uint8_t a = 1; a < 127; a++) {
      Wire.beginTransmission(a);
      if (Wire.endTransmission() == 0) {
        Serial.printf(" 0x%02X", a);
        found++;
      }
    }
    Serial.println(found ? "" : " nessuno");
  }
  ui.showMessage("AVVIO");
  if (!pmuOk) fatal("alimentazione PMU");
  if (WifiConfig::bootRequest()) {  // richiesto dal menu: si cancella subito l'indicatore (niente ciclo di riavvii) e si configura
    WifiConfig::setBootRequest(false);
    runWifiConfigMode();
  }
  if (settings.role != lcr::kRoleNormal) {  // scheda RX o TX del ripetitore: non ritorna
    // Scheda RX con l'altoparlante di monitor (CFG SET MONITOR ON): solo uscita audio, per ascoltare la voce che riceve.
    AudioIo* monAudio = nullptr;
    lcr::ICodec* monCodec = nullptr;
    if (settings.role == lcr::kRoleRepeaterRx && settings.rxMonitor) {
      ui.showMessage("MONITOR", "altoparlante");
      if (codec.begin(lcr::profileFor(settings.profile).codecMode) && audio.begin(true)) {
        audio.setVolumeStep(settings.volume);
        monAudio = &audio;
        monCodec = &codec;
      } else {
        Serial.println("[ripetitore] ERRORE: audio o Codec2 del monitor non partiti: si prosegue senza altoparlante");
      }
    }
    runRepeaterRole(settings, ui, [](const char* line, char* reply, size_t cap) {
      return console->handleLine(line, reply, cap) == lcr::ConsoleResult::Reboot;
    }, monAudio, monCodec);
  }
  if (!pttUsable) {
    Serial.printf("ATTENZIONE: il pin PTT (GPIO%d) e' instabile (manca il pull-up?): PTT disattivato\n", pins::kPtt);
    ui.showMessage("PTT?", "pin instabile");
    delay(2500);
  }

  if (!lcr::isValidCallsign(settings.callsign)) fatal("nominativo non valido");
  if (lcr::isPlaceholderCallsign(settings.callsign)) {
    Serial.println("Nominativo N0CALL: trasmissione disattivata, solo ascolto");
    ui.showMessage("SOLO RX", "Imposta il nominativo");
    delay(2500);
  }
  // Ogni fase e' annunciata su OLED e seriale: se la scheda si ferma, l'ultimo
  // messaggio dice dove (utile per la diagnosi, vedi docs/prove.md).
  Serial.printf("[avvio] heap libero %u byte\n", static_cast<unsigned>(ESP.getFreeHeap()));
  ui.showMessage("CODEC2");
  Serial.println("[avvio] Codec2...");
  if (!codec.begin(lcr::profileFor(settings.profile).codecMode)) fatal("Codec2");
  Serial.printf("[avvio] Codec2 ok, heap libero %u byte\n", static_cast<unsigned>(ESP.getFreeHeap()));

  ui.showMessage("AUDIO");
  Serial.println("[avvio] audio I2S...");
  if (!audio.begin()) fatal("audio I2S");
  audio.setVolumeStep(settings.volume);
  audio.setMicFloor(settings.micFloorDb);
  Serial.printf("[avvio] audio ok, heap libero %u byte\n", static_cast<unsigned>(ESP.getFreeHeap()));

  ui.showMessage("RADIO");
  Serial.printf("[avvio] radio %s...\n", RadioLora::chipName());
  if (!radio.begin(settings)) fatal("radio", radio.lastError());
  Serial.println("[avvio] radio ok");

  lcr::NodeConfig cfg;
  strncpy(cfg.callsign, settings.callsign, lcr::kMaxCallsign);
  cfg.fec = LCR_FEC != 0;
  cfg.newSessionId = newSessionId;
  // Escursione MeshCom (solo testo): nominativo-SSID, identificatore hardware del frame (T-Beam con AXP2101 = 12,
  // LoRa32 T3 = 3: tabella delle schede della rete) e ID casuale a 32 bit per ogni testo
  settings.mcCallsign(cfg.mcCall, sizeof(cfg.mcCall));
  cfg.mcHw = strcmp(kBoardShort, "tbeam") == 0 ? 12 : 3;  // anche per la variante SX1262: stessa scheda (identificativo da verificare con la tabella della rete)
  cfg.newMsgId = newMsgId;
  cfg.meshLog = meshLog;
  if (settings.sysopKeySet) cfg.sysopKey = settings.sysopKey;  // stazione del sysop: verifica le risposte dei ripetitori
  cfg.profile = settings.profile;
  cfg.sendPosition = settings.posTx;  // spenta di default; la posizione fissa e' il QTH (CFG SET QTH)
  cfg.position = settings.qth();
  cfg.sweepMaxDbm = lcr::kMaxPowerDbm;
  cfg.sweepMinDbm = lcr::kMinPowerDbm;
  cfg.sweepRestoreDbm = settings.powerDbm;
  node = new lcr::VoiceNode(cfg, radio, codec, audio.mic(), audio.speaker(), ui);
  ui.setInbox(&node->inbox());  // il display legge da qui il testo ricevuto
  ui.setPicker(&textPicker);
  ui.setChoicePicker(&mcPicker);

  Serial.printf("[avvio] stack libero minimo del task principale: %u byte\n",
                static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
  const lcr::Profile& prof = lcr::profileFor(settings.profile);
  Serial.printf("%s pronto, profilo %s, %.3f MHz, %d dBm, SF%d BW%.0f\n", settings.callsign, prof.name,
                static_cast<double>(settings.freqMhz()), settings.powerDbm, prof.sf, static_cast<double>(prof.bwKhz));
  if (settings.shiftKhz != 0) {
    Serial.printf("Via ripetitore: ascolto su %.3f MHz, trasmetto su %.3f MHz (shift %+ld kHz)\n", static_cast<double>(settings.freqMhz()),
                  static_cast<double>(settings.txFreqKhz()) / 1000.0, static_cast<long>(settings.shiftKhz));
  }
  char reply[128];
  console->handleLine("CFG GET", reply, sizeof(reply));
  Serial.println(reply);  // la pagina di configurazione legge questa riga
}

// Trasmissione radio continua durante la seconda fase della prova audio: un task invia pacchetti di controllo
// (modo 15, ignorati dai ricevitori) uno dopo l'altro mentre il loop principale suona i toni.
static volatile bool rfBurstRun = false;
static volatile bool rfBurstDone = true;
static volatile uint32_t rfBurstPackets = 0;

static void rfBurstTask(void*) {
  lcr::Packet p;
  p.mode = lcr::kModeRfCheck;
  p.session = static_cast<uint16_t>(esp_random());
  strncpy(p.callsign, settings.callsign, lcr::kMaxCallsign);
  p.payloadLen = lcr::kMaxDataPayload;
  for (size_t k = 0; k < lcr::kMaxDataPayload; k++) p.payload[k] = static_cast<uint8_t>(0x55 ^ (k * 37));
  uint8_t seq = 0;
  while (rfBurstRun) {
    p.seq = seq++;
    uint8_t buf[lcr::kMaxWireBytes];
    const size_t n = lcr::encodePacket(p, buf, sizeof(buf));
    if (n) radio.send(buf, n);  // bloccante: la radio resta in trasmissione quasi di continuo
    rfBurstPackets = rfBurstPackets + 1;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  rfBurstDone = true;
  vTaskDelete(nullptr);
}

// Una fase della prova: tre toni a volume basso e fisso, con il livello del microfono a ogni frequenza e i massimi di
// livello e offset visti dal microfono nell'intera fase. Con rf=true le righe hanno il prefisso "rf".
static void runTonePhase(bool rf) {
  static const float kFreqs[] = {500.0f, 1000.0f, 2000.0f};
  audio.rfWatchBegin();
  for (float f : kFreqs) {
    float pauseDb = 0, toneDb = 0;
    audio.toneTest(f, pauseDb, toneDb);
    Serial.printf("CFG AUDIO %sf=%d base=%.1f tone=%.1f\n", rf ? "rf " : "", static_cast<int>(f),
                  static_cast<double>(pauseDb), static_cast<double>(toneDb));
    delay(150);
  }
  const AudioIo::RfWatch w = audio.rfWatchEnd();
  Serial.printf("CFG AUDIO m%d rms=%.1f agc=%.1f dc=%.1f\n", rf ? 2 : 1, static_cast<double>(w.rmsDb),
                static_cast<double>(w.agcInDb), static_cast<double>(w.dcPct));
}

// Prova dell'hardware audio (CFG TEST AUDIO), in due fasi: 1) tre toni senza radio; 2) gli stessi tre toni mentre la
// scheda trasmette in continuo. Il confronto dei livelli (e l'ascolto) dice se la radiofrequenza rientra nel microfono
// o nell'amplificatore. La fase 2 richiede un nominativo vero e l'antenna collegata, come ogni trasmissione.
// I risultati tornano alla pagina di configurazione; il giudizio e' dell'operatore.
static void runAudioTest() {
  constexpr int kTestVolumeStep = 1;  // -12 dB: udibile ma non forte (prima della scala nuova era il passo 5, -15 dB)
  const int savedVolume = audio.volumeStep();
  audio.setVolumeStep(kTestVolumeStep);

  ui.showMessage("PROVA TONI", "fase 1: senza radio");
  runTonePhase(false);

  if (lcr::isPlaceholderCallsign(settings.callsign)) {
    Serial.println("CFG AUDIO rf skip=nominativo");
  } else if (node->state() != lcr::VoiceNode::State::Idle) {
    Serial.println("CFG AUDIO rf skip=occupata");
  } else {
    ui.showMessage("PROVA TONI", "fase 2: con radio TX");
    delay(500);
    rfBurstPackets = 0;
    rfBurstRun = true;
    rfBurstDone = false;
    // Sul core 0 (il loop e la riproduzione dei toni girano sul core 1) e con priorita' inferiore al microfono: la
    // trasmissione non puo' togliere tempo ai toni.
    xTaskCreatePinnedToCore(rfBurstTask, "rfburst", 4096, nullptr, 2, nullptr, 0);
    delay(400);  // la trasmissione e' gia' in corso quando partono i toni
    runTonePhase(true);
    rfBurstRun = false;
    for (int i = 0; i < 100 && !rfBurstDone; i++) delay(20);
    radio.startReceive();
    Serial.printf("CFG AUDIO rf pkts=%u\n", static_cast<unsigned>(rfBurstPackets));
  }

  audio.setVolumeStep(savedVolume);
  Serial.println("CFG AUDIO fine");
  ui.refresh();
}

// Prova del Codec2 sulla scheda, con una scheda sola e senza radio (CFG TEST CODEC): 3 secondi dal microfono, poi
// quattro ascolti dalla stessa registrazione (i primi due sotto; il 3 e il 4 a ritmo di ricezione, con e senza display): 1) l'audio del microfono cosi' com'e', 2) lo stesso audio dopo codifica e
// decodifica Codec2. Entrambi passano dallo stesso percorso di riproduzione della ricezione. Confronto a orecchio e nei
// livelli: se il 1 e' buono e il 2 no, il guasto e' nel Codec2 sulla scheda. I frame codificati vengono stampati in
// esadecimale, per decodificarli sul PC con il Codec2 di riferimento.
// Frame in esadecimale, righe da 60 byte, con un prefisso: serve a confrontare cio' che si trasmette con cio' che si
// riceve (stesso numero di frame e stessi byte se la radio e il protocollo non alterano nulla).
static void printFrameHex(const char* tag, const uint8_t* data, size_t frames) {
  Serial.printf("%s %u frame\n", tag, static_cast<unsigned>(frames));
  // I registri del codec hanno un passo fisso di kMaxFrameBytes; si stampano solo i byte di ogni frame (una riga per frame
  // a 3200, righe da 60 byte a 1200).
  const size_t fb = codec.frameBytes();
  for (size_t f = 0; f < frames; f++) {
    if (f % (60 / fb) == 0) {
      if (f) Serial.println();
      Serial.print(tag);
      Serial.print(' ');
    }
    for (size_t k = 0; k < fb; k++) Serial.printf("%02x", data[f * lcr::kMaxFrameBytes + k]);
  }
  if (frames) Serial.println();
}

static void runCodecTest() {
  constexpr size_t kFrames = 75;  // 3 secondi
  if (node->state() != lcr::VoiceNode::State::Idle) {
    Serial.println("CFG CODEC ERR occupata");
    return;
  }
  // Un blocco da 640 byte per frame: la memoria libera e' frammentata e un unico buffer da 48 KB non si trova.
  static int16_t* raw[kFrames];
  static uint8_t frames[kFrames * lcr::kMaxFrameBytes];
  const size_t fb = codec.frameBytes();
  size_t allocated = 0;
  for (; allocated < kFrames; allocated++) {
    raw[allocated] = static_cast<int16_t*>(malloc(lcr::kFrameSamples * sizeof(int16_t)));
    if (!raw[allocated]) break;
  }
  auto freeAll = [&]() {
    for (size_t i = 0; i < allocated; i++) free(raw[i]);
    allocated = 0;
  };
  if (allocated < kFrames) {
    Serial.printf("CFG CODEC ERR memoria: %u frame su %u, blocco libero piu' grande %u byte\n",
                  static_cast<unsigned>(allocated), static_cast<unsigned>(kFrames),
                  static_cast<unsigned>(ESP.getMaxAllocHeap()));
    freeAll();
    return;
  }

  ui.showMessage("PARLA", "dopo il bip: 3 s");
  audio.beep(1000, 120);
  delay(350);  // il microfono tiene gli ultimi 200 ms prima dello start: cosi' non registra il bip
  audio.mic().start();
  size_t got = 0;
  const uint32_t t0 = millis();
  while (got < kFrames && millis() - t0 < 6000) {
    if (!audio.mic().readFrame(raw[got])) {
      delay(3);
      continue;
    }
    got++;
  }
  audio.mic().stop();
  if (got < kFrames) {
    freeAll();
    Serial.printf("CFG CODEC ERR microfono: registrati solo %u frame\n", static_cast<unsigned>(got));
    return;
  }

  const uint32_t e0 = millis();
  for (size_t i = 0; i < kFrames; i++) codec.encode(raw[i], frames + i * fb);
  const uint32_t encMs = millis() - e0;
  for (size_t i = 0; i < kFrames * fb; i += 60) {
    Serial.print("CFG CODEC hex ");
    for (size_t k = i; k < i + 60 && k < kFrames * fb; k++) Serial.printf("%02x", frames[k]);
    Serial.println();
  }

  double sqRaw = 0, sqDec = 0;
  int pkRaw = 0, pkDec = 0;
  auto accumulate = [](const int16_t* p, double& sq, int& pk) {
    for (size_t i = 0; i < lcr::kFrameSamples; i++) {
      sq += static_cast<double>(p[i]) * p[i];
      const int a = p[i] < 0 ? -p[i] : p[i];
      if (a > pk) pk = a;
    }
  };

  const int savedVolume = audio.volumeStep();
  audio.setVolumeStep(lcr::AudioBackEnd::kUnityStep);  // 0 dB
  ui.showMessage("ASCOLTO 1", "senza codec");
  delay(500);
  audio.resetBackEnd();
  for (size_t i = 0; i < kFrames; i++) {
    accumulate(raw[i], sqRaw, pkRaw);
    audio.speaker().playFrame(raw[i]);
  }
  delay(900);

  ui.showMessage("ASCOLTO 2", "con Codec2");
  delay(500);
  audio.resetBackEnd();
  int16_t dec[lcr::kFrameSamples];  // un frame alla volta: la decodifica avviene durante l'ascolto
  uint32_t decUs = 0;
  for (size_t i = 0; i < kFrames; i++) {
    const uint32_t d0 = micros();
    codec.decode(frames + i * fb, dec);
    decUs += micros() - d0;
    accumulate(dec, sqDec, pkDec);
    audio.speaker().playFrame(dec);
  }
  delay(900);

  // Ascolti 3 e 4: gli stessi frame decodificati, ma a ritmo di ricezione (un frame ogni 40 ms, come fa il loop), il
  // 3 con il display che si ridisegna come in ricezione, il 4 senza. Se il 2 e' buono e uno di questi no, il guasto
  // e' nelle condizioni della ricezione (ritmo di scrittura, display) e non nel codec o nell'altoparlante.
  for (int run = 3; run <= 4; run++) {
    const bool withUi = run == 3;
    ui.showMessage(withUi ? "ASCOLTO 3" : "ASCOLTO 4", withUi ? "ritmo RX + display" : "ritmo RX, no display");
    delay(500);
    audio.resetBackEnd();
    uint32_t next = millis();
    for (size_t i = 0; i < kFrames; i++) {
      while (static_cast<int32_t>(millis() - next) < 0) {
        if (withUi) {
          ui.loop(millis(), lcr::VoiceNode::State::Receiving, audio.volumeStep(), static_cast<int>(radio.lastRssi()),
                  radio.lastSnr(), battery.volts());
        }
        delay(1);
      }
      codec.decode(frames + i * fb, dec);
      audio.speaker().playFrame(dec);
      next += lcr::kFrameMs;
    }
    delay(900);
  }
  audio.setVolumeStep(savedVolume);

  const double n = static_cast<double>(kFrames * lcr::kFrameSamples) * 32768.0 * 32768.0;
  Serial.printf("CFG CODEC raw=%.1f rawpeak=%d dec=%.1f decpeak=%d encms=%u decms=%u frames=%u\n",
                10.0 * log10(sqRaw / n + 1e-12), pkRaw, 10.0 * log10(sqDec / n + 1e-12), pkDec,
                static_cast<unsigned>(encMs), static_cast<unsigned>(decUs / 1000), static_cast<unsigned>(kFrames));
  freeAll();
  codec.logReset();  // i frame della prova non devono finire nel confronto [tx-hex] / [rx-hex]
  Serial.println("CFG CODEC fine");
  ui.refresh();
}

// Trasmette un comando firmato al ripetitore (due copie, senza PTT). true se e' partito; il motivo di un rifiuto e' sul display.
static bool sendSysop(uint8_t code, uint16_t param) {
  if (!settings.sysopKeySet) {
    Serial.println("CFG SYSCMD ERR manca la chiave (CFG SET SYSOPKEY, poi CFG REBOOT)");
    ui.showMessage("MANCA LA CHIAVE", "Impostala dal Wi-Fi");
    delay(1500);
    return false;
  }
  const uint32_t counter = loadSysopCounter() + 1;
  uint16_t session = static_cast<uint16_t>(counter & 0xFFFF);
  if (session == 0) session = 1;  // 0 = "assegna tu": con una firma sulla sessione non va bene
  ui.setSendingCommand(true);
  if (!node->sendSingle(lcr::makeSysopCommand(session, settings.callsign, settings.sysopKey, {code, param, counter}), millis())) {
    ui.setSendingCommand(false);
    Serial.println("CFG SYSCMD ERR non trasmesso (canale occupato, nominativo o trasmissione in corso)");
    return false;
  }
  saveSysopCounter(counter);
  Serial.printf("CFG SYSCMD OK codice=%u contatore=%lu\n", static_cast<unsigned>(code), static_cast<unsigned long>(counter));
  return true;
}

// CFG SYSCMD STATO | SPEGNI [minuti] | RIACCENDI | RIAVVIA | IDENTIFICA | POTENZA <dBm>: trasmette un comando firmato al ripetitore
// (modo 5, docs/studio-ripetitore.md). Serve la chiave (CFG SET SYSOPKEY); il contatore cresce a ogni comando ed e' salvato.
static void runSysopCommand(const char* args) {
  char word[16] = {0};
  int param = 0;
  const int n = sscanf(args, " %15s %d", word, &param);
  uint8_t code = 0;
  if (n >= 1) {
    if (strcasecmp(word, "STATO") == 0) code = lcr::kSysopStatus;
    else if (strcasecmp(word, "SPEGNI") == 0) code = n >= 2 ? lcr::kSysopOffMinutes : lcr::kSysopOff;
    else if (strcasecmp(word, "RIACCENDI") == 0) code = lcr::kSysopOn;
    else if (strcasecmp(word, "RIAVVIA") == 0) code = lcr::kSysopRestart;
    else if (strcasecmp(word, "IDENTIFICA") == 0) code = lcr::kSysopIdentify;
    else if (strcasecmp(word, "POTENZA") == 0 && n >= 2) code = lcr::kSysopPower;
  }
  if (code == 0 || param < 0 || param > 65535) {
    Serial.println("CFG SYSCMD ERR uso: STATO | SPEGNI [minuti] | RIACCENDI | RIAVVIA | IDENTIFICA | POTENZA <dBm>");
    return;
  }
  sendSysop(code, static_cast<uint16_t>(param));
}

// ---- Prove radio del Morse (CFG RADIOTEST, docs/studio-identificativo-fm.md): CW puro e tono Morse in FM stretta con i pacchetti FSK ----

static bool radioTestAbort() {  // un tasto qualsiasi dalla seriale ferma la prova
  if (Serial.available() <= 0) return false;
  while (Serial.available() > 0) Serial.read();
  return true;
}

// Un identificativo: i pacchetti dei toni e le pause. Ritorna false se la radio non ha trasmesso o la prova e' stata fermata.
static bool radioTestMorse(const lcr::RadioTestRequest& rq, const lcr::MorseBurst* b, size_t n, float kbps, uint8_t pattern) {
  const uint32_t t0 = millis();
  for (size_t i = 0; i < n; i++) {
    if (radioTestAbort()) return false;
    if (b[i].tone) {
      if (!radio.fskSend(pattern, b[i].bytes)) {
        Serial.printf("CFG RADIOTEST ERR trasmissione (errore radio %d)\n", radio.lastError());
        return false;
      }
    } else {
      delay(b[i].ms);
    }
  }
  uint32_t planned = 0;
  for (size_t i = 0; i < n; i++) planned += b[i].tone ? static_cast<uint32_t>(lcr::burstAirMs(b[i].bytes, kbps) + 0.5f) : b[i].ms;
  Serial.printf("CFG RADIOTEST morse fine durata=%lu ms (prevista dai pacchetti %lu ms, testo %s)\n", static_cast<unsigned long>(millis() - t0),
                static_cast<unsigned long>(planned), rq.text);
  return true;
}

static void runRadioTest(const lcr::RadioTestRequest& rq) {
  if (lcr::isPlaceholderCallsign(settings.callsign)) {
    Serial.println("CFG RADIOTEST ERR imposta il nominativo (non N0CALL): la prova trasmette");
    return;
  }
  if (node->state() != lcr::VoiceNode::State::Idle || node->recPhase() != lcr::VoiceNode::RecPhase::None || node->meshcomActive()) {
    Serial.println("CFG RADIOTEST ERR la scheda sta ricevendo o trasmettendo");
    return;
  }
  if (radio.channelBusy()) {
    Serial.println("CFG RADIOTEST ERR canale occupato (CAD)");
    return;
  }
  const bool fmTone = rq.kind == lcr::RadioTestKind::Tone || ((rq.kind == lcr::RadioTestKind::Morse || rq.kind == lcr::RadioTestKind::Beacon) && rq.fm);
  const float kbps = lcr::radioTestBitRateKbps(rq);
  const float dev = fmTone ? rq.devKhz : lcr::kCwDevKhz;
  const float centerMhz = settings.freqMhz();
  // In FSK un bit 1 e' la frequenza centrale + la deviazione: per il CW (tutti 1) si compensa, cosi' la portante cade sulla frequenza
  const float carrierMhz = fmTone ? centerMhz : centerMhz - dev / 1000.0f;
  const uint8_t pattern = lcr::radioTestPattern(rq);
  static const char* const kKind[] = {"morse", "beacon", "carrier", "tone"};
  Serial.printf("CFG RADIOTEST start tipo=%s modo=%s freq=%.4f MHz portante=%.4f MHz dev=%.2f kHz bitrate=%.3f kbps tono=%.0f Hz potenza=%d dBm ramp=%d shape=%d\n",
                kKind[static_cast<int>(rq.kind)], fmTone ? "FM" : "CW", static_cast<double>(centerMhz), static_cast<double>(carrierMhz), static_cast<double>(dev),
                static_cast<double>(kbps), fmTone ? static_cast<double>(kbps * 500.0f) : 0.0, settings.powerDbm, rq.ramp, rq.shape);
  char l2[24];
  snprintf(l2, sizeof(l2), "%s %s", fmTone ? "FM" : "CW", rq.text[0] ? rq.text : kKind[static_cast<int>(rq.kind)]);
  ui.showMessage("PROVA MORSE", l2);

  lcr::MorseBurst bursts[160];
  size_t nb = 0;
  if (rq.kind == lcr::RadioTestKind::Morse || rq.kind == lcr::RadioTestKind::Beacon) {
    bool clamped = false;
    nb = lcr::planMorseBursts(rq, bursts, sizeof(bursts) / sizeof(bursts[0]), &clamped);
    if (clamped) Serial.println("CFG RADIOTEST avviso: un tono e' piu' lungo di un pacchetto (255 byte) ed e' stato accorciato (abbassa hz o alza wpm)");
    if (nb == 0) {
      Serial.println("CFG RADIOTEST ERR il testo non ha lettere ne' cifre");
      ui.refresh();
      return;
    }
  }

  uint32_t cycles = 0;
  bool ok = true;
  do {
    const uint32_t ts = millis();
    if (!radio.fskBegin(carrierMhz, kbps, dev, rq.ramp, fmTone ? rq.shape : 0)) {
      Serial.printf("CFG RADIOTEST ERR radio FSK non parte (errore %d)\n", radio.lastError());
      ok = false;
    } else {
      const uint32_t tb = millis() - ts;
      if (rq.kind == lcr::RadioTestKind::Carrier || rq.kind == lcr::RadioTestKind::Tone) {
        // portante o tono continuo: pacchetti da 255 byte uno dopo l'altro (fra uno e l'altro la portante si ferma per un attimo)
        const float packetMs = lcr::burstAirMs(255, kbps);
        const uint32_t n = static_cast<uint32_t>(rq.seconds * 1000.0f / packetMs) + 1;
        for (uint32_t i = 0; i < n && ok; i++) {
          if (radioTestAbort()) break;
          ok = radio.fskSend(pattern, 255);
        }
        Serial.printf("CFG RADIOTEST %s fine pacchetti=%lu da %.0f ms\n", kKind[static_cast<int>(rq.kind)], static_cast<unsigned long>(n), static_cast<double>(packetMs));
      } else {
        ok = radioTestMorse(rq, bursts, nb, kbps, pattern);
      }
      const uint32_t te = millis();
      radio.fskEnd();  // di nuovo LoRa e in ascolto
      cycles++;
      Serial.printf("CFG RADIOTEST ciclo %lu: cambio a FSK %lu ms, ritorno a LoRa %lu ms\n", static_cast<unsigned long>(cycles), static_cast<unsigned long>(tb),
                    static_cast<unsigned long>(millis() - te));
    }
    if (rq.kind == lcr::RadioTestKind::Beacon && ok) {  // pausa fino al prossimo identificativo, interrotta da un tasto
      const uint32_t until = millis() + static_cast<uint32_t>(rq.periodS) * 1000u;
      while (static_cast<int32_t>(until - millis()) > 0) {
        if (radioTestAbort()) {
          ok = false;
          break;
        }
        delay(20);
      }
    }
  } while (rq.kind == lcr::RadioTestKind::Beacon && ok);
  Serial.println("CFG RADIOTEST fine");
  ui.refresh();
}

// Righe "CFG ..." dalla seriale (pagina di configurazione o terminale). Le altre si ignorano.
static void serviceConsole() {
  static char line[224];  // "CFG SET TEXTn " + fino a 150 caratteri
  static size_t len = 0;
  static bool tooLong = false;
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      line[len] = 0;
      const bool skip = tooLong;
      len = 0;
      tooLong = false;
      if (skip) continue;  // riga troppo lunga: scartata
      if (strncasecmp(line, "CFG SYSCMD", 10) == 0) {
        runSysopCommand(line + 10);
        continue;
      }
      char reply[260];
      const lcr::ConsoleResult r = console->handleLine(line, reply, sizeof(reply));
      if (r != lcr::ConsoleResult::NotCommand) Serial.println(reply);
      if (r == lcr::ConsoleResult::MicMeasure) audio.startMeasure(3000);
      if (r == lcr::ConsoleResult::AudioTest) runAudioTest();
      if (r == lcr::ConsoleResult::RadioTest) runRadioTest(console->radioTestRequest());
      if (r == lcr::ConsoleResult::CodecTest) runCodecTest();
      audio.setMicFloor(console->stored().micFloorDb);  // il fondo si applica subito (CFG SET MICFLOOR)
      node->setTestMode(console->testMode());  // prova di collegamento: non salvata, spenta all'avvio
      node->setRecordMode(console->recordMode());  // registra e poi trasmetti: non salvato, spento all'avvio
      if (r == lcr::ConsoleResult::Reboot) {
        Serial.flush();
        delay(100);
        ESP.restart();
      }
    } else if (len < sizeof(line) - 1) {
      line[len++] = c;
    } else {
      tooLong = true;
    }
  }
}

// Configurazione dalla pagina interna (voce "Wi-Fi" del menu), come nel Tracker LoRa APRS di Guzman: la voce del menu salva
// un indicatore e riavvia; all'avvio, PRIMA di avviare Codec2, audio e radio, la scheda crea il suo Wi-Fi con una password
// salvata (casuale la prima volta) e serve la pagina su 192.168.4.1. Non trasmette e non riceve. Si esce riavviando.
static void rebootIntoWifiConfig() {
  WifiConfig::setBootRequest(true);
  Serial.println("[wifi] indicatore salvato: riavvio in modalita' configurazione Wi-Fi");
  ui.showMessage("RIAVVIO", "Configurazione Wi-Fi");
  Serial.flush();
  delay(1500);
  ESP.restart();
}

// Modo configurazione Wi-Fi: non ritorna mai (esce con un riavvio).
static void runWifiConfigMode() {
  char ssid[24], pass[16];
  ui.showMessage("WI-FI", "Avvio...");
  const bool ok = wifiConfig.begin(
      [](const char* line, char* reply, size_t cap) {
        const lcr::ConsoleResult r = console->handleLine(line, reply, cap);
        return r == lcr::ConsoleResult::Reboot;
      },
      ssid, sizeof(ssid), pass, sizeof(pass));
  if (!ok) {
    Serial.println("[wifi] ERRORE: il Wi-Fi non parte, riavvio");
    ui.showMessage("ERRORE", "Wi-Fi non parte");
    delay(2500);
    ESP.restart();
  }
  Serial.printf("[wifi] rete %s, password %s, pagina http://192.168.4.1 (trasmissione disattivata)\n", ssid, pass);
  ui.showWifi(ssid, pass);
  uint32_t noClientsSince = millis();
  for (;;) {
    const uint32_t now = millis();
    const bool vr = digitalRead(pins::kVolume) == LOW;
    if (!vr) volumeArmed = true;
    const lcr::ButtonGesture::Event ve = volumeButton.update(volumeArmed && vr, now);
    // Come nel Tracker di Guzman: senza client collegati per un po' la configurazione si chiude (qui dopo 5 minuti).
    if (WiFi.softAPgetStationNum() > 0) noClientsSince = now;
    if (wifiConfig.loop() || ve == lcr::ButtonGesture::Event::Long || now - noClientsSince > 5UL * 60UL * 1000UL) {
      ui.showMessage("RIAVVIO");
      Serial.flush();
      delay(500);
      ESP.restart();
    }
    delay(5);
  }
}

// Scelta confermata nel menu: si salva il profilo (le altre impostazioni salvate restano com'erano) e si riavvia.
static void applyProfileChoice(uint8_t id) {
  lcr::Settings s = console->stored();
  s.profile = id;
  const lcr::Profile& p = lcr::profileFor(id);
  if (!settingsStorage.save(s)) {
    Serial.println("[menu] ERRORE: salvataggio del profilo non riuscito");
    ui.showMessage("ERRORE", "Salvataggio fallito");
    delay(2000);
    ui.refresh();
    return;
  }
  Serial.printf("[menu] profilo %s scelto e salvato: riavvio\n", p.name);
  ui.showMessage("RIAVVIO", p.name);
  Serial.flush();
  delay(1500);
  ESP.restart();
}

// ---- Escursione MeshCom dal menu: testo (o solo ascolto), poi destinatario ----

static char modeLetter() { return settings.profile == lcr::kProfileMessage ? 'M' : (settings.profile == lcr::kProfileRange ? 'R' : 'S'); }

static void mcOpenText(uint32_t now) {
  const lcr::Settings& st = console->stored();
  size_t n = 0;
  for (size_t i = 0; i < lcr::kTextSlots && n < 7; i++) {
    if (st.texts[i][0]) {
      mcItems[n] = st.texts[i];
      mcSlots[n++] = static_cast<int>(i);
    }
  }
  mcItems[n] = "(solo ascolto)";
  mcSlots[n++] = -1;
  ui.setChoiceTitle("MESHCOM TESTO");
  mcStage = McStage::Text;
  if (!mcPicker.open(mcItems, n, now)) mcStage = McStage::None;
}

// Modo Sysop: l'elenco dei comandi al posto dei testi predefiniti
static void sysopOpenList(uint32_t now) {
  size_t n = 0;
  const lcr::SysopMenuEntry* e = lcr::sysopMenuEntries(n);
  for (size_t i = 0; i < n && i < lcr::ChoicePicker::kMaxItems; i++) {
    mcItems[i] = e[i].label;
    mcSlots[i] = static_cast<int>(i);
  }
  ui.setChoiceTitle("COMANDI SYSOP");
  mcStage = McStage::Sysop;
  if (!mcPicker.open(mcItems, n < lcr::ChoicePicker::kMaxItems ? n : lcr::ChoicePicker::kMaxItems, now)) mcStage = McStage::None;
}

static void mcStart(const char* text, const char* dest, uint32_t now) {
  // Oltre ai messaggi diretti a noi si vedono quelli dei gruppi impostati tra i destinatari (le risposte ai nostri QRV)
  const char* groups[lcr::kMcDestSlots];
  for (size_t i = 0; i < lcr::kMcDestSlots; i++) groups[i] = console->stored().mcDests[i];
  node->setMeshcomGroups(groups, lcr::kMcDestSlots);
  const uint32_t windowMs = static_cast<uint32_t>(console->stored().mcWindowS) * 1000u;
  if (node->startMeshcom(text, dest, now, windowMs)) {
    Serial.printf("[meshcom] escursione di %u s\n", static_cast<unsigned>(console->stored().mcWindowS));
    if (text) Serial.printf("[meshcom] testo verso %s: %s\n", dest, text);
  }  // altrimenti il motivo e' gia' sul display
}

// Scelta confermata: nello stadio del testo, poi in quello del destinatario
static void mcChosen(size_t index, uint32_t now) {
  const lcr::Settings& st = console->stored();
  if (mcStage == McStage::Sysop) {
    mcStage = McStage::None;
    size_t n = 0;
    const lcr::SysopMenuEntry* e = lcr::sysopMenuEntries(n);
    if (index < n) {
      Serial.printf("[sysop] comando %s\n", e[index].label);
      sendSysop(e[index].code, e[index].param);
    }
    return;
  }
  if (mcStage == McStage::Text) {
    const int slot = mcSlots[index];
    if (slot < 0) {  // solo ascolto
      mcStage = McStage::None;
      mcStart(nullptr, nullptr, now);
      return;
    }
    if (lcr::expandTextTemplate(st.texts[slot], settings.callsign, modeLetter(), settings.profileFreqKhz(), mcText, sizeof(mcText)) == 0) {
      ui.showMessage("TESTO NON VALIDO", "troppo lungo?");
      delay(1500);
      mcStage = McStage::None;
      return;
    }
    size_t n = 0;
    for (size_t i = 0; i < lcr::kMcDestSlots; i++) {
      if (st.mcDests[i][0]) {
        mcItems[n] = st.mcDests[i];
        mcSlots[n++] = static_cast<int>(i);
      }
    }
    if (n == 0) {
      ui.showMessage("NESSUN DESTINAT.", "Impostalo dal Wi-Fi");
      delay(1500);
      mcStage = McStage::None;
      return;
    }
    ui.setChoiceTitle("MESHCOM A");
    mcStage = McStage::Dest;
    if (!mcPicker.open(mcItems, n, now)) mcStage = McStage::None;
    return;
  }
  if (mcStage == McStage::Dest) {
    mcStage = McStage::None;
    mcStart(mcText, st.mcDests[mcSlots[index]], now);
  }
}

void loop() {
  const uint32_t now = millis();
  serviceConsole();


  const bool raw = pttRawPressed();
  if (!raw) pttArmed = true;  // un PTT gia' premuto all'accensione non fa partire la trasmissione
  const bool pttDown = pttUsable && pttArmed && raw;
  const lcr::PttButton::Event pttEvent = ptt.update(pttDown, now);

  // Pulsante volume (opzionale): pressione breve = un passo in su (da 10 torna a 1); pressione lunga (2 s) a scheda
  // ferma = menu delle modalita'. Nel menu il PTT sceglie la voce e non trasmette mai (vedi lib/voicecore/menu.h).
  const bool volRaw = digitalRead(pins::kVolume) == LOW;
  if (!volRaw) volumeArmed = true;
  lcr::ButtonGesture::Event volEvent = volumeButton.update(volumeArmed && volRaw, now);
  // Escursione MeshCom in corso: il volume lungo la termina subito e la radio torna al profilo voce (invece di aprire il menu)
  if (volEvent == lcr::ButtonGesture::Event::Long && node->meshcomActive() && !menu->isOpen() && !textPicker.isOpen() && !mcPicker.isOpen()) {
    Serial.println("[meshcom] escursione terminata con il volume lungo");
    node->endMeshcom();
    volEvent = lcr::ButtonGesture::Event::None;
  }
  const bool nodeIdle = node->state() == lcr::VoiceNode::State::Idle && node->recPhase() == lcr::VoiceNode::RecPhase::None;
  lcr::MenuFlow::Actions act;  // niente di fatto finche' e' aperta la scelta del testo (i tasti vanno a lei)
  if (textPicker.isOpen()) {
    const lcr::TextPicker::Actions pa = textPicker.update(pttEvent, volEvent, nodeIdle, now);
    act.redraw = pa.redraw;
    act.beep = pa.beep;
    if (pa.send) {
      // I segnaposti (%C nominativo, %M modo, %F frequenza) si sostituiscono qui, come per la voce MeshCom
      char sendBuf[lcr::kMaxTextChars + 1];
      if (lcr::expandTextTemplate(console->stored().texts[pa.slot], settings.callsign, modeLetter(), settings.profileFreqKhz(), sendBuf,
                                  sizeof(sendBuf)) == 0) {
        ui.showMessage("TESTO NON VALIDO", "troppo lungo?");
        delay(1500);
      } else {
        ui.setSendingText(true);
        if (node->sendText(sendBuf, now)) {
          Serial.printf("[testo] invio del testo %d: %s\n", pa.slot + 1, sendBuf);
        } else {
          ui.setSendingText(false);  // rifiutato (canale occupato, ricezione in corso...): il motivo e' gia' sul display
        }
      }
    }
  } else if (mcPicker.isOpen()) {
    const lcr::ChoicePicker::Actions ca = mcPicker.update(pttEvent, volEvent, nodeIdle, now);
    act.redraw = ca.redraw;
    act.beep = ca.beep;
    if (!mcPicker.isOpen() && !ca.chosen) mcStage = McStage::None;  // uscito senza scegliere
    if (ca.chosen) mcChosen(ca.index, now);
  } else {
    act = menuFlow->update(pttEvent, volEvent, nodeIdle, settings.profile, now);
  }
  // Secondi che restano all'escursione MeshCom (0 se non e' in corso): lettera T, frequenza e schermo
  ui.setRepeaterBusy(node->repeaterBusyRemainingSec(now));
  ui.setMeshcomRemaining(node->meshcomActive() ? static_cast<int>((node->meshcomRemainingMs(now) + 999) / 1000) : 0);
  if (act.pttToNode) {
    if (pttEvent == lcr::PttButton::Event::Pressed) node->pttPressed(now);
    if (pttEvent == lcr::PttButton::Event::Released) node->pttReleased(now);
  }
  if (act.volumeUp && ui.textOpen()) {
    ui.nextTextPage();  // si sta leggendo un testo: il volume breve passa alla pagina successiva
  } else if (act.volumeUp) {
    const int next = audio.volumeStep() >= lcr::AudioBackEnd::kVolumeSteps ? 1 : audio.volumeStep() + 1;
    audio.setVolumeStep(next);
    audio.beep(600, 40);
  }
  if (act.redraw) ui.refresh();
  switch (act.beep) {
    case lcr::MenuBeep::Open: audio.beep(800, 60); break;
    case lcr::MenuBeep::Next: audio.beep(600, 30); break;
    case lcr::MenuBeep::Confirm: audio.beep(1000, 60); break;
    case lcr::MenuBeep::Cancel: audio.beep(400, 100); break;
    case lcr::MenuBeep::None: break;
  }
  if (act.outcome.kind == lcr::Menu::Outcome::Kind::Selected) {
    if (act.outcome.id == lcr::kMenuWifiId) {
      rebootIntoWifiConfig();
    } else if (act.outcome.id == lcr::kMenuMeshId) {
      mcOpenText(now);
      ui.refresh();
    } else if (act.outcome.id == lcr::kMenuSysopId) {
      sysopOpenList(now);
      ui.refresh();
    } else if (act.outcome.id == lcr::kMenuTextId) {
      if (!textPicker.open(console->stored(), now)) {
        ui.showMessage("NESSUN TESTO", "Impostalo dal Wi-Fi");
        delay(1500);
      }
      ui.refresh();
    } else {
      applyProfileChoice(act.outcome.id);
    }
  }

  // GPS: lettura della seriale e posizione da usare (il GPS se ha un fix recente, altrimenti il QTH fisso). Si aggiorna
  // una volta al secondo: serve al pacchetto START (se la trasmissione della posizione e' accesa) e alla distanza sul display.
  gps.loop(now);
  static uint32_t lastPosMs = 0;
  if (now - lastPosMs >= 1000) {
    lastPosMs = now;
    const lcr::Position pos = gps.position(now, settings.qth());
    node->setOwnPosition(pos, settings.posTx);
    ui.setOwnPosition(pos);
    ui.setGpsStatus(gps.active(), gps.hasFix(now), gps.satellites());
  }

  battery.update(now);

  // Stato dell'alimentazione in seriale ogni 10 s: utile per seguire la carica.
  static uint32_t lastPmuLogMs = 0;
  if (now - lastPmuLogMs >= 10000) {
    lastPmuLogMs = now;
    if (pmu.present()) {
      pmu.logStatus();
    } else {
      Serial.printf("[batteria] %.2f V\n", static_cast<double>(battery.volts()));
    }
  }

  // Livello del microfono in seriale (2 volte al secondo) finche' il PTT e' premuto.
  static uint32_t lastMicLogMs = 0;
  if (pttDown && now - lastMicLogMs >= 500) {
    lastMicLogMs = now;
    const float peak = audio.micPeakDb();
    Serial.printf("[mic] livello %.0f dBFS, picco %.0f dBFS, offset %.1f%%, AGC in %.0f dBFS, guadagno %.0f dB, uscita al codec %.0f dBFS, errori %u%s\n",
                  static_cast<double>(audio.micRmsDb()), static_cast<double>(peak),
                  static_cast<double>(audio.micDcPercent()), static_cast<double>(audio.micAgcInDb()),
                  static_cast<double>(audio.micGainDb()), static_cast<double>(audio.micOutDb()),
                  static_cast<unsigned>(audio.micErrors()),
                  peak < -80.0f ? "  <- silenzio: controlla i cavi o prova LCR_MIC_SLOT 1" : (peak > -3.0f ? "  <- saturazione" : ""));
  }

  node->tick(now);

  // Fine della misura del fondo (CFG MIC MEASURE): il risultato torna alla pagina di configurazione.
  lcr::LevelStats levels;
  if (audio.takeMeasureResult(levels)) {
    if (levels.count() == 0) {
      Serial.println("CFG MIC ERR nessun dato dal microfono");
    } else {
      Serial.printf("CFG MIC fondo=%d min=%d max=%d frames=%u\n", levels.percentile(90), levels.minDb(), levels.maxDb(),
                    static_cast<unsigned>(levels.count()));
    }
  }

  // Registra e poi trasmetti: fasi in seriale, con i frame registrati e le perdite del microfono.
  static lcr::VoiceNode::RecPhase lastRec = lcr::VoiceNode::RecPhase::None;
  if (node->recPhase() != lastRec) {
    lastRec = node->recPhase();
    switch (lastRec) {
      case lcr::VoiceNode::RecPhase::Recording: Serial.println("[rec] registrazione..."); break;
      case lcr::VoiceNode::RecPhase::Waiting:
        Serial.printf("[rec] registrati %u frame (%.1f s), frame persi dal microfono %u; si trasmette tra 1 s\n",
                      static_cast<unsigned>(node->recFrames()), static_cast<double>(node->recFrames()) * 0.04,
                      static_cast<unsigned>(audio.micOverruns()));
        break;
      case lcr::VoiceNode::RecPhase::Sending: Serial.println("[rec] trasmissione del messaggio"); break;
      case lcr::VoiceNode::RecPhase::None: Serial.println("[rec] trasmissione finita"); break;
    }
  }

  // Modo prova cambiato (comando CFG TEST o tre clic sul PTT): lo stato del nodo e' quello che vale.
  static bool lastTestMode = false;
  if (node->testMode() != lastTestMode) {
    lastTestMode = node->testMode();
    console->setTestMode(lastTestMode);
    ui.setTestMode(lastTestMode);
    Serial.printf("[prova] modo prova %s\n", lastTestMode ? "ATTIVO" : "spento");
  }

  // Fine ricezione: riga di diagnostica sui pacchetti (vedi docs/prove.md).
  static lcr::VoiceNode::State prevState = lcr::VoiceNode::State::Idle;
  const lcr::VoiceNode::State st = node->state();
  if (prevState == lcr::VoiceNode::State::Idle && st == lcr::VoiceNode::State::Receiving) {
    radio.resetRxStats();
    audio.resetPlayStats();
  }
  if (prevState == lcr::VoiceNode::State::Receiving && st != lcr::VoiceNode::State::Receiving) {
    Serial.printf("[rx] pacchetti letti %u, errori CRC %u, sovrascritti %u, ritardo massimo di lettura %u ms\n",
                  static_cast<unsigned>(radio.rxPackets()), static_cast<unsigned>(radio.rxCrcErrors()),
                  static_cast<unsigned>(radio.rxOverwritten()), static_cast<unsigned>(radio.rxMaxDelayMs()));
    char line[200];
    audio.playStatsLine(line, sizeof(line));  // livelli del parlato decodificato: vedi docs/prove.md
    if (line[0]) Serial.println(line);
    audio.playTimingLine(line, sizeof(line));
    if (line[0]) Serial.println(line);
    printFrameHex("[rx-hex]", codec.decLog(), codec.decLogCount());
    codec.logReset();
    if (codec.decodeAvgUs() > 0)
      Serial.printf("[rx-codec] decodifica Codec2: media %u.%u ms, massimo %u.%u ms per frame (limite 40 ms)\n",
                    static_cast<unsigned>(codec.decodeAvgUs() / 1000), static_cast<unsigned>((codec.decodeAvgUs() % 1000) / 100),
                    static_cast<unsigned>(codec.decodeMaxUs() / 1000), static_cast<unsigned>((codec.decodeMaxUs() % 1000) / 100));
    codec.resetTiming();
  }
  if (prevState == lcr::VoiceNode::State::Transmitting && st != lcr::VoiceNode::State::Transmitting) {
    printFrameHex("[tx-hex]", codec.encLog(), codec.encLogCount());
    codec.logReset();
    if (codec.encodeAvgUs() > 0)
      Serial.printf("[tx-codec] codifica Codec2: media %u.%u ms, massimo %u.%u ms per frame (limite 40 ms)\n",
                    static_cast<unsigned>(codec.encodeAvgUs() / 1000), static_cast<unsigned>((codec.encodeAvgUs() % 1000) / 100),
                    static_cast<unsigned>(codec.encodeMaxUs() / 1000), static_cast<unsigned>((codec.encodeMaxUs() % 1000) / 100));
    codec.resetTiming();
  }
  prevState = st;

  audio.loop(now);
  ui.loop(now, node->state(), audio.volumeStep(), static_cast<int>(radio.lastRssi()), radio.lastSnr(),
          battery.volts());
  delay(1);
}
