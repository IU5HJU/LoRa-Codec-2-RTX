#include "wifi_config.h"

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_random.h>

#include "wifi_page.h"

namespace {
WebServer* server = nullptr;
DNSServer* dns = nullptr;  // portale captive: ogni nome risolve la scheda (192.168.4.1)
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
  // Portale captive: il telefono controlla se la rete ha Internet con indirizzi come /generate_204 (Android) o
  // /hotspot-detect.html (iOS). Rispondendo con un rinvio alla pagina, il telefono apre da solo la configurazione
  // invece di dichiarare "nessun Internet" e usare i dati mobili (il motivo per cui la pagina non si caricava).
  server->onNotFound([]() {
    server->sendHeader("Location", "http://192.168.4.1/", true);
    server->send(302, "text/plain; charset=utf-8", "");
  });
  dns = new DNSServer();
  dns->start(53, "*", WiFi.softAPIP());
  server->begin();
  active_ = true;
  return true;
}

bool WifiConfig::loop() {
  if (!active_) return false;
  dns->processNextRequest();
  server->handleClient();
  if (rebootRequested && !reboot_) {
    reboot_ = true;
    rebootAtMs_ = millis() + 600;  // il tempo di far arrivare la risposta alla pagina
  }
  return reboot_ && static_cast<int32_t>(millis() - rebootAtMs_) >= 0;
}
