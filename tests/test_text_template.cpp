#include <cstring>
#include <string>

#include "protocol.h"
#include "testing.h"
#include "text_template.h"

using namespace lcr;

int main() {
  char out[200];
  CHECK(expandTextTemplate("%C QRV su VoLora modo %M %F", "IU5HJU", 'Q', 433900, out, sizeof(out)) == 35);
  CHECK(strcmp(out, "IU5HJU QRV su VoLora modo Q 433.900") == 0);
  // il primo testo predefinito: dice anche che non viene da un nodo della rete (le parentesi sono ammesse)
  CHECK(expandTextTemplate("%C QRV su VoLoRa modo %M %F (Inviato da dispositivo VoLoRa)", "IU5HJU", 'Q', 433900, out, sizeof(out)) == 67);
  CHECK(strcmp(out, "IU5HJU QRV su VoLoRa modo Q 433.900 (Inviato da dispositivo VoLoRa)") == 0);
  CHECK(expandTextTemplate("Frequenza %F, modo %M, 100%% sicuro", "IU5HJU", 'S', 433775, out, sizeof(out)) > 0);
  CHECK(strcmp(out, "Frequenza 433.775, modo S, 100% sicuro") == 0);
  CHECK(expandTextTemplate("Arrivo tra 10 minuti", "IU5HJU", 'S', 433900, out, sizeof(out)) == 20 && strcmp(out, "Arrivo tra 10 minuti") == 0);
  // un % seguito da un'altra lettera, o in fondo, resta com'e'
  CHECK(expandTextTemplate("sconto 50%x e 5%", "IU5HJU", 'S', 433900, out, sizeof(out)) > 0 && strcmp(out, "sconto 50%x e 5%") == 0);
  // frequenza con zeri
  CHECK(expandTextTemplate("%F", "IU5HJU", 'S', 433005, out, sizeof(out)) == 7 && strcmp(out, "433.005") == 0);
  // vuoto, troppo lungo dopo l'espansione, non valido, buffer piccolo
  CHECK(expandTextTemplate("", "IU5HJU", 'S', 433900, out, sizeof(out)) == 0 && out[0] == 0);
  std::string longT(140, 'x');
  longT += "%C%C";  // 140 + 12 = 152 > 150
  CHECK(expandTextTemplate(longT.c_str(), "IU5HJU", 'S', 433900, out, sizeof(out)) == 0);
  std::string ok150(138, 'x');
  ok150 += "%C";
  CHECK(expandTextTemplate(ok150.c_str(), "IU5HJU", 'S', 433900, out, sizeof(out)) == 144);
  CHECK(expandTextTemplate("a{b", "IU5HJU", 'S', 433900, out, sizeof(out)) == 0);  // le graffe non ammesse restano non ammesse
  CHECK(expandTextTemplate("%C QRV", "IU5HJU", 'S', 433900, out, 5) == 0);
  CHECK(expandTextTemplate(nullptr, "IU5HJU", 'S', 433900, out, sizeof(out)) == 0);
  CHECK(expandTextTemplate("%C", nullptr, 'S', 433900, out, sizeof(out)) == 0);  // nominativo mancante: testo vuoto
  TEST_MAIN_END();
}
