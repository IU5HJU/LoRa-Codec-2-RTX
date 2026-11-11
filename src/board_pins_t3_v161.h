// Mappa dei pin per LILYGO LoRa32 T3 V1.6.1 (ESP32-PICO-D4, SX1278 433 MHz).
//
// Pin radio, OLED, SD e LED: dalla documentazione e da utilities.h del repository
// ufficiale Xinyuan-LilyGO/LilyGo-LoRa-Series (T3_V1_6_SX1278).
// Pin audio e PTT: scelti da noi tra quelli liberi sul connettore; vanno
// verificati con il cablaggio reale. Controllati contro i conflitti da
// tests/test_pins.cpp.
#pragma once

namespace pins_t3_v161 {

// --- Radio SX1278 (fissi sulla scheda) ---
constexpr int kRadioSclk = 5;
constexpr int kRadioMiso = 19;
constexpr int kRadioMosi = 27;
constexpr int kRadioCs = 18;
constexpr int kRadioDio0 = 26;
constexpr int kRadioRst = 23;
constexpr int kRadioDio1 = 33;
constexpr int kRadioDio2 = 32;

// --- Altre periferiche della scheda ---
constexpr int kOledSda = 21;
constexpr int kOledScl = 22;
constexpr int kSdCs = 13;    // slot microSD: pin liberi se la scheda non e' inserita
constexpr int kSdMosi = 15;
constexpr int kSdMiso = 2;
constexpr int kSdSclk = 14;
constexpr int kLed = 25;
constexpr int kBatteryAdc = 35;

// --- Audio: I2S in full duplex, BCLK e WS condivisi tra microfono e amplificatore ---
constexpr int kI2sBclk = 14;     // clock di bit (INMP441 SCK + MAX98357A BCLK)
constexpr int kI2sWs = 13;       // word select (INMP441 WS + MAX98357A LRC)
constexpr int kMicData = 34;     // INMP441 SD -> ESP32 (GPIO34: solo ingresso, va bene)
constexpr int kAmpData = 4;      // ESP32 -> MAX98357A DIN
constexpr int kAmpShutdown = 25; // MAX98357A SD (alto = acceso). Condiviso col LED di bordo.

// --- Ripetitore (due schede, nessun audio): la seriale fra RX e TX usa i pin dell'I2S, liberi in quel ruolo.
// Collegare TX di una scheda con RX dell'altra e le masse: A.14 -> B.13 e A.13 <- B.14. ---
constexpr int kLinkTx = kI2sBclk;
constexpr int kLinkRx = kI2sWs;
// Scheda RX con altoparlante di monitor (CFG SET MONITOR ON): l'I2S occupa i pin 14 e 13, quindi la seriale verso la TX passa
// da altri due: RX su GPIO34 (il pin dati del microfono, solo ingresso: qui senza microfono) e TX su GPIO2 (MISO dello slot
// microSD: senza scheda inserita). Collegare TX scheda TX (14) -> 34 della RX e 2 della RX -> 13 della TX. DA VERIFICARE sul banco.
constexpr int kLinkRxMon = kMicData;
constexpr int kLinkTxMon = kSdMiso;

// --- PTT: pulsante esterno tra GPIO0 e massa (la scheda ha solo RST) ---
constexpr int kPtt = 0;
constexpr bool kPttHasInternalPullup = true;

// --- Volume: pulsante esterno opzionale tra GPIO15 e massa (pull-up interno).
// Se non e' collegato resta alto e non fa nulla. GPIO15 e' un pin di strapping:
// non tenerlo premuto durante accensione o reset. ---
constexpr int kVolume = 15;

}  // namespace pins_t3_v161
