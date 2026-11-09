#include <cmath>
#include <cstdlib>
#include <cstring>

#include "position.h"
#include "testing.h"

using namespace lcr;

static Position at(double lat, double lon, bool gps = false) {
  Position p;
  p.valid = true;
  p.fromGps = gps;
  p.latE7 = static_cast<int32_t>(std::lround(lat * 1e7));
  p.lonE7 = static_cast<int32_t>(std::lround(lon * 1e7));
  return p;
}

static void testRoundTrip() {
  // Errore massimo: mezzo passo = 90/2^24 gradi in latitudine (circa 0,6 m), 180/2^24 in longitudine (circa 1,2 m).
  const double kLatTol = 90.0 / 16777216.0 * 1e7 + 1;
  const double kLonTol = 180.0 / 16777216.0 * 1e7 + 1;
  const double pts[][2] = {{43.7696, 11.2558}, {-33.8688, 151.2093}, {0, 0}, {89.99999, 179.99999}, {-90, -180}, {90, 180}, {-0.0001, -0.0001}};
  for (const auto& c : pts) {
    const Position p = at(c[0], c[1], c[0] > 0);
    uint8_t b[kPositionBytes];
    CHECK(encodePosition(p, b));
    Position q;
    decodePosition(b, q);
    CHECK(q.valid && q.fromGps == p.fromGps);
    // Entro un passo dai limiti (poli, antimeridiano) l'ultimo valore del formato resta sotto il limite: errore di un passo intero.
    const bool edge = std::abs(c[0]) > 89.9 || std::abs(c[1]) > 179.9;
    CHECK(std::abs(static_cast<double>(q.latE7 - p.latE7)) <= (edge ? 2 * kLatTol : kLatTol));
    CHECK(std::abs(static_cast<double>(q.lonE7 - p.lonE7)) <= (edge ? 2 * kLonTol : kLonTol));
    CHECK(isValidCoordinate(q.latE7, q.lonE7));
  }
}

static void testEncodeRules() {
  uint8_t b[kPositionBytes];
  Position none;
  CHECK(!encodePosition(none, b));  // non valida
  Position out = at(0, 0);
  out.latE7 = kMaxLatE7 + 1;
  CHECK(!encodePosition(out, b));
  CHECK(!isValidCoordinate(0, kMaxLonE7 + 1));
  CHECK(isValidCoordinate(-kMaxLatE7, -kMaxLonE7));
  // Layout: stato, poi latitudine e longitudine a 24 bit. (0, 0) = meta' della scala = 0x800000.
  CHECK(encodePosition(at(0, 0, true), b));
  CHECK(b[0] == 1 && b[1] == 0x80 && b[2] == 0 && b[3] == 0 && b[4] == 0x80 && b[5] == 0 && b[6] == 0);
  CHECK(encodePosition(at(0, 0, false), b) && b[0] == 0);
  // I bit riservati dello stato si ignorano in ricezione.
  b[0] = 0xFE;
  Position q;
  decodePosition(b, q);
  CHECK(q.valid && !q.fromGps);
}

static void testDistance() {
  // Firenze - Roma: circa 232 km in linea d'aria.
  const double fr = distanceKm(at(43.7696, 11.2558), at(41.9028, 12.4964));
  CHECK(fr > 226.0 && fr < 238.0);
  CHECK(std::abs(distanceKm(at(43.7696, 11.2558), at(41.9028, 12.4964)) - distanceKm(at(41.9028, 12.4964), at(43.7696, 11.2558))) < 1e-9);
  CHECK(distanceKm(at(10, 10), at(10, 10)) == 0.0);
  // Un grado di latitudine: circa 111,2 km.
  const double d1 = distanceKm(at(0, 0), at(1, 0));
  CHECK(d1 > 111.0 && d1 < 111.4);
  // Attraverso l'antimeridiano e ai poli senza numeri assurdi.
  const double am = distanceKm(at(0, 179.5), at(0, -179.5));
  CHECK(am > 110.0 && am < 112.5);
  CHECK(distanceKm(at(90, 0), at(-90, 0)) > 20000.0);
  // Una posizione non valida: nessuna distanza.
  Position none;
  CHECK(distanceKm(none, at(1, 1)) == 0.0 && distanceKm(at(1, 1), none) == 0.0);
}

static void testFormat() {
  char s[16];
  formatDistance(s, sizeof(s), 0.03);
  CHECK(strcmp(s, "<0.1km") == 0);
  formatDistance(s, sizeof(s), 0.4);
  CHECK(strcmp(s, "0.4km") == 0);
  formatDistance(s, sizeof(s), 12.34);
  CHECK(strcmp(s, "12.3km") == 0);
  formatDistance(s, sizeof(s), 99.96);
  CHECK(strcmp(s, "100.0km") == 0);
  formatDistance(s, sizeof(s), 232.4);
  CHECK(strcmp(s, "232km") == 0);
  char tiny[4];
  const size_t n = formatDistance(tiny, sizeof(tiny), 232.4);
  CHECK(n == 3 && strlen(tiny) == 3);  // troncato ma terminato
}

int main() {
  testRoundTrip();
  testEncodeRules();
  testDistance();
  testFormat();
  TEST_MAIN_END();
}
