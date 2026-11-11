// Menu a bordo per cambiare modalita' con i due tasti della scheda (bozza in docs/roadmap.md, "Cambio di modalita' a bordo").
//
//   volume, pressione lunga (~2 s), a scheda ferma  ->  apre il menu
//   nel menu:  PTT, pressione breve    voce successiva (le voci non disponibili si saltano)
//              volume, pressione breve conferma
//              volume, pressione lunga esce senza cambiare
//              10 s senza toccare niente: esce senza cambiare
//   fuori dal menu:  PTT trasmette come sempre; volume, pressione breve, alza il volume
//
// Nel menu il PTT non trasmette mai. Indipendente dalla scheda e testato sul PC (tests/test_menu.cpp).
#pragma once

#include <cstddef>
#include <cstdint>

#include "button_gesture.h"
#include "ptt_button.h"

namespace lcr {

constexpr uint32_t kMenuTimeoutMs = 10000;
// Identificatore della voce "Wi-Fi" (configurazione dalla pagina interna): non e' un profilo, i profili vanno da 0.
constexpr uint8_t kMenuWifiId = 100;
// Voce "Invia testo": apre la scelta di uno dei testi predefiniti (text_picker.h). Non e' un profilo.
constexpr uint8_t kMenuTextId = 101;
// Voce "MeshCom": invia un testo sulle reti di testo e ascolta per un po' (escursione, docs/meshcom-testo.md). Non e' un profilo.
constexpr uint8_t kMenuMeshId = 102;

struct MenuItem {
  const char* label;  // testo sul display (al massimo 10 caratteri a testo grande)
  uint8_t id;         // per ora l'identificatore del profilo (profile.h)
  bool enabled;       // false: si vede ma non si puo' scegliere (per esempio il profilo Qualita' prima che esista)
};

// Le voci del menu: Standard, Messaggio, Qualita', Invia testo, MeshCom (escursione) e Wi-Fi (configurazione).
const MenuItem* defaultMenuItems(size_t& count);

// Segnali acustici suggeriti per ogni azione (la scheda senza display si guida con quelli).
enum class MenuBeep { None, Open, Next, Confirm, Cancel };

class Menu {
 public:
  struct Outcome {
    enum class Kind { None, Selected, Closed } kind = Kind::None;  // Selected: scelta una voce diversa da quella in uso
    uint8_t id = 0;
  };

  Menu(const MenuItem* items, size_t count) : items_(items), count_(count) {}

  bool isOpen() const { return open_; }
  size_t count() const { return count_; }
  const MenuItem& item(size_t i) const { return items_[i]; }
  size_t selectedIndex() const { return selected_; }

  // Apre il menu con evidenziata la voce in uso (currentId). Non apre un menu senza voci scelte possibili.
  bool open(uint8_t currentId, uint32_t nowMs);
  // Voce successiva tra quelle disponibili (con giro dall'ultima alla prima).
  void next(uint32_t nowMs);
  // Conferma: Selected se la voce e' diversa da quella in uso, altrimenti Closed (niente da cambiare). Chiude il menu.
  Outcome confirm(uint32_t nowMs);
  // Esce senza cambiare.
  void cancel();
  // Da chiamare spesso: chiude il menu dopo kMenuTimeoutMs senza tasti. true se l'ha appena chiuso.
  bool tick(uint32_t nowMs);

 private:
  const MenuItem* items_;
  size_t count_;
  bool open_ = false;
  size_t selected_ = 0;
  uint8_t current_ = 0;
  uint32_t lastActivityMs_ = 0;
};

// Instrada gli eventi dei due tasti tra il menu e il resto del firmware.
class MenuFlow {
 public:
  struct Actions {
    bool pttToNode = false;  // l'evento del PTT va al nodo (menu chiuso)
    bool volumeUp = false;   // pressione breve del volume fuori dal menu: alza il volume
    bool redraw = false;     // il menu e' stato aperto, chiuso o cambiato: ridisegnare lo schermo
    MenuBeep beep = MenuBeep::None;
    Menu::Outcome outcome;   // scelta confermata (Selected: salvare e riavviare)
  };

  explicit MenuFlow(Menu& menu) : menu_(menu) {}

  // ptt: evento antirimbalzato del PTT; vol: gesto del volume; nodeIdle: la scheda e' a riposo (ne' ricezione ne'
  // trasmissione); currentId: voce in uso. Ritorna cosa fare.
  Actions update(PttButton::Event ptt, ButtonGesture::Event vol, bool nodeIdle, uint8_t currentId, uint32_t nowMs);

 private:
  Menu& menu_;
  bool pttSwallowed_ = false;  // il PTT premuto nel menu: il suo rilascio non deve arrivare al nodo
};

}  // namespace lcr
