// Ruoli del ripetitore a due schede (docs/studio-ripetitore.md, opzione A): la scheda RX ascolta la frequenza di ingresso e
// manda i pacchetti da ripetere alla scheda TX, che li trasmette sulla frequenza di uscita. Nessun menu; l'audio solo sulla scheda RX con il monitor.
// Solo per ESP32 (Arduino): non e' compilato nei test sul PC; la logica sta in lib/voicecore/repeater*.{h,cpp}.
#pragma once

#include <functional>

#include "audio_io.h"
#include "interfaces.h"
#include "settings.h"
#include "ui.h"

// Esegue il ruolo scelto in settings.role (RX o TX). Non ritorna mai: si esce con CFG REBOOT dalla seriale USB dopo aver
// cambiato il ruolo (CFG SET ROLE OFF). handler esegue una riga CFG come la console e ritorna true se bisogna riavviare.
// Con audio e codec (scheda RX con CFG SET MONITOR ON) la RX riproduce sull'altoparlante la voce che riceve: vedi
// lib/voicecore/rx_monitor.h. Senza (nullptr) nessun audio, come prima.
[[noreturn]] void runRepeaterRole(const lcr::Settings& settings, UiNotifier& ui,
                                  std::function<bool(const char* line, char* reply, size_t cap)> handler,
                                  AudioIo* audio = nullptr, lcr::ICodec* codec = nullptr);
