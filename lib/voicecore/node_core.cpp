#include "node_core.h"

#include <cstring>

#include "packet.h"
#include "protocol.h"

namespace lcr {

NodeCore::NodeCore(IRadio& radio, INodeNet& net, const NodeLink::Config& link, uint32_t bootId, uint32_t (*rng)(), uint8_t voiceMode)
    : radio_(radio), net_(net), rng_(rng), link_(link, bootId, rng), voiceMode_(voiceMode) {
  nextSession_ = static_cast<uint16_t>(rng ? rng() : bootId);
  rep_.setModeMask(static_cast<uint16_t>((1u << voiceMode) | (1u << kModeText)));  // solo il modo voce del profilo e il testo
}

void NodeCore::onDatagram(const uint8_t* d, size_t n, uint32_t nowMs) {
  NodeLink::Rx& rx = link_.onDatagram(d, n, nowMs);
  for (size_t i = 0; i < rx.nReplies; i++) net_.sendDatagram(rx.replies[i].data, rx.replies[i].len);
  if (rx.kind != NodeLink::Kind::Packet) return;
  Packet p;
  if (!decodePacket(rx.packet, rx.packetLen, p) || !(p.mode == voiceMode_ || p.mode == kModeText)) {
    stats_.netBad++;
    return;
  }
  p.flags = static_cast<uint8_t>(p.flags | kFlagRelayed);  // anche se l'altro nodo non l'avesse messo: un pacchetto di rete e' gia' ripetuto
  uint8_t wire[kMaxWireBytes];
  const size_t m = encodePacket(p, wire, sizeof(wire));
  if (m == 0) {
    stats_.netBad++;
    return;
  }
  if (queue_.push(wire, m)) stats_.fromNet++;  // coda piena: contato dalla coda
}

void NodeCore::setIdentification(const char* callsign, uint32_t intervalMs, uint32_t freqKhz) {
  strncpy(callsign_, callsign ? callsign : "", kMaxCallsign);
  callsign_[kMaxCallsign] = 0;
  beaconIntervalMs_ = intervalMs;
  freqKhz_ = freqKhz;
}

void NodeCore::setMorseId(bool on, int wpm, float toneHz, float devKhz) {
  morseOn_ = on;
  morseReq_ = RadioTestRequest();
  morseReq_.kind = RadioTestKind::Morse;
  morseReq_.fm = true;
  morseReq_.wpm = wpm;
  morseReq_.toneHz = toneHz;
  morseReq_.devKhz = devKhz;
}

bool NodeCore::beaconDue(uint32_t nowMs) const {
  if (beaconIntervalMs_ == 0 || callsign_[0] == 0) return false;
  if (!haveBeacon_) return nowMs >= kBeaconFirstMs;
  return nowMs - lastBeaconMs_ >= beaconIntervalMs_;
}

// Dopo il testo: la radio passa in FSK e trasmette il nominativo in Morse a tono, un elemento alla volta a ogni giro (un tono e' un
// pacchetto e blocca per la sua durata, una pausa e' solo un'attesa).
void NodeCore::startMorse(uint32_t nowMs) {
  if (!morseOn_ || callsign_[0] == 0) return;
  strncpy(morseReq_.text, callsign_, sizeof(morseReq_.text) - 1);
  morseReq_.text[sizeof(morseReq_.text) - 1] = 0;
  morseN_ = planMorseBursts(morseReq_, morseBursts_, sizeof(morseBursts_) / sizeof(morseBursts_[0]), nullptr);
  if (morseN_ == 0) return;
  morseKbps_ = radioTestBitRateKbps(morseReq_);
  if (!radio_.fskBegin(static_cast<float>(freqKhz_) / 1000.0f, morseKbps_, morseReq_.devKhz, -1, 0)) {
    radio_.fskEnd();  // torna ai parametri di prima, se qualcosa e' cambiato
    stats_.morseAborted++;
    return;
  }
  morseIdx_ = 0;
  morseNextMs_ = nowMs;
  morseActive_ = true;
}

void NodeCore::morseStep(uint32_t nowMs) {
  if (static_cast<int32_t>(nowMs - morseNextMs_) < 0) return;
  if (morseIdx_ >= morseN_) {
    endMorse(false);
    return;
  }
  const MorseBurst& b = morseBursts_[morseIdx_++];
  if (!b.tone) {
    morseNextMs_ = nowMs + b.ms;
    return;
  }
  if (!radio_.fskSend(radioTestPattern(morseReq_), b.bytes)) {
    endMorse(true);
    return;
  }
  morseNextMs_ = nowMs;
}

// Fine del Morse (completo o interrotto): la radio torna LoRa ai parametri di prima, in ascolto.
void NodeCore::endMorse(bool aborted) {
  radio_.fskEnd();
  morseActive_ = false;
  if (aborted) stats_.morseAborted++;
  else stats_.morseIds++;
}

void NodeCore::tick(uint32_t nowMs, bool mayTransmit) {
  uint8_t dg[kNodeMaxDatagram];
  const size_t md = link_.maintenance(nowMs, dg, sizeof(dg));
  if (md) net_.sendDatagram(dg, md);

  // radio -> rete
  uint8_t buf[kMaxRxBytes];
  for (int i = 0; i < 4; i++) {
    int rssi = 0;
    const size_t n = radio_.receive(buf, sizeof(buf), &rssi);
    if (n == 0) break;
    Packet p;
    if (!decodePacket(buf, n, p)) {
      stats_.radioBad++;
      continue;
    }
    stats_.radioRx++;
    haveActivity_ = true;
    lastActivityMs_ = nowMs;
    Packet out;
    if (rep_.onPacket(p, nowMs, out) != Repeater::Verdict::Forward) continue;
    if (!link_.linkUp(nowMs)) {
      stats_.droppedLinkDown++;
      continue;
    }
    uint8_t wire[kMaxWireBytes];
    const size_t m = encodePacket(out, wire, sizeof(wire));
    const size_t k = m ? link_.makePacket(wire, m, dg, sizeof(dg)) : 0;
    if (k) {
      net_.sendDatagram(dg, k);
      stats_.toNet++;
    }
  }

  // Morse in corso: un pacchetto dalla rete (o la TX non piu' permessa) lo interrompe, altrimenti prosegue
  if (morseActive_) {
    if (queue_.size() > 0 || !mayTransmit) endMorse(true);
    else {
      morseStep(nowMs);
      return;
    }
  }

  // identificativo periodico: a canale libero, coda vuota e senza attivita' radio recente (voce in corso: prima la voce)
  if (mayTransmit && queue_.size() == 0 && beaconDue(nowMs)) {
    const bool quiet = !haveActivity_ || nowMs - lastActivityMs_ >= kQuietMs;
    if (!quiet || radio_.channelBusy()) {
      if (!deferredNoted_) stats_.beaconDeferred++;  // una volta per identificativo
      deferredNoted_ = true;
    } else {
      deferredNoted_ = false;
      uint8_t wire[kMaxWireBytes];
      const size_t m = encodePacket(makeBeaconPacket(nextSession_++, callsign_, "Nodo VoLoRa"), wire, sizeof(wire));
      if (m) {
        radio_.send(wire, m);
        haveActivity_ = true;
        lastActivityMs_ = nowMs;
        stats_.beacons++;
        startMorse(nowMs);  // poi il nominativo in Morse a tono, se acceso
      }
      haveBeacon_ = true;
      lastBeaconMs_ = nowMs;
      return;
    }
  }

  // rete -> radio
  if (queue_.size() == 0) return;
  uint8_t pkt[kMaxWireBytes];
  size_t len = 0;
  if (!mayTransmit) {
    if (queue_.next(nowMs, pkt, sizeof(pkt), len)) {
      queue_.sent(nowMs);  // senza nominativo si scarta tutto
      stats_.discardedNoTx++;
    }
    return;
  }
  if (radio_.channelBusy()) {  // una stazione locale sta trasmettendo: ha la precedenza
    stats_.busyDeferred++;
    return;
  }
  if (queue_.next(nowMs, pkt, sizeof(pkt), len)) {
    radio_.send(pkt, len);  // bloccante: dura il tempo in aria del pacchetto
    haveActivity_ = true;
    lastActivityMs_ = nowMs;
    queue_.sent(nowMs);
    stats_.transmitted++;
  }
}

}  // namespace lcr
