#include "audio_io.h"

#include <driver/i2s.h>

#include <cmath>

#include "board_pins.h"
#include "user_config.h"

namespace {
constexpr i2s_port_t kPort = I2S_NUM_0;
constexpr int kSampleRate = 16000;
constexpr uint32_t kAmpOffAfterMs = 400;

lcr::BackEndConfig backConfig() {
  lcr::BackEndConfig c;
  c.upsample2x = true;  // il microfono e l'I2S lavorano a 16 kHz
  return c;
}

lcr::FrontEndConfig frontConfig() {
  lcr::FrontEndConfig c;
  c.agc.maxGainDb = LCR_MIC_AGC_MAX_GAIN_DB;
  c.agc.gateDbfs = LCR_MIC_AGC_GATE_DBFS;
  return c;
}
}  // namespace

// ---- SafeMic ----

AudioIo::SafeMic::SafeMic() : ring_(5), lock_(xSemaphoreCreateMutex()) {}

void AudioIo::SafeMic::feed(const int16_t* pcm) {
  xSemaphoreTake(lock_, portMAX_DELAY);
  ring_.feed(pcm);
  xSemaphoreGive(lock_);
}

void AudioIo::SafeMic::start() {
  xSemaphoreTake(lock_, portMAX_DELAY);
  ring_.start();
  xSemaphoreGive(lock_);
}

void AudioIo::SafeMic::stop() {
  xSemaphoreTake(lock_, portMAX_DELAY);
  ring_.stop();
  xSemaphoreGive(lock_);
}

bool AudioIo::SafeMic::readFrame(int16_t* pcm) {
  xSemaphoreTake(lock_, portMAX_DELAY);
  const bool ok = ring_.readFrame(pcm);
  xSemaphoreGive(lock_);
  return ok;
}

uint32_t AudioIo::SafeMic::overruns() {
  xSemaphoreTake(lock_, portMAX_DELAY);
  const uint32_t n = ring_.overruns();
  xSemaphoreGive(lock_);
  return n;
}

// ---- AudioIo ----

AudioIo::AudioIo() : speaker_(*this), front_(frontConfig()), back_(backConfig()) {
  memset(rxStereo_, 0, sizeof(rxStereo_));
  memset(micMono_, 0, sizeof(micMono_));
  memset(micPcm_, 0, sizeof(micPcm_));
  memset(txStereo_, 0, sizeof(txStereo_));
  memset(spkPcm_, 0, sizeof(spkPcm_));
}

bool AudioIo::begin() {
  pinMode(pins::kAmpShutdown, OUTPUT);
  digitalWrite(pins::kAmpShutdown, LOW);  // amplificatore spento finche' non serve

  i2s_config_t cfg = {};
  cfg.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
  cfg.sample_rate = kSampleRate;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = static_cast<i2s_comm_format_t>(I2S_COMM_FORMAT_STAND_I2S);
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 8;
  cfg.dma_buf_len = 160;  // frame stereo per buffer DMA (10 ms)
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;  // in assenza di dati l'uscita e' silenzio
  cfg.fixed_mclk = 0;
  if (i2s_driver_install(kPort, &cfg, 0, nullptr) != ESP_OK) return false;

  i2s_pin_config_t pin = {};
  pin.mck_io_num = I2S_PIN_NO_CHANGE;
  pin.bck_io_num = pins::kI2sBclk;
  pin.ws_io_num = pins::kI2sWs;
  pin.data_out_num = pins::kAmpData;
  pin.data_in_num = pins::kMicData;
  if (i2s_set_pin(kPort, &pin) != ESP_OK) return false;
  i2s_zero_dma_buffer(kPort);

  setVolumeStep(volStep_);
  return xTaskCreatePinnedToCore(captureTask, "mic", 8192, this, 3, nullptr, 0) == pdPASS;
}

void AudioIo::captureTask(void* arg) { static_cast<AudioIo*>(arg)->captureLoop(); }

void AudioIo::captureLoop() {
  for (;;) {
    size_t bytes = 0;
    const esp_err_t e = i2s_read(kPort, rxStereo_, sizeof(rxStereo_), &bytes, portMAX_DELAY);
    if (e != ESP_OK || bytes != sizeof(rxStereo_)) {
      micErrors_++;
      // Senza questa pausa un errore ripetuto terrebbe occupato il core 0 e farebbe
      // scattare il watchdog (riavvio continuo).
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    for (size_t i = 0; i < lcr::kMicSamplesPerFrame; i++) micMono_[i] = rxStereo_[2 * i + LCR_MIC_SLOT];
    measureLevel();
    front_.processFrame(micMono_, micPcm_);
    mic_.feed(micPcm_);
  }
}

void AudioIo::measureLevel() {
  // Parole I2S a 32 bit con 24 bit significativi allineati a sinistra: >>8 e fondo scala 2^23.
  constexpr float kFullScale = 8388608.0f;
  const size_t n = lcr::kMicSamplesPerFrame;
  float sum = 0.0f;
  for (size_t i = 0; i < n; i++) sum += static_cast<float>(micMono_[i] >> 8);
  const float mean = sum / static_cast<float>(n);
  float sq = 0.0f;
  float peak = 0.0f;
  for (size_t i = 0; i < n; i++) {
    const float v = static_cast<float>(micMono_[i] >> 8) - mean;
    sq += v * v;
    const float a = v < 0 ? -v : v;
    if (a > peak) peak = a;
  }
  const float rms = sqrtf(sq / static_cast<float>(n));
  micRmsDb_ = 20.0f * log10f((rms + 1.0f) / kFullScale);
  micPeakDb_ = 20.0f * log10f((peak + 1.0f) / kFullScale);
  micDcPct_ = 100.0f * mean / kFullScale;
}

void AudioIo::setVolumeStep(int step) {
  if (step < 0) step = 0;
  if (step > lcr::AudioBackEnd::kVolumeSteps) step = lcr::AudioBackEnd::kVolumeSteps;
  volStep_ = step;
  back_.setVolumeStep(step);
}

void AudioIo::ampOn() {
  if (!ampOn_) {
    digitalWrite(pins::kAmpShutdown, HIGH);
    ampOn_ = true;
  }
  lastPlayMs_ = millis();
}

void AudioIo::loop(uint32_t nowMs) {
  if (ampOn_ && nowMs - lastPlayMs_ > kAmpOffAfterMs) {
    digitalWrite(pins::kAmpShutdown, LOW);
    ampOn_ = false;
  }
}

// Scrive campioni a 16 kHz su entrambi gli slot (alti 16 bit della parola da 32).
void AudioIo::writeMono(const int16_t* samples, size_t n) {
  if (n > 2 * lcr::kFrameSamples) n = 2 * lcr::kFrameSamples;
  for (size_t i = 0; i < n; i++) {
    const int32_t v = static_cast<int32_t>(samples[i]) << 16;
    txStereo_[2 * i] = v;
    txStereo_[2 * i + 1] = v;
  }
  size_t written = 0;
  i2s_write(kPort, txStereo_, n * 2 * sizeof(int32_t), &written, pdMS_TO_TICKS(200));
}

void AudioIo::playFrame(const int16_t* pcm) {
  ampOn();
  back_.processFrame(pcm, spkPcm_);
  writeMono(spkPcm_, back_.outputSamplesPerFrame());
}

void AudioIo::playLost() {
  ampOn();
  back_.processLost(spkPcm_);  // silenzio, ma si scrive comunque per mantenere il ritmo
  writeMono(spkPcm_, back_.outputSamplesPerFrame());
}

void AudioIo::beep(float freqHz, uint32_t durationMs) {
  if (back_.muted()) return;
  if (durationMs > 300) durationMs = 300;
  ampOn();
  const float amp = 0.25f * std::pow(10.0f, back_.volumeDb() / 20.0f) * 32767.0f;
  const size_t total = static_cast<size_t>(kSampleRate / 1000) * durationMs;
  const size_t fade = 80;  // 5 ms: evita i clic
  size_t done = 0;
  while (done < total) {
    const size_t n = min(total - done, static_cast<size_t>(2 * lcr::kFrameSamples));
    for (size_t i = 0; i < n; i++) {
      const size_t k = done + i;
      float env = 1.0f;
      if (k < fade) env = static_cast<float>(k) / fade;
      if (total - k < fade) env = static_cast<float>(total - k) / fade;
      spkPcm_[i] = static_cast<int16_t>(amp * env * sinf(2.0f * 3.14159265f * freqHz * static_cast<float>(k) / kSampleRate));
    }
    writeMono(spkPcm_, n);
    done += n;
  }
  lastPlayMs_ = millis();
}
