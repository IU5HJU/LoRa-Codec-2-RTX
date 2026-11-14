// Configurazione dalla pagina interna, avviata all'accensione prima di Codec2, audio e radio (come il Tracker di Guzman): la scheda crea un proprio Wi-Fi (VoLoRa-XXXX, password casuale mostrata sul
// display) e serve su 192.168.4.1 la stessa configurazione che si fa dalla seriale. Con il Wi-Fi attivo la scheda non
// trasmette ne' riceve: il resto del firmware non gira (vedi main.cpp). Si esce riavviando.
// Server: ESPAsyncWebServer, come il Tracker LoRa APRS di Guzman.
// Solo per ESP32: non e' compilato nei test sul PC.
#pragma once

#include <functional>

class WifiConfig {
 public:
  // Esegue una riga CFG e scrive la risposta; ritorna true se dopo la risposta la scheda deve riavviarsi.
  using Handler = std::function<bool(const char* line, char* reply, size_t cap)>;

  // Avvia punto di accesso e server. ssid/password (almeno 20 caratteri di spazio) sono scritti per il display.
  // false se il Wi-Fi non parte.
  bool begin(Handler handler, char* ssid, size_t ssidCap, char* password, size_t passCap);
  // Da chiamare spesso. true se e' stato chiesto il riavvio.
  // Pagina di gestione sulla rete di casa (la scheda e' un client Wi-Fi): stesse pagine, ma con utente 'volora' e la password della scheda
  // (quella mostrata sul display all'accensione). La password e' scritta in 'password' (almeno 16 caratteri). false se non parte.
  bool beginLan(Handler handler, char* password, size_t passCap);
  bool loop();
  bool active() const { return active_; }
  // Chiude server e punto di accesso e spegne il Wi-Fi (la scheda puo' poi usarlo come client). Libera la memoria del server.
  void end();
  // Indicatore salvato nella scheda: la voce "Wi-Fi" del menu lo accende e riavvia; all'avvio si controlla e si cancella.
  static bool bootRequest();
  static void setBootRequest(bool on);

 private:
  bool active_ = false;
  bool reboot_ = false;
  uint32_t rebootAtMs_ = 0;
};
