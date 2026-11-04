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
#include "ui.h"
#include "user_config.h"
#include "voice_node.h"

// Codec2 e il resto dell'applicazione girano nel task principale. Lo stack
// predefinito (8 kB) non basta: "stack overflow in task loopTask" e riavvio
// continuo. 32 kB come nel progetto di riferimento per Codec2.
SET_LOOP_TASK_STACK_SIZE(32 * 1024);

#if defined(BOARD_TBEAM_V12)
static const char* const kBoardName = "T-Beam V1.2";
#else
static const char* const kBoardName = "LoRa32 T3 V1.6.1";
#endif

static Pmu pmu;
static Battery battery;
static RadioLora radio;
static Codec2Adapter codec;
static AudioIo audio;
static UiNotifier ui(audio);
static lcr::VoiceNode* node = nullptr;
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
  Serial.printf("[avvio] scheda %s\n", kBoardName);

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

  if (!lcr::isValidCallsign(LCR_CALLSIGN)) fatal("nominativo non valido");
  if (lcr::isPlaceholderCallsign(LCR_CALLSIGN)) {
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
  Serial.printf("[avvio] audio ok, heap libero %u byte\n", static_cast<unsigned>(ESP.getFreeHeap()));

  ui.showMessage("RADIO");
  Serial.println("[avvio] radio SX127x...");
  if (!radio.begin()) fatal("radio", radio.lastError());
  Serial.println("[avvio] radio ok");

  lcr::NodeConfig cfg;
  strncpy(cfg.callsign, LCR_CALLSIGN, lcr::kMaxCallsign);
  cfg.fec = LCR_FEC != 0;
  cfg.newSessionId = newSessionId;
  node = new lcr::VoiceNode(cfg, radio, codec, audio.mic(), audio.speaker(), ui);

  Serial.printf("[avvio] stack libero minimo del task principale: %u byte\n",
                static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
  Serial.printf("%s pronto, %.3f MHz, SF%d BW%.0f\n", LCR_CALLSIGN, static_cast<double>(LCR_FREQUENCY_MHZ),
                LCR_SPREADING_FACTOR, static_cast<double>(LCR_BANDWIDTH_KHZ));
}

void loop() {
  const uint32_t now = millis();

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
    Serial.printf("[mic] livello %.0f dBFS, picco %.0f dBFS, offset %.1f%%, AGC %.0f dB, errori %u%s\n",
                  static_cast<double>(audio.micRmsDb()), static_cast<double>(peak),
                  static_cast<double>(audio.micDcPercent()), static_cast<double>(audio.micGainDb()),
                  static_cast<unsigned>(audio.micErrors()),
                  peak < -80.0f ? "  <- silenzio: controlla i cavi o prova LCR_MIC_SLOT 1" : (peak > -3.0f ? "  <- saturazione" : ""));
  }

  node->tick(now);
  audio.loop(now);
  ui.loop(now, node->state(), audio.volumeStep(), static_cast<int>(radio.lastRssi()), radio.lastSnr(),
          battery.volts());
  delay(1);
}
