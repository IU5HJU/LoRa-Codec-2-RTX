#include <cstring>
#include <string>

#include "packet.h"
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
  // Firma automatica sulla rete di testo
  CHECK(signText("Ciao a tutti", out, sizeof(out)) == 12 + 1 + 31);
  CHECK(strcmp(out, "Ciao a tutti (Inviato da dispositivo VoLoRa)") == 0);
  CHECK(strlen(kVoLoRaSignature) == 31);
  // gia' firmato (anche in minuscolo o con spazi in fondo): non si ripete
  CHECK(signText("QRV (Inviato da dispositivo VoLoRa)", out, sizeof(out)) == 35 && strcmp(out, "QRV (Inviato da dispositivo VoLoRa)") == 0);
  CHECK(signText("QRV (inviato da dispositivo volora)  ", out, sizeof(out)) > 0 && strcmp(out, "QRV (inviato da dispositivo volora)") == 0);
  // testo lungo: si accorcia, la firma resta intera e il totale e' al massimo 150
  std::string longS(200, 'x');
  CHECK(signText(longS.c_str(), out, sizeof(out)) == kMaxTextChars);
  CHECK(strlen(out) == kMaxTextChars && strcmp(out + 150 - 31, kVoLoRaSignature) == 0 && out[118] == ' ');
  std::string exact(118, 'y');
  CHECK(signText(exact.c_str(), out, sizeof(out)) == 150);
  std::string over(119, 'z');
  CHECK(signText(over.c_str(), out, sizeof(out)) == 150 && out[117] == 'z' && out[118] == ' ');
  // vuoto, solo spazi, buffer piccolo, puntatori nulli
  CHECK(signText("", out, sizeof(out)) == 0 && signText("   ", out, sizeof(out)) == 0);
  CHECK(signText("ciao", out, 20) == 0);
  CHECK(signText(nullptr, out, sizeof(out)) == 0 && signText("ciao", nullptr, 10) == 0);
  // il testo firmato e' valido (ASCII, senza graffe)
  CHECK(signText("Ciao", out, sizeof(out)) > 0 && isValidText(out, strlen(out)));
  TEST_MAIN_END();
}
