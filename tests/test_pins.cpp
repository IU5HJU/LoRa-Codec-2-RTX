// Controlla che la mappa dei pin non abbia conflitti e non usi pin vietati.
#include <vector>

#include "../src/board_pins.h"
#include "testing.h"

int main() {
  using namespace pins;
  // Pin che assegniamo noi (audio e PTT) e pin gia' usati dalla scheda.
  const std::vector<int> mine = {kI2sBclk, kI2sWs, kMicData, kAmpData, kAmpShutdown, kPtt};
  const std::vector<int> board = {kRadioSclk, kRadioMiso, kRadioMosi, kRadioCs, kRadioDio0, kRadioRst,
                                  kRadioDio1, kRadioDio2, kOledSda, kOledScl, kBatteryAdc};

  // Nessun pin assegnato due volte tra i nostri.
  for (size_t i = 0; i < mine.size(); i++)
    for (size_t j = i + 1; j < mine.size(); j++) CHECK(mine[i] != mine[j]);

  // Nessun conflitto con radio, OLED e ADC batteria.
  for (int a : mine)
    for (int b : board) CHECK(a != b);

  // Pin vietati: flash SPI (6-11), UART0 (1, 3), strapping GPIO12.
  const std::vector<int> forbidden = {1, 3, 6, 7, 8, 9, 10, 11, 12};
  for (int a : mine)
    for (int f : forbidden) CHECK(a != f);

  // GPIO34-39 sono solo ingressi: nessuna uscita li puo' usare.
  for (int out : {kI2sBclk, kI2sWs, kAmpData, kAmpShutdown}) CHECK(out < 34);
  CHECK(kMicData >= 34);  // e' un ingresso

  // Il LED condiviso con lo shutdown dell'amplificatore e' voluto.
  CHECK(kLed == kAmpShutdown);
  // BCLK e WS sono sui pin dello slot microSD: servono liberi.
  CHECK(kI2sBclk == kSdSclk && kI2sWs == kSdCs);
  TEST_MAIN_END();
}
