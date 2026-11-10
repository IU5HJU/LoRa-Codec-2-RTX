// Ruoli del ripetitore a due schede (docs/studio-ripetitore.md, opzione A): la scheda RX ascolta la frequenza di ingresso e
// manda i pacchetti da ripetere alla scheda TX, che li trasmette sulla frequenza di uscita. Nessun audio, nessun menu.
// Solo per ESP32 (Arduino): non e' compilato nei test sul PC; la logica sta in lib/voicecore/repeater*.{h,cpp}.
#pragma once

#include <functional>

#include "settings.h"
#include "ui.h"

// Esegue il ruolo scelto in settings.role (RX o TX). Non ritorna mai: si esce con CFG REBOOT dalla seriale USB dopo aver
// cambiato il ruolo (CFG SET ROLE OFF). handler esegue una riga CFG come la console e ritorna true se bisogna riavviare.
[[noreturn]] void runRepeaterRole(const lcr::Settings& settings, UiNotifier& ui,
                                  std::function<bool(const char* line, char* reply, size_t cap)> handler);
