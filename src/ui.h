// Interfaccia utente: display OLED SSD1306 e segnali acustici.
// Implementa lcr::INotifier. Solo per ESP32 (Arduino): non e' compilato nei test sul PC.
#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>

#include "audio_io.h"
#include "interfaces.h"
#include "voice_node.h"

class UiNotifier : public lcr::INotifier {
 public:
  explicit UiNotifier(AudioIo& audio) : audio_(audio), display_(128, 64, &Wire, -1) {}

  // false se il display non risponde (il resto del sistema funziona comunque).
  bool begin();
  void showMessage(const char* line1, const char* line2 = nullptr);  // schermata bloccante (avvio/errori)

  // Da chiamare nel loop: ridisegna a ~4 Hz e quando cambia qualcosa.
  void loop(uint32_t nowMs, lcr::VoiceNode::State state, int volumeStep, int rssiDbm, float snrDb);

  // lcr::INotifier
  void txStarted() override;
  void txEnded(bool turnLimitReached) override;
  void pttDenied(lcr::DenyReason r) override;
  void rxStarted() override;
  void rxCallsign(const char* callsign) override;
  void rxEnded(const char* callsign, uint32_t lostFrames, uint32_t recoveredPackets) override;

 private:
  void draw(uint32_t nowMs);

  AudioIo& audio_;
  Adafruit_SSD1306 display_;
  bool ok_ = false;
  bool dirty_ = true;
  uint32_t lastDrawMs_ = 0;

  lcr::VoiceNode::State state_ = lcr::VoiceNode::State::Idle;
  int volume_ = 0;
  int rssi_ = 0;
  float snr_ = 0;
  uint32_t txStartMs_ = 0;
  bool limitHit_ = false;
  char rxCall_[lcr::kMaxCallsign + 1] = {0};
  uint32_t lastLost_ = 0;
  uint32_t lastRecovered_ = 0;
  bool haveLast_ = false;
  const char* notice_ = nullptr;
  uint32_t noticeUntilMs_ = 0;
};
