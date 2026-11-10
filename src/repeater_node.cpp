#include "repeater_node.h"

#include <Arduino.h>

#include "board_pins.h"
#include "packet.h"
#include "profile.h"
#include "radio_lora.h"
#include "repeater.h"
#include "repeater_link.h"

namespace {

constexpr uint32_t kLinkBaud = 115200;
constexpr uint32_t kHeartbeatMs = 1000;
constexpr uint32_t kLinkDeadMs = 3500;
constexpr uint32_t kDisplayMs = 500;

typedef std::function<bool(const char*, char*, size_t)> CfgHandler;

// Righe CFG dalla seriale USB: stesso comportamento della console della scheda normale. true se serve riavviare.
class UsbConsole {
 public:
  explicit UsbConsole(CfgHandler h) : handler_(h) {}
  bool poll() {
    while (Serial.available() > 0) {
      const char c = static_cast<char>(Serial.read());
      if (c == '\n' || c == '\r') {
        line_[len_] = 0;
        const bool skip = tooLong_;
        len_ = 0;
        tooLong_ = false;
        if (skip || line_[0] == 0) continue;
        char reply[260];
        reply[0] = 0;
        const bool reboot = handler_(line_, reply, sizeof(reply));
        if (reply[0]) Serial.println(reply);
        if (reboot) return true;
      } else if (len_ < sizeof(line_) - 1) {
        line_[len_++] = c;
      } else {
        tooLong_ = true;
      }
    }
    return false;
  }

 private:
  CfgHandler handler_;
  char line_[96];
  size_t len_ = 0;
  bool tooLong_ = false;
};

[[noreturn]] void restartNow(UiNotifier& ui) {
  ui.showMessage("RIAVVIO");
  Serial.flush();
  delay(500);
  ESP.restart();
  for (;;) delay(1000);
}

[[noreturn]] void stopWithError(UiNotifier& ui, const char* what, int code) {
  Serial.printf("[ripetitore] ERRORE: %s (%d)\n", what, code);
  ui.showMessage("ERRORE", what);
  for (;;) delay(1000);
}

void sendFrame(uint8_t type, const uint8_t* data, size_t len) {
  uint8_t frame[lcr::kLinkMaxFrame];
  const size_t n = lcr::encodeLinkFrame(type, data, len, frame, sizeof(frame));
  if (n) Serial2.write(frame, n);
}

// ---- Scheda RX: ascolta l'ingresso, decide, manda alla TX ----
[[noreturn]] void runRx(const lcr::Settings& settings, UiNotifier& ui, CfgHandler handler) {
  RadioLora radio;
  if (!radio.begin(settings)) stopWithError(ui, "radio", radio.lastError());
  Serial2.begin(kLinkBaud, SERIAL_8N1, pins::kLinkRx, pins::kLinkTx);
  const lcr::Profile& prof = lcr::profileFor(settings.profile);
  lcr::Repeater rep;
  rep.setModeMask(static_cast<uint16_t>(1u << prof.codecMode));  // solo il modo del profilo (di base il 1200)
  lcr::LinkParser parser;
  UsbConsole usb(handler);

  uint32_t received = 0, badPackets = 0, sent = 0, discarded = 0;
  uint8_t txQueue = 0, txDropped = 0;
  uint32_t lastHbMs = 0, lastStatusMs = 0, lastDispMs = 0;
  Serial.printf("[ripetitore] RX: ingresso %.3f MHz, profilo %s, modo ripetuto %u\n", static_cast<double>(settings.freqMhz()), prof.name,
                static_cast<unsigned>(prof.codecMode));

  for (;;) {
    const uint32_t now = millis();

    // 1. radio: un pacchetto con CRC valido
    uint8_t buf[lcr::kMaxWireBytes + 8];
    int rssi = 0;
    const size_t n = radio.receive(buf, sizeof(buf), &rssi);
    if (n > 0) {
      received++;
      lcr::Packet p;
      if (!lcr::decodePacket(buf, n, p)) {
        badPackets++;  // non e' un pacchetto del progetto
      } else {
        lcr::Packet out;
        const lcr::Repeater::Verdict v = rep.onPacket(p, now, out);
        if (v == lcr::Repeater::Verdict::Forward) {
          uint8_t wire[lcr::kMaxWireBytes];
          const size_t m = lcr::encodePacket(out, wire, sizeof(wire));
          if (m > 0) {
            sendFrame(lcr::kLinkPacket, wire, m);
            sent++;
          }
        } else {
          discarded++;
        }
      }
    }

    // 2. battito verso la TX
    if (now - lastHbMs >= kHeartbeatMs) {
      lastHbMs = now;
      uint8_t hb[4] = {static_cast<uint8_t>(received >> 24), static_cast<uint8_t>(received >> 16), static_cast<uint8_t>(received >> 8),
                       static_cast<uint8_t>(received)};
      sendFrame(lcr::kLinkHeartbeat, hb, sizeof(hb));
    }

    // 3. stato dalla TX
    while (Serial2.available() > 0) {
      if (parser.feed(static_cast<uint8_t>(Serial2.read())) && parser.frame().type == lcr::kLinkStatus && parser.frame().len >= 2) {
        txQueue = parser.frame().data[0];
        txDropped = parser.frame().data[1];
        lastStatusMs = now;
      }
    }

    // 4. display
    if (now - lastDispMs >= kDisplayMs) {
      lastDispMs = now;
      char l1[24], l2[24], l3[24], l4[24], l5[24];
      snprintf(l1, sizeof(l1), "RIPETITORE  RX");
      snprintf(l2, sizeof(l2), "In %.3f MHz %s", static_cast<double>(settings.freqMhz()), prof.code);
      snprintf(l3, sizeof(l3), "Ric %lu  Rip %lu", static_cast<unsigned long>(received), static_cast<unsigned long>(sent));
      snprintf(l4, sizeof(l4), "Scarti %lu  Altri %lu", static_cast<unsigned long>(discarded), static_cast<unsigned long>(badPackets));
      const bool linkOk = lastStatusMs != 0 && now - lastStatusMs < kLinkDeadMs;
      snprintf(l5, sizeof(l5), "TX %s coda %u", linkOk ? "ok" : "NON RISPONDE", static_cast<unsigned>(txQueue));
      ui.showLines(l1, l2, l3, l4, l5);
    }
    (void)txDropped;

    if (usb.poll()) restartNow(ui);
    delay(1);
  }
}

// ---- Scheda TX: riceve dalla RX e trasmette sull'uscita ----
[[noreturn]] void runTx(const lcr::Settings& settings, UiNotifier& ui, CfgHandler handler) {
  // La radio lavora sulla frequenza di uscita al posto di quella del profilo.
  const lcr::Settings out = settings.withProfileFreqKhz(settings.outFreqKhz());
  RadioLora radio;
  if (!radio.begin(out)) stopWithError(ui, "radio", radio.lastError());
  Serial2.begin(kLinkBaud, SERIAL_8N1, pins::kLinkRx, pins::kLinkTx);
  const lcr::Profile& prof = lcr::profileFor(settings.profile);
  lcr::LinkParser parser;
  lcr::TxQueue queue;
  UsbConsole usb(handler);
  const bool mayTransmit = !lcr::isPlaceholderCallsign(settings.callsign);  // come le altre schede: con N0CALL non si trasmette

  uint32_t transmitted = 0, rxCount = 0;
  uint32_t lastHbMs = 0, lastStatusMs = 0, lastDispMs = 0;
  Serial.printf("[ripetitore] TX: uscita %.3f MHz, profilo %s, nominativo %s%s\n", static_cast<double>(out.freqMhz()), prof.name, settings.callsign,
                mayTransmit ? "" : " (N0CALL: non trasmette)");

  for (;;) {
    const uint32_t now = millis();

    // 1. dalla RX: pacchetti e battito
    while (Serial2.available() > 0) {
      if (!parser.feed(static_cast<uint8_t>(Serial2.read()))) continue;
      const lcr::LinkParser::Frame& f = parser.frame();
      if (f.type == lcr::kLinkPacket) {
        queue.push(f.data, f.len);  // se la coda e' piena il pacchetto si perde e si conta
      } else if (f.type == lcr::kLinkHeartbeat && f.len >= 4) {
        rxCount = (static_cast<uint32_t>(f.data[0]) << 24) | (static_cast<uint32_t>(f.data[1]) << 16) | (static_cast<uint32_t>(f.data[2]) << 8) | f.data[3];
        lastHbMs = now;
      }
    }

    // 2. trasmissione di un pacchetto, se la pausa minima e' trascorsa
    uint8_t pkt[lcr::kMaxWireBytes];
    size_t len = 0;
    if (mayTransmit && queue.next(now, pkt, sizeof(pkt), len)) {
      radio.send(pkt, len);  // bloccante: dura il tempo in aria del pacchetto
      queue.sent(millis());
      transmitted++;
    } else if (!mayTransmit && queue.next(now, pkt, sizeof(pkt), len)) {
      queue.sent(now);  // senza nominativo si scarta tutto
    }

    // 3. stato verso la RX, una volta al secondo
    if (now - lastStatusMs >= kHeartbeatMs) {
      lastStatusMs = now;
      const uint32_t d = queue.dropped();
      uint8_t st[2] = {static_cast<uint8_t>(queue.size()), static_cast<uint8_t>(d > 255 ? 255 : d)};
      sendFrame(lcr::kLinkStatus, st, sizeof(st));
    }

    // 4. display
    if (now - lastDispMs >= kDisplayMs) {
      lastDispMs = now;
      char l1[24], l2[24], l3[24], l4[24], l5[24];
      snprintf(l1, sizeof(l1), "RIPETITORE  TX");
      snprintf(l2, sizeof(l2), "Out %.3f MHz %s", static_cast<double>(out.freqMhz()), prof.code);
      snprintf(l3, sizeof(l3), "Trasm %lu  Coda %u", static_cast<unsigned long>(transmitted), static_cast<unsigned>(queue.size()));
      snprintf(l4, sizeof(l4), "Persi %lu  da RX %lu", static_cast<unsigned long>(queue.dropped()), static_cast<unsigned long>(rxCount));
      const bool linkOk = lastHbMs != 0 && now - lastHbMs < kLinkDeadMs;
      snprintf(l5, sizeof(l5), mayTransmit ? "RX %s" : "N0CALL: non trasmette", linkOk ? "ok" : "NON RISPONDE");
      ui.showLines(l1, l2, l3, l4, l5);
    }

    if (usb.poll()) restartNow(ui);
    delay(1);
  }
}

}  // namespace

[[noreturn]] void runRepeaterRole(const lcr::Settings& settings, UiNotifier& ui, std::function<bool(const char*, char*, size_t)> handler) {
  ui.showMessage("RIPETITORE", settings.role == lcr::kRoleRepeaterRx ? "scheda RX" : "scheda TX");
  Serial.printf("[ripetitore] ruolo %s (antenna collegata?)\n", lcr::roleCode(settings.role));
  delay(1500);
  if (settings.role == lcr::kRoleRepeaterRx) runRx(settings, ui, handler);
  runTx(settings, ui, handler);
}
