// Posizione geografica: blocco di 7 byte nel pacchetto START (protocollo v2) e distanza tra due posizioni.
//
// Blocco sul canale (flag kFlagPosition, dopo il nominativo):
//   byte 0      stato: bit 0 = 1 se la posizione viene da un GPS, 0 se e' una posizione fissa impostata a mano;
//               gli altri bit sono riservati (zero in trasmissione, ignorati in ricezione)
//   byte 1-3    latitudine, 24 bit senza segno big endian: (lat + 90) / 180 * 2^24  (passo circa 1,2 m)
//   byte 4-6    longitudine, 24 bit senza segno big endian: (lon + 180) / 360 * 2^24 (passo circa 2,4 m all'equatore)
//
// In memoria le coordinate sono in decimilionesimi di grado (come i GPS u-blox): 43.7696 gradi = 437696000.
// Indipendente dalla scheda e testato sul PC (tests/test_position.cpp).
#pragma once

#include <cstddef>
#include <cstdint>

namespace lcr {

constexpr size_t kPositionBytes = 7;
constexpr int32_t kMaxLatE7 = 900000000;
constexpr int32_t kMaxLonE7 = 1800000000;

struct Position {
  bool valid = false;    // false: nessuna posizione
  bool fromGps = false;  // true GPS, false posizione fissa impostata dall'operatore
  int32_t latE7 = 0;     // -900000000..900000000
  int32_t lonE7 = 0;     // -1800000000..1800000000
};

// Coordinate nei limiti?
bool isValidCoordinate(int32_t latE7, int32_t lonE7);

// Scrive il blocco (kPositionBytes byte). false se la posizione non e' valida.
bool encodePosition(const Position& p, uint8_t* out);
// Legge il blocco. Le coordinate tornano arrotondate al passo del formato (circa 1-2 m).
void decodePosition(const uint8_t* in, Position& out);

// Distanza sulla sfera terrestre (formula dell'emisenoverso), in chilometri. 0 se una delle due posizioni non e' valida.
double distanceKm(const Position& a, const Position& b);

// Testo breve della distanza per il display: "0.4km", "12.3km" (sotto i 100 km un decimale), "123km", "<0.1km". Ritorna i caratteri scritti.
size_t formatDistance(char* out, size_t cap, double km);

}  // namespace lcr
