// LoRa-Codec-2-RTX: messaggi vocali broadcast a turni su LoRa, con Codec2.
// Scheda: LILYGO LoRa32 T3 V1.6.1 (433 MHz). Vedi README.md e docs/.
//
// ATTENZIONE: collegare l'antenna a 433 MHz prima di alimentare la scheda.
#include <Arduino.h>
#include <esp_system.h>

#include "audio_io.h"
#include "board_pins.h"
#include "codec2_adapter.h"
#include "packet.h"
#include "ptt_button.h"
#include "radio_lora.h"
#include "ui.h"
#include "user_config.h"
#include "voice_node.h"

static RadioLora radio;
static Codec2Adapter codec;
static AudioIo audio;
static UiNotifier ui(audio);
static lcr::VoiceNode* node = nullptr;
static lcr::PttButton ptt(LCR_PTT_DEBOUNCE_MS);
static lcr::PttButton volumeButton(30);

static uint16_t newSessionId() { return static_cast<uint16_t>(esp_random()); }

static void fatal(const char* what, int code = 0) {
  Serial.printf("ERRORE: %s (%d)\n", what, code);
  ui.showMessage("ERRORE", what);
  for (;;) delay(1000);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(pins::kPtt, LCR_PTT_ACTIVE_LOW ? INPUT_PULLUP : INPUT_PULLDOWN);
  pinMode(pins::kVolume, INPUT_PULLUP);

  if (!ui.begin()) Serial.println("OLED non trovato: si continua senza display");
  ui.showMessage("AVVIO");

  if (!lcr::isValidCallsign(LCR_CALLSIGN)) fatal("nominativo non valido");
  if (lcr::isPlaceholderCallsign(LCR_CALLSIGN)) {
    Serial.println("Nominativo N0CALL: trasmissione disattivata, solo ascolto");
    ui.showMessage("SOLO RX", "Imposta il nominativo");
    delay(2500);
  }
  if (!codec.begin()) fatal("Codec2");
  if (!audio.begin()) fatal("audio I2S");
  if (!radio.begin()) fatal("radio", radio.lastError());

  lcr::NodeConfig cfg;
  strncpy(cfg.callsign, LCR_CALLSIGN, lcr::kMaxCallsign);
  cfg.fec = LCR_FEC != 0;
  cfg.newSessionId = newSessionId;
  node = new lcr::VoiceNode(cfg, radio, codec, audio.mic(), audio.speaker(), ui);

  Serial.printf("%s pronto, %.3f MHz, SF%d BW%.0f\n", LCR_CALLSIGN, static_cast<double>(LCR_FREQUENCY_MHZ),
                LCR_SPREADING_FACTOR, static_cast<double>(LCR_BANDWIDTH_KHZ));
}

void loop() {
  const uint32_t now = millis();

  const bool pttDown = (digitalRead(pins::kPtt) == LOW) == (LCR_PTT_ACTIVE_LOW != 0);
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

  node->tick(now);
  audio.loop(now);
  ui.loop(now, node->state(), audio.volumeStep(), static_cast<int>(radio.lastRssi()), radio.lastSnr());
  delay(1);
}
