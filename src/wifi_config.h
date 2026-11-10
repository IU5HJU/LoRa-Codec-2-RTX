// Configurazione dalla pagina interna: la scheda crea un proprio Wi-Fi (LoRaCodec2-XXXX, password casuale mostrata sul
// display) e serve su 192.168.4.1 la stessa configurazione che si fa dalla seriale. Con il Wi-Fi attivo la scheda non
// trasmette ne' riceve: il resto del firmware non gira (vedi main.cpp). Si esce riavviando.
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
  bool loop();
  bool active() const { return active_; }

 private:
  bool active_ = false;
  bool reboot_ = false;
  uint32_t rebootAtMs_ = 0;
};
