# Copie di sicurezza di librerie esterne

Questa cartella **non fa parte della compilazione**: PlatformIO legge solo `src/` e `lib/`. Le copie servono a non dipendere da un servizio esterno se una libreria venisse tolta o cambiata.

## esp32_codec2-1.0.7

- **Origine:** <https://github.com/sh123/esp32_codec2_arduino>, tag `1.0.7`, commit `15a27e1e7531f7f3f1c82deadbef6c4372f41d76` (06/01/2026). È la versione fissata in `platformio.ini` (`sh123/esp32_codec2 @ 1.0.7`).
- **Cos'è:** il Codec2 di David Rowe (<https://github.com/drowe67/codec2>) impacchettato per l'ESP32. Contiene i modi 3200, 2400, 1600, 1400, 1300, 1200, 700C, 450 e 450PWB, tutti attivi. Il suo README dice che si basa sull'ultima versione stabile del repository `codec2-dev` e che la versione di sviluppo di Codec2 "may stop supporting some of low bit rate modes in favor of RADEV1": è una ragione in più per tenere questa copia.
- **Copia esatta del tag**, estratta con `git archive` (senza la cartella `.git`): 164 file, 4,2 MB. `SHA256SUMS` contiene le somme di controllo; per verificare, dalla cartella `esp32_codec2-1.0.7`: `sha256sum -c SHA256SUMS`.
- **Come usarla se serve:** in `platformio.ini` sostituire `sh123/esp32_codec2 @ 1.0.7` con `file://third_party/esp32_codec2-1.0.7` (oppure copiarla in `lib/`). Oggi non è usata.

### Licenza (letta dalle intestazioni dei file, da confermare)

- Il pacchetto `esp32_codec2` ha un file `LICENSE` con la **GPL 3**, come questo progetto.
- Il codice di Codec2 è per lo più sotto **LGPL 2.1**: circa 70 file dicono "GNU Lesser General Public License version 2.1, as published by the Free Software Foundation", quasi sempre **senza** la formula "o una versione successiva"; pochi file (per esempio `varicode.c`) nominano la GPL; molte tabelle di dati (circa 75 file) non hanno nessuna intestazione.
- La LGPL 2.1 (sezione 3) permette di applicare a una copia le condizioni della GPL, e il firmware qui è distribuito sotto GPL 3 con i sorgenti: la ridistribuzione con tutte le note originali al loro posto sembra permessa. **Non è un parere legale**: prima di una versione stabile conviene una verifica più attenta (file per file, soprattutto quelli con la sola GPL 2.1).
- Le note di copyright e le licenze originali sono rimaste dove erano; questa copia non le modifica.
