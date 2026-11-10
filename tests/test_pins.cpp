// Controlla che la mappa dei pin di ogni scheda non abbia conflitti e non usi pin vietati.
#include <vector>

#include "../src/board_pins_t3_v161.h"
#include "../src/board_pins_tbeam_v12.h"
#include "testing.h"

struct Board {
  std::vector<int> mine;     // pin che assegniamo noi: audio, PTT, volume
  std::vector<int> used;     // radio e I2C, fissi sulla scheda
  std::vector<int> outputs;  // pin usati come uscita
  int ptt, volume, micData;
  bool pttHasPullup;
};

#define BOARD_FROM(ns)                                                                                      \
  Board {                                                                                                   \
    {ns::kI2sBclk, ns::kI2sWs, ns::kMicData, ns::kAmpData, ns::kAmpShutdown, ns::kPtt, ns::kVolume},       \
        {ns::kRadioSclk, ns::kRadioMiso, ns::kRadioMosi, ns::kRadioCs, ns::kRadioDio0, ns::kRadioRst,      \
         ns::kRadioDio1, ns::kRadioDio2, ns::kOledSda, ns::kOledScl},                                      \
        {ns::kI2sBclk, ns::kI2sWs, ns::kAmpData, ns::kAmpShutdown}, ns::kPtt, ns::kVolume, ns::kMicData,  \
        ns::kPttHasInternalPullup                                                                           \
  }

static void checkCommon(const Board& b) {
  // Nessun pin assegnato due volte tra i nostri.
  for (size_t i = 0; i < b.mine.size(); i++)
    for (size_t j = i + 1; j < b.mine.size(); j++) CHECK(b.mine[i] != b.mine[j]);
  // Nessun conflitto con radio e I2C.
  for (int a : b.mine)
    for (int u : b.used) CHECK(a != u);
  // Pin vietati: flash SPI (6-11), UART0 (1, 3), strapping GPIO12.
  const std::vector<int> forbidden = {1, 3, 6, 7, 8, 9, 10, 11, 12};
  for (int a : b.mine)
    for (int f : forbidden) CHECK(a != f);
  // GPIO34-39 sono solo ingressi: nessuna uscita li usa, i dati del microfono si'.
  for (int out : b.outputs) CHECK(out < 34);
  CHECK(b.micData >= 34);
  // Il volume usa il pull-up interno: non puo' stare sui pin 34-39.
  CHECK(b.volume < 34);
  // Un PTT sui pin 34-39 non ha pull-up interno: va dichiarato.
  CHECK(b.ptt < 34 || !b.pttHasPullup);
}

int main() {
  // ---- T3 V1.6.1 ----
  {
    using namespace pins_t3_v161;
    const Board b = BOARD_FROM(pins_t3_v161);
    checkCommon(b);
    // Altre periferiche della scheda: ADC batteria e microSD. BCLK, WS e il pulsante volume
    // usano pin dello slot microSD (voluto: la scheda va usata senza microSD); gli altri no.
    for (int a : b.mine) {
      CHECK(a != kSdMiso && a != kBatteryAdc);
      if (a != kI2sBclk && a != kI2sWs && a != kVolume) CHECK(a != kSdCs && a != kSdSclk && a != kSdMosi);
    }
    CHECK(kI2sBclk == kSdSclk && kI2sWs == kSdCs && kVolume == kSdMosi);
    CHECK(kLed == kAmpShutdown);  // LED condiviso con lo shutdown dell'amplificatore
    CHECK(kPtt == 0 && kPttHasInternalPullup);
    CHECK(kLinkTx == kI2sBclk && kLinkRx == kI2sWs);  // la seriale del ripetitore riusa i pin dell'I2S
  }

  // ---- T-Beam V1.2 ----
  {
    using namespace pins_tbeam_v12;
    const Board b = BOARD_FROM(pins_tbeam_v12);
    checkCommon(b);
    // GPS, interrupt dell'alimentazione e LED non toccati dai nostri pin.
    for (int a : b.mine)
      for (int u : {kPmuIrq, kGpsRx, kGpsTx, kLed}) CHECK(a != u);
    CHECK(kGpsRx == 34 && kGpsTx == 12);
    // Il PTT e' il tasto di bordo IO38, senza pull-up interno.
    CHECK(kPtt == 38 && !kPttHasInternalPullup);
    CHECK(kLinkTx == kI2sBclk && kLinkRx == kI2sWs && kLinkTx < 34 && kLinkRx < 34);
    // La radio e' sugli stessi pin della T3.
    CHECK(kRadioSclk == pins_t3_v161::kRadioSclk && kRadioMiso == pins_t3_v161::kRadioMiso &&
          kRadioMosi == pins_t3_v161::kRadioMosi && kRadioCs == pins_t3_v161::kRadioCs &&
          kRadioDio0 == pins_t3_v161::kRadioDio0 && kRadioRst == pins_t3_v161::kRadioRst &&
          kRadioDio1 == pins_t3_v161::kRadioDio1);
  }
  TEST_MAIN_END();
}
