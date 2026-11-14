#include "menu.h"

namespace lcr {

const MenuItem* defaultMenuItems(size_t& count, bool sysopMode) {
  static const MenuItem kItems[] = {
      // "Modo ..." davanti ai tre profili: Standard (Codec2 3200, id 2), Range (Codec2 1200, id 0), Messaggio (id 1). Dal 12/11/2026; prima
      // erano Standard (1200), Messaggio e Qualita' (3200).
      {"Modo Standard", 2, true},
      {"Modo Range", 0, true},
      {"Modo Messaggio", 1, true},
      {"Invia testo", kMenuTextId, true},
      {"MeshCom", kMenuMeshId, true},
      {"MeshCom ascolto", kMenuMeshListenId, true},
      {"Wi-Fi", kMenuWifiId, true},
  };
  static const MenuItem kSysopItems[] = {
      {"Modo Standard", 2, true},
      {"Modo Range", 0, true},
      {"Modo Messaggio", 1, true},
      {"Comandi Sysop", kMenuSysopId, true},
      {"MeshCom", kMenuMeshId, true},
      {"MeshCom ascolto", kMenuMeshListenId, true},
      {"Wi-Fi", kMenuWifiId, true},
  };
  count = sizeof(kItems) / sizeof(kItems[0]);
  return sysopMode ? kSysopItems : kItems;
}

bool Menu::open(uint8_t currentId, uint32_t nowMs) {
  size_t first = count_;
  size_t cur = count_;
  for (size_t i = 0; i < count_; i++) {
    if (!items_[i].enabled) continue;
    if (first == count_) first = i;
    if (items_[i].id == currentId) cur = i;
  }
  if (first == count_) return false;  // nessuna voce scegliibile
  open_ = true;
  current_ = currentId;
  selected_ = cur != count_ ? cur : first;
  lastActivityMs_ = nowMs;
  return true;
}

void Menu::next(uint32_t nowMs) {
  if (!open_) return;
  lastActivityMs_ = nowMs;
  for (size_t step = 1; step <= count_; step++) {
    const size_t i = (selected_ + step) % count_;
    if (items_[i].enabled) {
      selected_ = i;
      return;
    }
  }
}

Menu::Outcome Menu::confirm(uint32_t nowMs) {
  Outcome o;
  if (!open_) return o;
  (void)nowMs;
  open_ = false;
  const MenuItem& it = items_[selected_];
  if (it.id != current_) {
    o.kind = Outcome::Kind::Selected;
    o.id = it.id;
  } else {
    o.kind = Outcome::Kind::Closed;
    o.id = it.id;
  }
  return o;
}

void Menu::cancel() { open_ = false; }

bool Menu::tick(uint32_t nowMs) {
  if (open_ && static_cast<int32_t>(nowMs - lastActivityMs_) >= static_cast<int32_t>(kMenuTimeoutMs)) {
    open_ = false;
    return true;
  }
  return false;
}

MenuFlow::Actions MenuFlow::update(PttButton::Event ptt, ButtonGesture::Event vol, bool nodeIdle, uint8_t currentId,
                                   uint32_t nowMs) {
  Actions a;
  if (menu_.isOpen()) {
    // Il menu si chiude da solo se la scheda non e' piu' a riposo (e' arrivata una ricezione) o dopo il tempo massimo.
    if (!nodeIdle) {
      menu_.cancel();
      a.redraw = true;
      a.beep = MenuBeep::Cancel;
      // Eventi di questo giro: il PTT va al nodo solo se non e' la fine di una pressione fatta nel menu.
    } else if (menu_.tick(nowMs)) {
      a.redraw = true;
      a.beep = MenuBeep::Cancel;
    }
  }

  if (menu_.isOpen()) {
    if (ptt == PttButton::Event::Pressed) {
      menu_.next(nowMs);
      pttSwallowed_ = true;
      a.redraw = true;
      a.beep = MenuBeep::Next;
    } else if (ptt == PttButton::Event::Released && pttSwallowed_) {
      pttSwallowed_ = false;  // il rilascio di una pressione fatta nel menu non e' per il nodo
    }
    if (vol == ButtonGesture::Event::Short) {
      a.outcome = menu_.confirm(nowMs);
      a.redraw = true;
      a.beep = MenuBeep::Confirm;
    } else if (vol == ButtonGesture::Event::Long) {
      menu_.cancel();
      a.redraw = true;
      a.beep = MenuBeep::Cancel;
    }
    return a;
  }

  // Menu chiuso
  if (ptt == PttButton::Event::Released && pttSwallowed_) {
    pttSwallowed_ = false;  // il PTT premuto nel menu e rilasciato dopo la chiusura: il nodo non ne sa niente
  } else if (ptt != PttButton::Event::None) {
    a.pttToNode = true;
  }
  if (vol == ButtonGesture::Event::Short) {
    a.volumeUp = true;
  } else if (vol == ButtonGesture::Event::Long && nodeIdle && menu_.open(currentId, nowMs)) {
    a.redraw = true;
    a.beep = MenuBeep::Open;
  }
  return a;
}

}  // namespace lcr
