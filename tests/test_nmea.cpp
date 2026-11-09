#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "nmea.h"
#include "testing.h"

using namespace lcr;

// "$" + body + "*" + checksum + "\r\n"
static std::string sentence(const std::string& body) {
  uint8_t x = 0;
  for (char c : body) x ^= static_cast<uint8_t>(c);
  char tail[8];
  snprintf(tail, sizeof(tail), "*%02X\r\n", x);
  return "$" + body + tail;
}

static bool feedAll(NmeaParser& p, const std::string& s) {
  bool updated = false;
  for (char c : s) updated = p.feed(c) || updated;
  return updated;
}

static void testKnownSentences() {
  // Gli esempi classici della documentazione NMEA (checksum 47 e 6A).
  NmeaParser p;
  CHECK(feedAll(p, "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n"));
  CHECK(p.fix().valid && p.fix().sats == 8);
  CHECK(p.fix().latE7 == 481173000);   // 48 gradi e 7,038 minuti = 48,1173
  CHECK(p.fix().lonE7 == 115166667);   // 11 gradi e 31,000 minuti = 11,5166667
  CHECK(p.sentences() == 1 && p.checksumErrors() == 0);
  NmeaParser q;
  CHECK(feedAll(q, "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n"));
  CHECK(q.fix().valid && q.fix().latE7 == 481173000 && q.fix().lonE7 == 115166667);
}

static void testSigns() {
  NmeaParser p;
  // Sud e Ovest: negativi. Buenos Aires circa 34 36,0 S, 58 22,5 W.
  CHECK(feedAll(p, sentence("GNGGA,000001,3436.0000,S,05822.5000,W,1,09,1.0,25.0,M,0.0,M,,")));
  CHECK(p.fix().valid);
  CHECK(p.fix().latE7 == -346000000);
  CHECK(p.fix().lonE7 == -583750000);
  // Firenze: 43 46,176 N, 11 15,348 E = 43,7696 N, 11,2558 E
  CHECK(feedAll(p, sentence("GPGGA,000002,4346.176,N,01115.348,E,1,07,1.1,50.0,M,0.0,M,,")));
  CHECK(std::abs(p.fix().latE7 - 437696000) <= 1 && std::abs(p.fix().lonE7 - 112558000) <= 1);
  CHECK(p.fix().sats == 7);
}

static void testNoFix() {
  NmeaParser p;
  CHECK(feedAll(p, sentence("GPGGA,000001,3436.0000,S,05822.5000,W,1,09,1.0,25.0,M,0.0,M,,")));
  CHECK(p.fix().valid);
  // Qualita' 0 e campi vuoti: il fix e' perso, e la scheda lo sa.
  CHECK(feedAll(p, sentence("GPGGA,000010,,,,,0,00,99.99,,,,,,")));
  CHECK(!p.fix().valid && p.fix().sats == 0);
  // RMC non valida (V) senza coordinate.
  CHECK(feedAll(p, sentence("GPGGA,000020,3436.0000,S,05822.5000,W,1,05,1.0,25.0,M,0.0,M,,")));
  CHECK(p.fix().valid && p.fix().sats == 5);
  CHECK(feedAll(p, sentence("GPRMC,000021,V,,,,,,,010125,,,N")));
  CHECK(!p.fix().valid);
}

static void testRobustness() {
  NmeaParser p;
  // Checksum sbagliato: scartata, il fix non cambia.
  std::string good = sentence("GPGGA,000001,4346.176,N,01115.348,E,1,07,1.1,50.0,M,0.0,M,,");
  std::string bad = good;
  bad[20] = (bad[20] == '1') ? '2' : '1';  // un carattere cambiato nel corpo
  CHECK(!feedAll(p, bad));
  CHECK(!p.fix().valid && p.checksumErrors() == 1 && p.sentences() == 0);
  CHECK(feedAll(p, good) && p.fix().valid);
  // Rumore prima della frase, frase spezzata e ripresa: la frase nuova vince.
  NmeaParser r;
  CHECK(feedAll(r, "xx\xff\x01garbage $GPGG" + good));
  CHECK(r.fix().valid);
  // Frase troppo lunga: scartata senza uscire dai limiti, il parser resta utilizzabile.
  std::string longBody = "GPGGA,";
  longBody.append(300, '7');
  CHECK(!feedAll(r, sentence(longBody)));
  CHECK(feedAll(r, good));
  // Fine riga con solo \n; tipi che non ci interessano (GSV, GSA): contano come frasi ma non aggiornano il fix.
  NmeaParser s;
  const std::string gsv = sentence("GPGSV,3,1,11,03,03,111,00,04,15,270,00,06,01,010,00,13,06,292,00");
  CHECK(!feedAll(s, gsv));
  CHECK(s.sentences() == 1 && !s.fix().valid);
  // Coordinate non valide (minuti >= 60, latitudine oltre 90, emisfero sconosciuto).
  NmeaParser t;
  CHECK(!feedAll(t, sentence("GPGGA,1,4380.000,N,01115.348,E,1,07,1.1,50.0,M,0.0,M,,")));
  CHECK(!feedAll(t, sentence("GPGGA,1,9100.000,N,01115.348,E,1,07,1.1,50.0,M,0.0,M,,")));
  CHECK(!feedAll(t, sentence("GPGGA,1,4346.176,X,01115.348,E,1,07,1.1,50.0,M,0.0,M,,")));
  CHECK(!feedAll(t, sentence("GPGGA,1,4346.176,N,01115.348,E,1")));  // frase troppo corta: manca il campo dei satelliti
  CHECK(!t.fix().valid);
  // Checksum con lettere minuscole accettato; checksum mancante o troppo corto no.
  NmeaParser u;
  std::string lower = good;
  for (auto& ch : lower) {
    if (ch >= 'A' && ch <= 'F' && &ch > &lower[lower.size() - 5]) ch = static_cast<char>(ch - 'A' + 'a');
  }
  CHECK(feedAll(u, lower));
  CHECK(!feedAll(u, "$GPGGA,000001,4346.176,N\r\n"));
  CHECK(!feedAll(u, "$GPGGA,000001,4346.176,N*4\r\n"));
}

static void testChoosePosition() {
  GpsFix fix;
  fix.valid = true;
  fix.latE7 = 437696000;
  fix.lonE7 = 112558000;
  Position qth;
  qth.valid = true;
  qth.latE7 = 400000000;
  qth.lonE7 = 100000000;
  // GPS valido e recente: vince il GPS (stato "da GPS").
  Position p = choosePosition(fix, 500, qth);
  CHECK(p.valid && p.fromGps && p.latE7 == 437696000 && p.lonE7 == 112558000);
  // Al limite dell'eta': ancora il GPS; oltre: la posizione fissa.
  CHECK(choosePosition(fix, kGpsMaxAgeMs, qth).fromGps);
  p = choosePosition(fix, kGpsMaxAgeMs + 1, qth);
  CHECK(p.valid && !p.fromGps && p.latE7 == 400000000);
  // GPS senza fix: la posizione fissa; senza nemmeno quella: nessuna posizione.
  GpsFix none;
  p = choosePosition(none, 0, qth);
  CHECK(p.valid && !p.fromGps);
  Position noQth;
  CHECK(!choosePosition(none, 0, noQth).valid);
  CHECK(!choosePosition(fix, kGpsMaxAgeMs + 1, noQth).valid);
}

int main() {
  testKnownSentences();
  testSigns();
  testNoFix();
  testRobustness();
  testChoosePosition();
  TEST_MAIN_END();
}
