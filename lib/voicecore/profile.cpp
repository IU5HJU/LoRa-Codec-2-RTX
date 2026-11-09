#include "profile.h"

#include <cctype>
#include <cmath>
#include <cstring>

namespace lcr {

namespace {

constexpr size_t kNever = static_cast<size_t>(kMaxPackets) * kFramesPerPacket;  // "non partire prima della fine"

const Profile kProfiles[kProfileCount] = {
    // Standard: i valori storici di protocol.h e user_config.h
    {kProfileStandard, "std", "Standard", 8, 250.0f, 5, 12, kMaxTurnMs, false, kPrebufferFrames, kPrebufferTimeoutMs,
     kSessionTimeoutMs},
    // Messaggio: SF12/BW125 (+13 dB). Un pacchetto da 77 byte dura circa 3,4 s in aria: la ricezione aspetta la fine del
    // messaggio (o 12 s di silenzio) prima di riprodurlo. 10 s di voce a 1200 bps sono circa 91 s in aria.
    {kProfileMessage, "msg", "Messaggio", 12, 125.0f, 5, 12, 10000, true, kNever, 0x7FFFFFFFu, 12000},
};

}  // namespace

const Profile& profileFor(uint8_t id) { return id < kProfileCount ? kProfiles[id] : kProfiles[kProfileStandard]; }

bool profileFromCode(const char* code, uint8_t& id) {
  if (!code) return false;
  for (uint8_t i = 0; i < kProfileCount; i++) {
    const char* c = kProfiles[i].code;
    size_t k = 0;
    for (; c[k] && code[k]; k++) {
      if (tolower(static_cast<unsigned char>(code[k])) != c[k]) break;
    }
    if (c[k] == 0 && code[k] == 0) {
      id = i;
      return true;
    }
  }
  return false;
}

double airtimeMs(size_t bytes, const Profile& p) {
  const double ts = std::ldexp(1.0, p.sf) / (p.bwKhz * 1000.0) * 1000.0;  // durata del simbolo, ms
  const int de = ts >= 16.0 ? 1 : 0;                                         // ottimizzazione per basse velocita'
  const double num = 8.0 * static_cast<double>(bytes) - 4.0 * p.sf + 28.0 + 16.0;  // intestazione esplicita, CRC attivo
  const double den = 4.0 * (p.sf - 2 * de);
  const double payloadSymbols = 8.0 + std::fmax(std::ceil(num / den) * (p.cr), 0.0);
  return (p.preambleSymbols + 4.25) * ts + payloadSymbols * ts;
}

double messageAirtimeMs(uint32_t voiceMs, size_t callsignLen, const Profile& p) {
  // Segue TxSession: START senza frame, pacchetti da 12 frame (nominativo ogni 10 pacchetti), pacchetto END con i frame
  // residui e il nominativo, parita' dopo ogni 4 pacchetti e dopo l'ultimo gruppo parziale.
  const size_t frames = voiceMs / kFrameMs;
  const size_t cs = callsignLen ? 1 + callsignLen : 0;
  double t = 0;
  size_t seq = 0;
  size_t lastCs = 0;
  size_t groupCount = 0;
  size_t groupMax = 0;
  auto data = [&](size_t payload, bool force) {
    const bool withCs = cs && (force || seq == 0 || seq - lastCs >= kCallsignRepeatPackets);
    if (withCs) lastCs = seq;
    t += airtimeMs(5 + (withCs ? cs : 0) + payload, p);
    seq++;
    if (payload > groupMax) groupMax = payload;
    if (++groupCount == kParityGroup) {
      t += airtimeMs(5 + kParityHeader + groupMax, p);
      seq++;
      groupCount = 0;
      groupMax = 0;
    }
  };
  data(0, true);  // START
  size_t left = frames;
  while (left >= kFramesPerPacket) {
    data(kFramesPerPacket * kFrameBytes, false);
    left -= kFramesPerPacket;
  }
  data(left * kFrameBytes, true);  // END con i frame residui
  if (groupCount > 0) t += airtimeMs(5 + kParityHeader + groupMax, p);
  return t;
}

}  // namespace lcr
