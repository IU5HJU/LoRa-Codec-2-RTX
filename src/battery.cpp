#include "battery.h"

#include "board_pins.h"

#if defined(BOARD_TBEAM_V12)

void Battery::begin(Pmu& pmu) { pmu_ = &pmu; }

void Battery::update(uint32_t nowMs) {
  if (!pmu_ || !pmu_->present()) {
    volts_ = -1.0f;
    return;
  }
  if (nowMs - lastMs_ < 2000 && lastMs_ != 0) return;  // il PMU si legge via I2C: basta ogni 2 s
  lastMs_ = nowMs ? nowMs : 1;
  const int mv = pmu_->batteryMillivolt();  // -1 se la batteria e' assente
  volts_ = mv < 0 ? -1.0f : static_cast<float>(mv) / 1000.0f;
}

#else  // LoRa32 T3 V1.6.1: ADC su GPIO35 (partitore 100k/100k)

namespace {
constexpr uint32_t kSampleEveryMs = 50;
constexpr float kDividerRatio = 2.0f;  // 100k + 100k: la batteria e' il doppio della tensione letta
}  // namespace

void Battery::begin(Pmu&) {
  pinMode(pins::kBatteryAdc, INPUT);
  analogSetPinAttenuation(pins::kBatteryAdc, ADC_11db);  // fino a circa 3,1 V sul pin = 6,2 V di batteria
  // Riempimento iniziale della finestra (circa 50 ms, solo all'avvio).
  for (size_t i = 0; i < lcr::AverageFilter::kWindow; i++) {
    filter_.add(static_cast<float>(analogReadMilliVolts(pins::kBatteryAdc)));
    delay(3);
  }
  volts_ = filter_.average() * kDividerRatio / 1000.0f;
}

void Battery::update(uint32_t nowMs) {
  if (nowMs - lastMs_ < kSampleEveryMs) return;
  lastMs_ = nowMs;
  // analogReadMilliVolts usa la calibrazione del chip (eFuse), come l'esp_adc_cal di Guzman.
  filter_.add(static_cast<float>(analogReadMilliVolts(pins::kBatteryAdc)));
  volts_ = filter_.average() * kDividerRatio / 1000.0f;
}

#endif
