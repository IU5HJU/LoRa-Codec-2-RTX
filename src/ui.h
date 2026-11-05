// Interfaccia utente: display OLED SSD1306 e segnali acustici.
// Implementa lcr::INotifier. Solo per ESP32 (Arduino): non e' compilato nei test sul PC.
#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>

#include "audio_io.h"
#include "interfaces.h"
#include "settings.h"
#include "voice_node.h"

class UiNotifier : public lcr::INotifier {
 public:
  explicit UiNotifier(AudioIo& audio) : audio_(audio), display_(128, 64, &Wire, -1) {}

  // Nominativo e frequenza mostrati sul display (da chiamare appena lette le impostazioni).
  void setSettings(const lcr::Settings& s) { settings_ = s; }

  // false se il display non risponde (il resto del sistema funziona comunque).
  bool begin();
  void showMessage(const char* line1, const char* line2 = nullptr);  // schermata bloccante (avvio/errori)

  // Da chiamare nel loop: ridisegna a ~4 Hz e quando cambia qualcosa.
  // batteryVolts: tensione della batteria in volt, negativa se non disponibile.
  void loop(uint32_t nowMs, lcr::VoiceNode::State state, int volumeStep, int rssiDbm, float snrDb,
            float batteryVolts);

  // lcr::INotifier
  void txStarted() override;
  void txEnded(bool turnLimitReached) override;
  void pttDenied(lcr::DenyReason r) override;
  void rxStarted() override;
  void rxCallsign(const char* callsign) override;
  void rxEnded(const char* callsign, uint32_t lostFrames, uint32_t recoveredPackets) override;
  void linkTestReport(bool fromRemote, const char* callsign, const lcr::TestReport& r) override;

  // Prova di collegamento attiva: lo schermo a riposo lo dice.
  void setTestMode(bool on) {
    if (on != testMode_) dirty_ = true;
    testMode_ = on;
  }

 private:
  void draw(uint32_t nowMs);
  void drawTestReport();

  AudioIo& audio_;
  lcr::Settings settings_;
  Adafruit_SSD1306 display_;
  bool ok_ = false;
  bool dirty_ = true;
  uint32_t lastDrawMs_ = 0;

  lcr::VoiceNode::State state_ = lcr::VoiceNode::State::Idle;
  int volume_ = 0;
  int rssi_ = 0;
  float snr_ = 0;
  float battery_ = -1.0f;        // ultima misura
  float batteryShown_ = -1.0f;   // valore a schermo (con isteresi, per non ridisegnare di continuo)
  uint32_t batteryShownMs_ = 0;
  uint32_t txStartMs_ = 0;
  bool limitHit_ = false;
  char rxCall_[lcr::kMaxCallsign + 1] = {0};
  uint32_t lastLost_ = 0;
  uint32_t lastRecovered_ = 0;
  bool haveLast_ = false;
  bool testMode_ = false;
  bool testShown_ = false;        // schermata con l'esito della prova
  uint32_t testUntilMs_ = 0;
  bool testFromRemote_ = false;
  char testCall_[lcr::kMaxCallsign + 1] = {0};
  lcr::TestReport testRep_;
  const char* notice_ = nullptr;
  uint32_t noticeUntilMs_ = 0;
};
