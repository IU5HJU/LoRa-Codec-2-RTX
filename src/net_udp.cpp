#include "net_udp.h"

#include <WiFi.h>
#include <WiFiUdp.h>

#include "peer_resolve.h"
#include "user_config.h"

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

// Chiede l'indirizzo pubblico a un servizio esterno: una GET in HTTP semplice (porta 80) che risponde con il solo indirizzo, per esempio
// "ifconfig.me/ip". Il servizio vede solo l'indirizzo da cui la scheda lo chiede, che e' quello che vuole sapere. Se non risponde, o risponde altro,
// resta il valore di prima. Con LCR_PUBLIC_IP_HOST vuoto la funzione e' spenta e la scheda non contatta nessuno.
void NetUdp::fetchPublicAddress() {
  const char* host = LCR_PUBLIC_IP_HOST;
  if (host[0] == 0) return;
  WiFiClient c;
  if (!c.connect(host, 80, 4000)) {
    Serial.printf("[rete] indirizzo pubblico: %s non raggiungibile\n", host);
    return;
  }
  c.printf("GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: curl/8\r\nConnection: close\r\n\r\n", LCR_PUBLIC_IP_PATH, host);
  char buf[400];
  size_t n = 0;
  const uint32_t t0 = millis();
  while ((c.connected() || c.available()) && millis() - t0 < 4000 && n < sizeof(buf) - 1) {
    if (c.available()) buf[n++] = static_cast<char>(c.read());
    else vTaskDelay(pdMS_TO_TICKS(5));
  }
  c.stop();
  buf[n] = 0;
  const char* body = strstr(buf, "\r\n\r\n");
  if (!body) return;
  body += 4;
  char ipText[20];
  size_t k = 0;
  while (*body && k < sizeof(ipText) - 1 && ((*body >= '0' && *body <= '9') || *body == '.')) ipText[k++] = *body++;
  ipText[k] = 0;
  IPAddress ip;
  if (k < 7 || !ip.fromString(ipText)) {
    Serial.println("[rete] indirizzo pubblico: risposta non valida");
    return;
  }
  const uint32_t a = (static_cast<uint32_t>(ip[0]) << 24) | (static_cast<uint32_t>(ip[1]) << 16) | (static_cast<uint32_t>(ip[2]) << 8) | ip[3];
  if (a != publicAddr_) Serial.printf("[rete] indirizzo pubblico: %s\n", ipText);
  publicAddr_ = a;
}

// La richiesta puo' durare qualche secondo (connessione e risposta): gira in un task a parte, che finisce da solo, cosi' il task della rete
// continua a passare i pacchetti.
void NetUdp::fetchEntry(void* self) {
  NetUdp* n = static_cast<NetUdp*>(self);
  n->fetchPublicAddress();
  n->fetching_ = false;
  vTaskDelete(nullptr);
}

// Il solo task che tocca il socket: lo apre al ritorno del Wi-Fi e lo richiude alla perdita, risolve il nome, spedisce quello che e' in coda e
// legge quello che arriva (il ciclo principale puo' restare bloccato per la durata di un pacchetto in radio).
void NetUdp::run() {
  WiFiUDP udp;
  bool open = false;
  uint32_t lastPubMs = 0;
  bool havePubTry = false;
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
    // indirizzo pubblico: al primo collegamento Wi-Fi (riprovando ogni 30 s se non risponde) e poi ogni 10 minuti
    if (up) {
      const uint32_t every = publicAddr_ == 0 ? 30000 : 600000;
      if (!fetching_ && (!havePubTry || millis() - lastPubMs >= every)) {
        havePubTry = true;
        lastPubMs = millis();
        fetching_ = true;
        if (xTaskCreatePinnedToCore(fetchEntry, "pubip", 5120, this, 1, nullptr, 0) != pdPASS) fetching_ = false;
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
