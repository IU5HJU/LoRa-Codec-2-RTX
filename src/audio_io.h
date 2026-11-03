// Audio hardware: I2S full duplex (INMP441 + MAX98357A) con la catena audio del core.
// Solo per ESP32 (Arduino): non e' compilato nei test sul PC.
//
//   microfono -> task di cattura (core 0) -> AudioFrontEnd -> PrerollMic -> IAudioIn (loop)
//   IAudioOut (loop) -> AudioBackEnd -> I2S TX -> MAX98357A
//
// Una sola porta I2S a 16 kHz, slot a 32 bit, stereo: il microfono usa lo slot
// sinistro, l'altoparlante riceve lo stesso campione su entrambi gli slot
// (cosi' la media dei canali del MAX98357A non dimezza il livello).
#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "audio_backend.h"
#include "audio_frontend.h"
#include "interfaces.h"
#include "preroll_mic.h"

class AudioIo {
 public:
  AudioIo();
  bool begin();

  lcr::IAudioIn& mic() { return mic_; }
  lcr::IAudioOut& speaker() { return speaker_; }

  // Da chiamare nel loop: spegne l'amplificatore dopo un po' di silenzio.
  void loop(uint32_t nowMs);

  void setVolumeStep(int step);
  int volumeStep() const { return volStep_; }
  // Tono breve per i segnali acustici (fino a ~300 ms). Rispetta volume e muto.
  void beep(float freqHz, uint32_t durationMs);

  uint32_t micErrors() const { return micErrors_; }
  uint32_t micOverruns() { return mic_.overruns(); }

 private:
  // Microfono con pre-roll protetto da mutex (task di cattura <-> loop principale).
  class SafeMic : public lcr::IAudioIn {
   public:
    SafeMic();
    void feed(const int16_t* pcm);
    void start() override;
    void stop() override;
    bool readFrame(int16_t* pcm) override;
    uint32_t overruns();

   private:
    lcr::PrerollMic ring_;
    SemaphoreHandle_t lock_;
  };

  class Speaker : public lcr::IAudioOut {
   public:
    explicit Speaker(AudioIo& io) : io_(io) {}
    void playFrame(const int16_t* pcm) override { io_.playFrame(pcm); }
    void playLost() override { io_.playLost(); }

   private:
    AudioIo& io_;
  };

  static void captureTask(void* arg);
  void captureLoop();
  void playFrame(const int16_t* pcm);
  void playLost();
  void writeMono(const int16_t* samples, size_t n);
  void ampOn();

  SafeMic mic_;
  Speaker speaker_;
  lcr::AudioFrontEnd front_;
  lcr::AudioBackEnd back_;

  int volStep_ = 7;
  bool ampOn_ = false;
  uint32_t lastPlayMs_ = 0;
  volatile uint32_t micErrors_ = 0;

  // Buffer grandi come membri (non sullo stack).
  int32_t rxStereo_[2 * lcr::kMicSamplesPerFrame];
  int32_t micMono_[lcr::kMicSamplesPerFrame];
  int16_t micPcm_[lcr::kFrameSamples];
  int32_t txStereo_[2 * 2 * lcr::kFrameSamples];
  int16_t spkPcm_[2 * lcr::kFrameSamples];
};
