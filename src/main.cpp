// LoRa-Codec-2-RTX: messaggi vocali broadcast a turni su LoRa, con Codec2.
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
#include "codec2_adapter.h"
#include "packet.h"
#include "pmu.h"
#include "ptt_button.h"
#include "radio_lora.h"
#include "settings.h"
#include "settings_nvs.h"
#include "ui.h"
#include "user_config.h"
#include "voice_node.h"

// Codec2 e il resto dell'applicazione girano nel task principale. Lo stack
// predefinito (8 kB) non basta: "stack overflow in task loopTask" e riavvio
// continuo. 32 kB come nel progetto di riferimento per Codec2.
SET_LOOP_TASK_STACK_SIZE(32 * 1024);

#if defined(BOARD_TBEAM_V12)
static const char* const kBoardName = "T-Beam V1.2";
static const char* const kBoardShort = "tbeam";
#else
static const char* const kBoardName = "LoRa32 T3 V1.6.1";
static const char* const kBoardShort = "t3";
#endif

static Pmu pmu;
static Battery battery;
static RadioLora radio;
static Codec2Adapter codec;
static AudioIo audio;
static UiNotifier ui(audio);
static lcr::VoiceNode* node = nullptr;
static NvsSettingsStorage settingsStorage;
static lcr::Settings settings;                // in uso: NVS se valida, altrimenti user_config.h
static lcr::SettingsConsole* console = nullptr;  // comandi CFG sulla seriale
static lcr::PttButton ptt(LCR_PTT_DEBOUNCE_MS);
static lcr::PttButton volumeButton(30);

static bool pttUsable = true;   // false se il pin PTT risulta instabile all'avvio
static bool pttArmed = false;   // il PTT vale solo dopo essere stato rilasciato almeno una volta

static uint16_t newSessionId() { return static_cast<uint16_t>(esp_random()); }

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
  console = new lcr::SettingsConsole(settings, defaults, settingsStorage, kBoardShort, LCR_VERSION);
  ui.setSettings(settings);

  // L'alimentazione va per prima: sulla T-Beam senza il PMU la radio non e' alimentata.
  const bool pmuOk = pmu.begin();
  battery.begin(pmu);

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
  if (!codec.begin()) fatal("Codec2");
  Serial.printf("[avvio] Codec2 ok, heap libero %u byte\n", static_cast<unsigned>(ESP.getFreeHeap()));

  ui.showMessage("AUDIO");
  Serial.println("[avvio] audio I2S...");
  if (!audio.begin()) fatal("audio I2S");
  audio.setVolumeStep(settings.volume);
  audio.setMicFloor(settings.micFloorDb);
  Serial.printf("[avvio] audio ok, heap libero %u byte\n", static_cast<unsigned>(ESP.getFreeHeap()));

  ui.showMessage("RADIO");
  Serial.println("[avvio] radio SX127x...");
  if (!radio.begin(settings)) fatal("radio", radio.lastError());
  Serial.println("[avvio] radio ok");

  lcr::NodeConfig cfg;
  strncpy(cfg.callsign, settings.callsign, lcr::kMaxCallsign);
  cfg.fec = LCR_FEC != 0;
  cfg.newSessionId = newSessionId;
  node = new lcr::VoiceNode(cfg, radio, codec, audio.mic(), audio.speaker(), ui);

  Serial.printf("[avvio] stack libero minimo del task principale: %u byte\n",
                static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
  Serial.printf("%s pronto, %.3f MHz, %d dBm, SF%d BW%.0f\n", settings.callsign,
                static_cast<double>(settings.freqMhz()), settings.powerDbm, LCR_SPREADING_FACTOR,
                static_cast<double>(LCR_BANDWIDTH_KHZ));
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
  constexpr int kTestVolumeStep = 5;  // -15 dB: udibile ma non forte
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

// Prova del Codec2 sulla scheda, con una scheda sola e senza radio (CFG TEST CODEC): 3 secondi dal microfono, poi due
// ascolti dalla stessa registrazione: 1) l'audio del microfono cosi' com'e', 2) lo stesso audio dopo codifica e
// decodifica Codec2. Entrambi passano dallo stesso percorso di riproduzione della ricezione. Confronto a orecchio e nei
// livelli: se il 1 e' buono e il 2 no, il guasto e' nel Codec2 sulla scheda. I frame codificati vengono stampati in
// esadecimale, per decodificarli sul PC con il Codec2 di riferimento.
static void runCodecTest() {
  constexpr size_t kFrames = 75;  // 3 secondi
  if (node->state() != lcr::VoiceNode::State::Idle) {
    Serial.println("CFG CODEC ERR occupata");
    return;
  }
  int16_t* raw = static_cast<int16_t*>(malloc(kFrames * lcr::kFrameSamples * sizeof(int16_t)));
  int16_t* dec = static_cast<int16_t*>(malloc(kFrames * lcr::kFrameSamples * sizeof(int16_t)));
  if (!raw || !dec) {
    free(raw);
    free(dec);
    Serial.println("CFG CODEC ERR memoria");
    return;
  }
  static uint8_t frames[kFrames * lcr::kFrameBytes];

  ui.showMessage("PARLA", "dopo il bip: 3 s");
  audio.beep(1000, 120);
  delay(350);  // il microfono tiene gli ultimi 200 ms prima dello start: cosi' non registra il bip
  audio.mic().start();
  size_t got = 0;
  const uint32_t t0 = millis();
  while (got < kFrames && millis() - t0 < 6000) {
    if (!audio.mic().readFrame(raw + got * lcr::kFrameSamples)) {
      delay(3);
      continue;
    }
    got++;
  }
  audio.mic().stop();
  if (got < kFrames) {
    free(raw);
    free(dec);
    Serial.printf("CFG CODEC ERR microfono: registrati solo %u frame\n", static_cast<unsigned>(got));
    return;
  }

  const uint32_t e0 = millis();
  for (size_t i = 0; i < kFrames; i++) codec.encode(raw + i * lcr::kFrameSamples, frames + i * lcr::kFrameBytes);
  const uint32_t encMs = millis() - e0;
  const uint32_t d0 = millis();
  for (size_t i = 0; i < kFrames; i++) codec.decode(frames + i * lcr::kFrameBytes, dec + i * lcr::kFrameSamples);
  const uint32_t decMs = millis() - d0;

  auto rmsDb = [](const int16_t* p, size_t n, int& peakOut) {
    double sq = 0;
    int pk = 0;
    for (size_t i = 0; i < n; i++) {
      sq += static_cast<double>(p[i]) * p[i];
      const int a = p[i] < 0 ? -p[i] : p[i];
      if (a > pk) pk = a;
    }
    peakOut = pk;
    return 10.0 * log10(sq / static_cast<double>(n) / (32768.0 * 32768.0) + 1e-12);
  };
  int pkRaw = 0, pkDec = 0;
  const double rRaw = rmsDb(raw, kFrames * lcr::kFrameSamples, pkRaw);
  const double rDec = rmsDb(dec, kFrames * lcr::kFrameSamples, pkDec);
  Serial.printf("CFG CODEC raw=%.1f rawpeak=%d dec=%.1f decpeak=%d encms=%u decms=%u frames=%u\n", rRaw, pkRaw, rDec,
                pkDec, static_cast<unsigned>(encMs), static_cast<unsigned>(decMs), static_cast<unsigned>(kFrames));
  for (size_t i = 0; i < kFrames * lcr::kFrameBytes; i += 60) {
    Serial.print("CFG CODEC hex ");
    for (size_t k = i; k < i + 60 && k < kFrames * lcr::kFrameBytes; k++) Serial.printf("%02x", frames[k]);
    Serial.println();
  }

  const int savedVolume = audio.volumeStep();
  audio.setVolumeStep(10);
  ui.showMessage("ASCOLTO 1", "senza codec");
  delay(500);
  audio.resetBackEnd();
  for (size_t i = 0; i < kFrames; i++) audio.speaker().playFrame(raw + i * lcr::kFrameSamples);
  delay(900);
  ui.showMessage("ASCOLTO 2", "con Codec2");
  delay(500);
  audio.resetBackEnd();
  for (size_t i = 0; i < kFrames; i++) audio.speaker().playFrame(dec + i * lcr::kFrameSamples);
  delay(900);
  audio.setVolumeStep(savedVolume);
  free(raw);
  free(dec);
  Serial.println("CFG CODEC fine");
  ui.refresh();
}

// Righe "CFG ..." dalla seriale (pagina di configurazione o terminale). Le altre si ignorano.
static void serviceConsole() {
  static char line[96];
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
      char reply[128];
      const lcr::ConsoleResult r = console->handleLine(line, reply, sizeof(reply));
      if (r != lcr::ConsoleResult::NotCommand) Serial.println(reply);
      if (r == lcr::ConsoleResult::MicMeasure) audio.startMeasure(3000);
      if (r == lcr::ConsoleResult::AudioTest) runAudioTest();
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

void loop() {
  const uint32_t now = millis();
  serviceConsole();

  const bool raw = pttRawPressed();
  if (!raw) pttArmed = true;  // un PTT gia' premuto all'accensione non fa partire la trasmissione
  const bool pttDown = pttUsable && pttArmed && raw;
  switch (ptt.update(pttDown, now)) {
    case lcr::PttButton::Event::Pressed: node->pttPressed(now); break;
    case lcr::PttButton::Event::Released: node->pttReleased(now); break;
    case lcr::PttButton::Event::None: break;
  }

  // Pulsante volume (opzionale): ogni pressione sale di un passo, da 10 torna a 1.
  if (volumeButton.update(digitalRead(pins::kVolume) == LOW, now) == lcr::PttButton::Event::Pressed) {
    const int next = audio.volumeStep() >= lcr::AudioBackEnd::kVolumeSteps ? 1 : audio.volumeStep() + 1;
    audio.setVolumeStep(next);
    audio.beep(600, 40);
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
    if (codec.decodeAvgUs() > 0)
      Serial.printf("[rx-codec] decodifica Codec2: media %u ms, massimo %u ms per frame (limite 40 ms)\n",
                    static_cast<unsigned>(codec.decodeAvgUs() / 1000), static_cast<unsigned>(codec.decodeMaxUs() / 1000));
    codec.resetTiming();
  }
  prevState = st;

  audio.loop(now);
  ui.loop(now, node->state(), audio.volumeStep(), static_cast<int>(radio.lastRssi()), radio.lastSnr(),
          battery.volts());
  delay(1);
}
