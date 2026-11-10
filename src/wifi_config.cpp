#include "wifi_config.h"

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_random.h>

#include "wifi_page.h"

namespace {
WebServer* server = nullptr;
WifiConfig::Handler handler;
bool rebootRequested = false;
}  // namespace

bool WifiConfig::begin(Handler h, char* ssid, size_t ssidCap, char* password, size_t passCap) {
  handler = h;
  uint8_t mac[6] = {0};
  WiFi.macAddress(mac);
  snprintf(ssid, ssidCap, "LoRaCodec2-%02X%02X", mac[4], mac[5]);
  // Password casuale di 8 cifre (esp_random usa l'hardware): non esiste una password uguale per tutte le schede.
  snprintf(password, passCap, "%08lu", static_cast<unsigned long>(esp_random() % 100000000UL));

  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(ssid, password, 1, 0, 2)) {  // canale 1, non nascosta, al massimo 2 client
    WiFi.mode(WIFI_OFF);
    return false;
  }
  server = new WebServer(80);
  server->on("/", HTTP_GET, []() { server->send_P(200, "text/html; charset=utf-8", kWifiPage); });
  server->on("/cfg", HTTP_GET, []() {
    String line = server->arg("c");
    char reply[260];
    reply[0] = 0;
    // Solo comandi CFG: il resto non ha senso da qui. I comandi che usano audio o radio non ci sono (la scheda e' ferma).
    if (line.length() == 0 || line.length() > 90 || !line.startsWith("CFG ")) {
      server->send(400, "text/plain; charset=utf-8", "CFG ERR comando non valido");
      return;
    }
    if (line.startsWith("CFG TEST") || line.startsWith("CFG MIC") || line.startsWith("CFG REC")) {
      server->send(200, "text/plain; charset=utf-8", "CFG ERR non disponibile con il Wi-Fi");
      return;
    }
    const bool reboot = handler(line.c_str(), reply, sizeof(reply));
    server->send(200, "text/plain; charset=utf-8", reply);
    if (reboot) rebootRequested = true;
  });
  server->onNotFound([]() { server->send(404, "text/plain; charset=utf-8", "non trovato"); });
  server->begin();
  active_ = true;
  return true;
}

bool WifiConfig::loop() {
  if (!active_) return false;
  server->handleClient();
  if (rebootRequested && !reboot_) {
    reboot_ = true;
    rebootAtMs_ = millis() + 600;  // il tempo di far arrivare la risposta alla pagina
  }
  return reboot_ && static_cast<int32_t>(millis() - rebootAtMs_) >= 0;
}
