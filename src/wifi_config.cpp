#include "wifi_config.h"

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_random.h>

#include "wifi_page.h"

namespace {
AsyncWebServer* server = nullptr;
WifiConfig::Handler handler;
volatile bool rebootRequested = false;
bool lanAuth = false;  // true: pagine dalla rete di casa, con utente e password
char lanPass[16] = {0};
}  // namespace

bool WifiConfig::bootRequest() {
  Preferences pref;
  bool on = false;
  if (pref.begin("lcr", true)) {
    on = pref.getBool("wifiboot", false);
    pref.end();
  }
  return on;
}

void WifiConfig::setBootRequest(bool on) {
  Preferences pref;
  if (pref.begin("lcr", false)) {
    pref.putBool("wifiboot", on);
    pref.end();
  }
}

// Le due pagine (la configurazione e il comando CFG). Dalla rete di casa (lanAuth) servono utente e password: la rete non e' protetta come il
// punto di accesso della scheda.
static void addRoutes() {
  server->on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (lanAuth && !request->authenticate("volora", lanPass)) {
      request->requestAuthentication();
      return;
    }
    Serial.println("[wifi] richiesta della pagina");
    // Come nel Tracker di Guzman: risposta dalla flash (senza copiare la pagina nella memoria), con tipo e lunghezza.
    request->send(request->beginResponse(200, "text/html", reinterpret_cast<const uint8_t*>(kWifiPage), sizeof(kWifiPage) - 1));
  });
  server->on("/cfg", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (lanAuth && !request->authenticate("volora", lanPass)) {
      request->requestAuthentication();
      return;
    }
    const String line = request->hasParam("c") ? request->getParam("c")->value() : String();
    Serial.printf("[wifi] comando dalla pagina: %s\n", line.c_str());
    // Solo comandi CFG; quelli che usano audio o radio non ci sono (la scheda e' ferma).
    if (line.length() == 0 || line.length() > 200 || !line.startsWith("CFG ")) {
      request->send(400, "text/plain; charset=utf-8", "CFG ERR comando non valido");
      return;
    }
    if (line.startsWith("CFG TEST") || line.startsWith("CFG MIC") || line.startsWith("CFG REC")) {
      request->send(200, "text/plain; charset=utf-8", "CFG ERR non disponibile con il Wi-Fi");
      return;
    }
    char reply[260];
    reply[0] = 0;
    const bool reboot = handler(line.c_str(), reply, sizeof(reply));
    request->send(200, "text/plain; charset=utf-8", reply);
    if (reboot) rebootRequested = true;
  });
  server->onNotFound([](AsyncWebServerRequest* request) {
    Serial.println("[wifi] richiesta non trovata");
    request->send(404, "text/plain; charset=utf-8", "non trovato");
  });
}

// Password casuale di 8 cifre (esp_random usa l'hardware), generata la prima volta e poi salvata nella scheda: resta la stessa a ogni avvio
// del Wi-Fi (si rigenera solo con i valori di fabbrica). Non esiste una password uguale per tutte le schede.
static void ensurePassword(char* password, size_t passCap) {
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

bool WifiConfig::begin(Handler h, char* ssid, size_t ssidCap, char* password, size_t passCap) {
  handler = h;
  ensurePassword(password, passCap);

  // Come nel Tracker LoRa APRS di Guzman (wifi_utils.cpp, startAutoAP): modo NULL, poi AP, softAP(nome, password) con i
  // valori predefiniti, nessun indirizzo configurato a mano e nessun DNS. Le righe [wifi] sulla seriale dicono se il
  // client si collega, che indirizzo riceve e se la richiesta della pagina arriva.
  WiFi.mode(WIFI_MODE_NULL);
  WiFi.mode(WIFI_AP);
  uint8_t mac[6] = {0};
  WiFi.macAddress(mac);
  snprintf(ssid, ssidCap, "VoLoRa-%02X%02X", mac[4], mac[5]);
  WiFi.onEvent([](WiFiEvent_t ev, WiFiEventInfo_t info) {
    if (ev == ARDUINO_EVENT_WIFI_AP_STACONNECTED) Serial.println("[wifi] un client si e' collegato alla rete");
    if (ev == ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED) Serial.printf("[wifi] indirizzo assegnato al client: %s\n", IPAddress(info.wifi_ap_staipassigned.ip.addr).toString().c_str());
    if (ev == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) Serial.println("[wifi] il client si e' scollegato");
  });
  if (!WiFi.softAP(ssid, password)) {
    WiFi.mode(WIFI_OFF);
    return false;
  }
  // Server asincrono come nel Tracker LoRa APRS di Guzman (ESPAsyncWebServer): le richieste si servono nel task di rete e
  // il loop principale non resta bloccato. Il server sincrono (WebServer) dava una pagina bianca e poi non rispondeva piu'.
  server = new AsyncWebServer(80);
  addRoutes();
  server->begin();
  Serial.printf("[wifi] punto di accesso %s, indirizzo %s\n", ssid, WiFi.softAPIP().toString().c_str());
  active_ = true;
  return true;
}

bool WifiConfig::beginLan(Handler h, char* password, size_t passCap) {
  if (active_) return true;
  handler = h;
  ensurePassword(password, passCap);
  snprintf(lanPass, sizeof(lanPass), "%s", password);
  lanAuth = true;
  server = new AsyncWebServer(80);
  addRoutes();
  server->begin();
  Serial.printf("[wifi] pagina di gestione sulla rete di casa: http://%s/ (utente volora)\n", WiFi.localIP().toString().c_str());
  active_ = true;
  return true;
}

void WifiConfig::end() {
  if (!active_) return;
  if (server) {
    server->end();
    delete server;
    server = nullptr;
  }
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  active_ = false;
  reboot_ = false;
  rebootRequested = false;
  lanAuth = false;
}

bool WifiConfig::loop() {
  if (!active_) return false;
  if (rebootRequested && !reboot_) {
    reboot_ = true;
    rebootAtMs_ = millis() + 600;  // il tempo di far arrivare la risposta alla pagina
  }
  return reboot_ && static_cast<int32_t>(millis() - rebootAtMs_) >= 0;
}
