// LoRa-Codec-2-RTX: messaggi vocali broadcast a turni su LoRa, con Codec2.
// Schede: LILYGO LoRa32 T3 V1.6.1 (predefinita) e T-Beam V1.2 (-DBOARD_TBEAM_V12).
// Vedi README.md e docs/.
//
// ATTENZIONE: collegare l'antenna a 433 MHz prima di alimentare la scheda.
#include <Arduino.h>
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

// Prova dell'hardware audio (CFG TEST AUDIO): tre toni a volume fisso e basso, con il livello del microfono a ogni
// frequenza. Bloccante per circa 3,5 s. I risultati tornano alla pagina di configurazione; il giudizio e' dell'operatore.
static void runAudioTest() {
  constexpr int kTestVolumeStep = 5;  // -15 dB: udibile ma non forte
  ui.showMessage("PROVA TONI", "ascolta i toni");
  const int savedVolume = audio.volumeStep();
  audio.setVolumeStep(kTestVolumeStep);
  static const float kFreqs[] = {500.0f, 1000.0f, 2000.0f};
  for (float f : kFreqs) {
    float pauseDb = 0, toneDb = 0;
    audio.toneTest(f, pauseDb, toneDb);
    Serial.printf("CFG AUDIO f=%d base=%.1f tone=%.1f\n", static_cast<int>(f), static_cast<double>(pauseDb),
                  static_cast<double>(toneDb));
    delay(150);
  }
  audio.setVolumeStep(savedVolume);
  Serial.println("CFG AUDIO fine");
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
      audio.setMicFloor(console->stored().micFloorDb);  // il fondo si applica subito (CFG SET MICFLOOR)
      node->setTestMode(console->testMode());  // prova di collegamento: non salvata, spenta all'avvio
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
    Serial.printf("[mic] livello %.0f dBFS, picco %.0f dBFS, offset %.1f%%, AGC in %.0f dBFS, guadagno %.0f dB, errori %u%s\n",
                  static_cast<double>(audio.micRmsDb()), static_cast<double>(peak),
                  static_cast<double>(audio.micDcPercent()), static_cast<double>(audio.micAgcInDb()),
                  static_cast<double>(audio.micGainDb()),
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
  if (prevState == lcr::VoiceNode::State::Idle && st == lcr::VoiceNode::State::Receiving) radio.resetRxStats();
  if (prevState == lcr::VoiceNode::State::Receiving && st != lcr::VoiceNode::State::Receiving) {
    Serial.printf("[rx] pacchetti letti %u, errori CRC %u, sovrascritti %u, ritardo massimo di lettura %u ms\n",
                  static_cast<unsigned>(radio.rxPackets()), static_cast<unsigned>(radio.rxCrcErrors()),
                  static_cast<unsigned>(radio.rxOverwritten()), static_cast<unsigned>(radio.rxMaxDelayMs()));
  }
  prevState = st;

  audio.loop(now);
  ui.loop(now, node->state(), audio.volumeStep(), static_cast<int>(radio.lastRssi()), radio.lastSnr(),
          battery.volts());
  delay(1);
}
