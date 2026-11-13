#include "net_udp.h"

#include <WiFi.h>
#include <WiFiUdp.h>

#include "peer_resolve.h"

void NetUdp::begin(uint16_t localPort, const char* host, uint16_t peerPort) {
  localPort_ = localPort;
  peerPort_ = peerPort;
  strncpy(host_, host ? host : "", sizeof(host_) - 1);
  rx_ = xQueueCreate(16, sizeof(Dgram));
  tx_ = xQueueCreate(16, sizeof(Dgram));
  xTaskCreatePinnedToCore(taskEntry, "netudp", 6144, this, 2, nullptr, 0);
}

void NetUdp::sendDatagram(const uint8_t* data, size_t len) {
  if (!tx_ || len == 0 || len > sizeof(Dgram::data)) return;
  Dgram d;
  d.len = static_cast<uint16_t>(len);
  memcpy(d.data, data, len);
  xQueueSend(tx_, &d, 0);  // coda piena: il datagramma si perde (UDP)
}

bool NetUdp::receive(uint8_t* out, size_t cap, size_t& len) {
  Dgram d;
  if (!rx_ || xQueueReceive(rx_, &d, 0) != pdTRUE) return false;
  len = d.len < cap ? d.len : cap;
  memcpy(out, d.data, len);
  return true;
}

// Il solo task che tocca il socket: lo apre al ritorno del Wi-Fi e lo richiude alla perdita, risolve il nome, spedisce quello che e' in coda e
// legge quello che arriva (il ciclo principale puo' restare bloccato per la durata di un pacchetto in radio).
void NetUdp::run() {
  WiFiUDP udp;
  bool open = false;
  lcr::PeerResolveSchedule sched(host_);
  if (sched.literal()) {
    IPAddress ip;
    if (ip.fromString(host_)) peerAddr_ = static_cast<uint32_t>(ip);
  }
  for (;;) {
    const bool up = WiFi.status() == WL_CONNECTED;
    if (up && !open) open = udp.begin(localPort_) != 0;
    if (!up && open) {
      udp.stop();
      open = false;
    }
    const uint32_t now = millis();
    if (sched.due(now, up, linkUp_)) {
      IPAddress ip;
      const bool ok = WiFi.hostByName(host_, ip) == 1;  // puo' durare qualche secondo: per questo e' qui e non nel ciclo principale
      sched.done(ok, millis());
      if (ok) {
        const uint32_t a = static_cast<uint32_t>(ip);
        if (a != peerAddr_) {
          peerAddr_ = a;
          resolved_ = resolved_ + 1;
          Serial.printf("[rete] %s = %s\n", host_, ip.toString().c_str());
        }
      } else {
        Serial.printf("[rete] nome %s non risolto, riprovo\n", host_);
      }
    }
    // spedisce dallo stesso socket da cui riceve
    Dgram d;
    for (int i = 0; i < 4 && open && peerAddr_ != 0 && xQueueReceive(tx_, &d, 0) == pdTRUE; i++) {
      const uint32_t dest = peerAddr_;
      if (udp.beginPacket(IPAddress(dest), peerPort_)) {
        udp.write(d.data, d.len);
        udp.endPacket();
      }
    }
    int n = open ? udp.parsePacket() : 0;
    if (n > 0) {
      if (static_cast<size_t>(n) <= sizeof(d.data)) {
        d.len = static_cast<uint16_t>(udp.read(d.data, sizeof(d.data)));
        if (d.len > 0) xQueueSend(rx_, &d, 0);  // coda piena: il datagramma si perde (UDP)
      } else {
        udp.flush();
      }
    } else {
      vTaskDelay(pdMS_TO_TICKS(2));
    }
  }
}
