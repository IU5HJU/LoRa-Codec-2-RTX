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
#include "level_stats.h"
#include "tone_meter.h"
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

  // Livello del microfono (ultimo frame da 40 ms), per la diagnostica in seriale.
  // Valori in dB rispetto al fondo scala dell'I2S (0 dBFS = massimo, circa -90 = silenzio digitale).
  float micRmsDb() const { return micRmsDb_; }    // valore efficace, senza componente continua
  float micPeakDb() const { return micPeakDb_; }  // picco
  float micDcPercent() const { return micDcPct_; }  // offset continuo, % del fondo scala
  float micGainDb() const { return front_.gainDb(); }  // guadagno attuale dell'AGC
  // Prova dell'hardware audio (bloccante, circa 1 s): misura per 0,4 s il microfono alla frequenza del tono (pausa),
  // poi suona il tono per 0,5 s e rimisura. Livelli in dBFS alla sola frequenza data (Goertzel). Con il volume corrente:
  // chi chiama lo porta prima a un valore fisso e basso.
  void toneTest(float freqHz, float& pauseDb, float& toneDb);

  // Fondo del microfono (dBFS): l'AGC lavora sopra fondo + 6 dB.
  void setMicFloor(int floorDb);
  // "Misura il fondo": raccoglie per 'ms' il livello visto dall'AGC; il risultato si ritira con takeMeasureResult().
  void startMeasure(uint32_t ms);
  bool takeMeasureResult(lcr::LevelStats& out);  // true una volta sola a misura finita

  float micOutDb() const { return front_.outputDb(); }  // livello che va al codec (dopo l'AGC)

  // Diagnostica della riproduzione: livelli del parlato decodificato durante l'ultima ricezione.
  void resetPlayStats();
  // Scrive una riga di riepilogo in buf (vuota se non c'e' stato audio).
  void playStatsLine(char* buf, size_t cap) const;

  float micAgcInDb() const { return front_.agcInputDb(); }  // livello visto dall'AGC (dopo il passa-alto)

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
  void measureLevel();
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
  // statistiche di riproduzione (loop principale)
  uint32_t playFrames_ = 0, playLimited_ = 0;
  double playInPow_ = 0, playOutPow_ = 0;
  float playInMax_ = -120.0f, playOutMax_ = -120.0f, playGainMin_ = 99.0f, playGainMax_ = -99.0f;
  volatile bool toneActive_ = false;
  lcr::ToneMeter toneMeter_;
  volatile bool measuring_ = false;
  volatile bool measureDone_ = false;
  volatile uint32_t measureUntilMs_ = 0;
  lcr::LevelStats stats_;
  volatile float micRmsDb_ = -120.0f;
  volatile float micPeakDb_ = -120.0f;
  volatile float micDcPct_ = 0.0f;

  // Buffer grandi come membri (non sullo stack).
  int32_t rxStereo_[2 * lcr::kMicSamplesPerFrame];
  int32_t micMono_[lcr::kMicSamplesPerFrame];
  int16_t micPcm_[lcr::kFrameSamples];
  int32_t txStereo_[2 * 2 * lcr::kFrameSamples];
  int16_t spkPcm_[2 * lcr::kFrameSamples];
};
