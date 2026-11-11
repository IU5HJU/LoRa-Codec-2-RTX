// Messaggi di testo compatibili con le reti radioamatoriali a pacchetto "MeshCom" (433,175 MHz): codifica e decodifica dei
// frame di testo. Solo testo: la voce non passa da questa rete. Layout verificato su frame veri.
//
// Layout (byte):
//   0        tipo: 0x3A ':' testo, 0x21 '!' posizione, 0x40 '@' rapporto sui vicini (qui si leggono, si trasmette solo il testo)
//   1-4      ID del messaggio, 32 bit, byte meno significativo per primo
//   5        bit 0x80 passato dal server, 0x40 aggiungi il percorso, 0x20 app offline, 0x10 mesh; nibble basso = salti massimi
//   6...     percorso ASCII "MITTENTE[,VIA1,VIA2...]" fino a '>'
//   poi      destinatario ASCII ("*" = tutti, un nominativo-SSID, un gruppo) fino al carattere del tipo (':' per il testo)
//   poi      testo fino al byte 0x00; "{NNN" in fondo chiede la conferma di ricezione; la conferma e' un testo
//            "MITTENTE:ackNNN" verso chi ha scritto
//   poi      HW (1), MOD (1), FCS (2 byte, PRIMA il byte alto) = somma a 16 bit di tutti i byte dal tipo fino a MOD compreso
//   poi      (facoltativo) versione firmware (se presente deve essere >= 35), ultimo HW, sotto-versione, 0x7E
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace lcr {
namespace mc {

constexpr uint8_t kTypeText = 0x3A, kTypePos = 0x21, kTypeHey = 0x40;
constexpr size_t kMaxFrame = 255;       // UDP_TX_BUF_SIZE del firmware
constexpr uint8_t kFlagServer = 0x80, kFlagTrack = 0x40, kFlagAppOffline = 0x20, kFlagMesh = 0x10;

struct Frame {
  uint8_t type = kTypeText;
  uint32_t msgId = 0;
  uint8_t flags = kFlagMesh;   // bit alti del byte 5
  uint8_t maxHop = 5;          // nibble basso del byte 5
  char path[121] = {0};        // mittente e eventuali ripetitori, separati da virgola
  char dest[64] = {0};
  char payload[200] = {0};
  uint8_t hw = 0, mod = 0x88;
  uint16_t fcs = 0;
  bool fcsOk = false;          // solo in decodifica
  bool hasFwTrailer = false;
  uint8_t fwVersion = 0, lastHw = 0, fwSub = '#';
};

inline uint16_t fcsSum(const uint8_t* b, size_t n) {
  uint32_t s = 0;
  for (size_t i = 0; i < n; i++) s += b[i];
  return static_cast<uint16_t>(s);  // il firmware tiene un intero senza segno e scrive i due byte bassi
}

inline bool printable(uint8_t c) { return c >= 0x20 && c <= 0x7E; }

// true se il frame e' ben formato (la FCS giusta o sbagliata e' in f.fcsOk: il firmware scarta quelli con FCS sbagliata)
inline bool decode(const uint8_t* b, size_t n, Frame& f) {
  f = Frame();
  if (n < 16 || n > kMaxFrame) return false;
  if (b[0] != kTypeText && b[0] != kTypePos && b[0] != kTypeHey) return false;
  f.type = b[0];
  f.msgId = static_cast<uint32_t>(b[1]) | (static_cast<uint32_t>(b[2]) << 8) | (static_cast<uint32_t>(b[3]) << 16) | (static_cast<uint32_t>(b[4]) << 24);
  f.flags = b[5] & 0xF0;
  f.maxHop = b[5] & 0x0F;
  size_t i = 6, k = 0;
  bool ok = false;
  for (; i < n && i - 6 < 120; i++) {
    if (b[i] == '>') { ok = true; i++; break; }
    if (!printable(b[i])) return false;
    f.path[k++] = static_cast<char>(b[i]);
  }
  if (!ok) return false;
  k = 0;
  ok = false;
  size_t start = i;
  for (; i < n && i - start < 120; i++) {
    if (b[i] == f.type) { ok = true; i++; break; }
    if (!printable(b[i])) return false;
    if (k + 1 < sizeof(f.dest)) f.dest[k++] = static_cast<char>(b[i]);
  }
  if (!ok) return false;
  k = 0;
  ok = false;
  for (; i < n && k + 1 < sizeof(f.payload); i++) {
    if (b[i] == 0x00) { ok = true; i++; break; }
    f.payload[k++] = static_cast<char>(b[i]);
  }
  if (!ok) return false;
  if (i + 4 > n) return false;  // HW, MOD, FCS
  f.hw = b[i];
  f.mod = b[i + 1];
  f.fcs = static_cast<uint16_t>((b[i + 2] << 8) | b[i + 3]);
  f.fcsOk = f.fcs == fcsSum(b, i + 2);
  i += 4;
  if (i < n) {
    f.hasFwTrailer = true;
    f.fwVersion = b[i++];
    if (i < n) f.lastHw = b[i++];
    if (i < n) f.fwSub = b[i] == 0x7E || b[i] == 0x00 ? '#' : b[i];
  }
  if (f.hasFwTrailer && f.fwVersion > 0 && f.fwVersion < 35) return false;
  return true;
}

// Scrive il frame. 0 se non ci sta o se i campi non sono ammessi. La FCS si calcola qui.
inline size_t encode(const Frame& f, uint8_t* out, size_t cap) {
  const size_t lp = strlen(f.path), ld = strlen(f.dest), ly = strlen(f.payload);
  if (lp == 0 || ld == 0 || lp > 120 || ld > 120) return 0;
  const size_t need = 6 + lp + 1 + ld + 1 + ly + 1 + 4 + (f.hasFwTrailer ? 4 : 0);
  if (need > kMaxFrame - 10 || need > cap) return 0;
  for (size_t j = 0; j < lp; j++) if (!printable(static_cast<uint8_t>(f.path[j])) || f.path[j] == '>') return 0;
  for (size_t j = 0; j < ld; j++) if (!printable(static_cast<uint8_t>(f.dest[j])) || f.dest[j] == static_cast<char>(f.type)) return 0;
  for (size_t j = 0; j < ly; j++) if (static_cast<uint8_t>(f.payload[j]) < 0x20) return 0;  // 0x00 chiude il carico
  size_t p = 0;
  out[p++] = f.type;
  for (int s = 0; s < 4; s++) out[p++] = static_cast<uint8_t>(f.msgId >> (8 * s));
  out[p++] = static_cast<uint8_t>((f.flags & 0xF0) | (f.maxHop & 0x0F));
  memcpy(out + p, f.path, lp); p += lp;
  out[p++] = '>';
  memcpy(out + p, f.dest, ld); p += ld;
  out[p++] = f.type;
  memcpy(out + p, f.payload, ly); p += ly;
  out[p++] = 0x00;
  out[p++] = f.hw;
  out[p++] = f.mod;
  const uint16_t s = fcsSum(out, p);
  out[p++] = static_cast<uint8_t>(s >> 8);
  out[p++] = static_cast<uint8_t>(s & 0xFF);
  if (f.hasFwTrailer) {
    out[p++] = f.fwVersion;
    out[p++] = f.lastHw;
    out[p++] = f.fwSub ? f.fwSub : '#';
    out[p++] = 0x7E;
  }
  return p;
}

}  // namespace mc
}  // namespace lcr
