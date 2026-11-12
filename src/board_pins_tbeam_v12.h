// Mappa dei pin per LILYGO T-Beam V1.2 (ESP32, AXP2101, radio SX127x 433 MHz, GPS u-blox).
//
// Pin della scheda (radio, OLED, GPS, tasto, alimentazione): dalla documentazione e da
// utilities.h del repository ufficiale Xinyuan-LilyGO/LilyGo-LoRa-Series (T_BEAM_SX1278)
// e dalle scritte stampate sulla scheda.
// Pin audio: scelti da noi tra quelli liberi sul connettore; da verificare con il
// cablaggio reale. Controllati contro i conflitti da tests/test_pins.cpp.
//
// SUPPORTO IN FASE 1: non ancora provato sulla scheda.
#pragma once

namespace pins_tbeam_v12 {

// --- Radio SX127x (stessi pin della T3) ---
constexpr int kRadioSclk = 5;
constexpr int kRadioMiso = 19;
constexpr int kRadioMosi = 27;
constexpr int kRadioCs = 18;
constexpr int kRadioDio0 = 26;
constexpr int kRadioRst = 23;
constexpr int kRadioDio1 = 33;
constexpr int kRadioDio2 = 32;

// --- Bus I2C condiviso: OLED opzionale (0x3C) e AXP2101 (0x34) ---
constexpr int kOledSda = 21;
constexpr int kOledScl = 22;

// --- Altre periferiche della scheda ---
constexpr int kPmuIrq = 35;   // interrupt dell'AXP2101
constexpr int kGpsRx = 34;    // ingresso dell'ESP32 (TX del GPS); occupato dal GPS
constexpr int kGpsTx = 12;    // uscita dell'ESP32 (RX del GPS)
constexpr int kLed = 4;       // LED di bordo, attivo basso

// --- Audio: I2S in full duplex, BCLK e WS condivisi tra microfono e amplificatore ---
constexpr int kI2sBclk = 14;      // INMP441 SCK + MAX98357A BCLK
constexpr int kI2sWs = 13;        // INMP441 WS + MAX98357A LRC
constexpr int kMicData = 36;      // INMP441 SD -> ESP32 (pin VP: solo ingresso, va bene)
constexpr int kAmpData = 2;       // ESP32 -> MAX98357A DIN (GPIO2: pin di avvio, ma qui e' un'uscita)
constexpr int kAmpShutdown = 25;  // MAX98357A SD (alto = acceso)

// --- Ripetitore: NON supportato sulla T-Beam (il ripetitore usa due T3: due T-Beam sarebbero uno spreco, docs/studio-ripetitore.md).
// Il firmware rifiuta il ruolo del ripetitore su questa scheda (CFG SET ROLE); i due pin servono solo perche' il codice compili. ---
constexpr int kLinkTx = kI2sBclk;
constexpr int kLinkRx = kI2sWs;

// --- PTT: tasto utente di bordo IO38 ---
// GPIO38 e' solo ingresso e non ha pull-up interno: si conta su quello esterno della
// scheda (da verificare). Il firmware controlla all'avvio che il pin sia stabile.
constexpr int kPtt = 38;
constexpr bool kPttHasInternalPullup = false;

// --- Volume: pulsante esterno opzionale tra GPIO15 e massa (pull-up interno) ---
constexpr int kVolume = 15;


}  // namespace pins_tbeam_v12
