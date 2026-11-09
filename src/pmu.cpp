#include "pmu.h"

#include "board_pins.h"

#if defined(BOARD_TBEAM_V12)

#include <Wire.h>
#include <XPowersLib.h>

namespace {
XPowersLibInterface* g_pmu = nullptr;
}

// Sequenza ripresa da quella del codice di riferimento LilyGO per la T-Beam con AXP2101
// (examples/.../LoRaBoards.cpp, ramo ESP32).
bool Pmu::begin() {
  Wire.begin(pins::kOledSda, pins::kOledScl);
  g_pmu = new XPowersAXP2101(Wire, pins::kOledSda, pins::kOledScl);
  if (!g_pmu->init()) {
    delete g_pmu;
    g_pmu = nullptr;
    present_ = false;
    return false;
  }

  // Alimentazione dell'ESP32: protetta, mai da spegnere.
  g_pmu->setProtectedChannel(XPOWERS_DCDC1);

  // Canali non usati spenti, come nel codice di riferimento.
  g_pmu->disablePowerOutput(XPOWERS_DCDC2);
  g_pmu->disablePowerOutput(XPOWERS_DCDC3);
  g_pmu->disablePowerOutput(XPOWERS_DCDC4);
  g_pmu->disablePowerOutput(XPOWERS_DCDC5);
  g_pmu->disablePowerOutput(XPOWERS_ALDO1);
  g_pmu->disablePowerOutput(XPOWERS_ALDO4);
  g_pmu->disablePowerOutput(XPOWERS_BLDO1);
  g_pmu->disablePowerOutput(XPOWERS_BLDO2);
  g_pmu->disablePowerOutput(XPOWERS_DLDO1);
  g_pmu->disablePowerOutput(XPOWERS_DLDO2);
  g_pmu->disablePowerOutput(XPOWERS_CPULDO);

  // Radio LoRa: ALDO2 a 3,3 V.
  g_pmu->setPowerChannelVoltage(XPOWERS_ALDO2, 3300);
  g_pmu->enablePowerOutput(XPOWERS_ALDO2);

  // GPS: ALDO3. In questa fase non e' usato: lo teniamo spento per risparmiare batteria.
  g_pmu->disablePowerOutput(XPOWERS_ALDO3);

  // Carica della batteria 18650: 500 mA, fine carica a 4,2 V.
  g_pmu->setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_500MA);
  g_pmu->setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);

  g_pmu->enableBattVoltageMeasure();
  g_pmu->enableVbusVoltageMeasure();
  g_pmu->enableSystemVoltageMeasure();

  present_ = true;
  delay(100);  // la radio ha bisogno di qualche istante per alimentarsi prima di begin()

  Serial.println("[pmu] AXP2101 ok");
  // Configurazione letta dai registri del PMU: serve a capire perche' la scheda non parte a batteria.
  Serial.printf("[pmu] soglia di spegnimento VOFF %u mV, DC1 (ESP32) %u mV %s, radio ALDO2 %u mV %s, GPS ALDO3 %s\n",
                static_cast<unsigned>(g_pmu->getSysPowerDownVoltage()),
                static_cast<unsigned>(g_pmu->getPowerChannelVoltage(XPOWERS_DCDC1)),
                g_pmu->isPowerChannelEnable(XPOWERS_DCDC1) ? "acceso" : "SPENTO",
                static_cast<unsigned>(g_pmu->getPowerChannelVoltage(XPOWERS_ALDO2)),
                g_pmu->isPowerChannelEnable(XPOWERS_ALDO2) ? "acceso" : "SPENTO",
                g_pmu->isPowerChannelEnable(XPOWERS_ALDO3) ? "acceso" : "spento");
  logStatus();
  return true;
}

void Pmu::setGpsPower(bool on) {
  if (!g_pmu) return;
  if (on) {
    g_pmu->setPowerChannelVoltage(XPOWERS_ALDO3, 3300);
    g_pmu->enablePowerOutput(XPOWERS_ALDO3);
  } else {
    g_pmu->disablePowerOutput(XPOWERS_ALDO3);
  }
  Serial.printf("[pmu] GPS ALDO3 %s\n", g_pmu->isPowerChannelEnable(XPOWERS_ALDO3) ? "acceso" : "spento");
}

void Pmu::logStatus() {
  if (!g_pmu) return;
  Serial.printf("[pmu] batteria %s, %d mV, %d %%, USB %s, carica %s\n",
                g_pmu->isBatteryConnect() ? "presente" : "assente", batteryMillivolt(), batteryPercent(),
                usbPresent() ? "si" : "no", charging() ? "in corso" : "no");
}

int Pmu::batteryMillivolt() {
  if (!g_pmu || !g_pmu->isBatteryConnect()) return -1;
  return static_cast<int>(g_pmu->getBattVoltage());
}

int Pmu::batteryPercent() {
  if (!g_pmu || !g_pmu->isBatteryConnect()) return -1;
  return g_pmu->getBatteryPercent();
}

bool Pmu::charging() { return g_pmu && g_pmu->isCharging(); }

bool Pmu::usbPresent() { return g_pmu && g_pmu->isVbusIn(); }

#else  // schede senza PMU (T3 V1.6.1)

bool Pmu::begin() { return true; }
int Pmu::batteryMillivolt() { return -1; }
int Pmu::batteryPercent() { return -1; }
bool Pmu::charging() { return false; }
bool Pmu::usbPresent() { return false; }
void Pmu::setGpsPower(bool) {}
void Pmu::logStatus() {}

#endif
