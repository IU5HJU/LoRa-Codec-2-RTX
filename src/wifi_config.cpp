#include "wifi_config.h"

#include <Arduino.h>
#include <WebServer.h>
#include <Preferences.h>
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
  // Password casuale di 8 cifre (esp_random usa l'hardware), generata la prima volta e poi salvata nella scheda: resta
  // la stessa a ogni avvio del Wi-Fi (si rigenera solo con i valori di fabbrica). Non esiste una password uguale per
  // tutte le schede.
  {
    Preferences pref;
    String saved;
    if (pref.begin("lcr", true)) {
      saved = pref.getString("wpass", "");
      pref.end();
    }
    if (saved.length() == 8) {
      snprintf(password, passCap, "%s", saved.c_str());
    } else {
      snprintf(password, passCap, "%08lu", static_cast<unsigned long>(esp_random() % 100000000UL));
      if (pref.begin("lcr", false)) {
        pref.putString("wpass", password);
        pref.end();
      }
    }
  }

  // Come nel Tracker LoRa APRS di Guzman (wifi_utils.cpp, startAutoAP): modo NULL, poi AP, softAP(nome, password) con i
  // valori predefiniti, nessun indirizzo configurato a mano e nessun DNS. Le righe [wifi] sulla seriale dicono se il
  // client si collega, che indirizzo riceve e se la richiesta della pagina arriva.
  WiFi.mode(WIFI_MODE_NULL);
  WiFi.mode(WIFI_AP);
  uint8_t mac[6] = {0};
  WiFi.macAddress(mac);
  snprintf(ssid, ssidCap, "LoRaCodec2-%02X%02X", mac[4], mac[5]);
  WiFi.onEvent([](WiFiEvent_t ev, WiFiEventInfo_t info) {
    if (ev == ARDUINO_EVENT_WIFI_AP_STACONNECTED) Serial.println("[wifi] un client si e' collegato alla rete");
    if (ev == ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED) Serial.printf("[wifi] indirizzo assegnato al client: %s\n", IPAddress(info.wifi_ap_staipassigned.ip.addr).toString().c_str());
    if (ev == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) Serial.println("[wifi] il client si e' scollegato");
  });
  if (!WiFi.softAP(ssid, password)) {
    WiFi.mode(WIFI_OFF);
    return false;
  }
  server = new WebServer(80);
  server->on("/", HTTP_GET, []() { Serial.println("[wifi] richiesta della pagina"); server->send_P(200, "text/html; charset=utf-8", kWifiPage); });
  server->on("/cfg", HTTP_GET, []() {
    String line = server->arg("c");
    Serial.printf("[wifi] comando dalla pagina: %s\n", line.c_str());
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
  server->onNotFound([]() { Serial.println("[wifi] richiesta non trovata"); server->send(404, "text/plain; charset=utf-8", "non trovato"); });
  server->begin();
  Serial.printf("[wifi] punto di accesso %s, indirizzo %s\n", ssid, WiFi.softAPIP().toString().c_str());
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
