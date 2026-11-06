# Firmware

Schede supportate (un ambiente PlatformIO e un artefatto di Actions ciascuna):

| Scheda | Ambiente | Artefatto di GitHub Actions | Stato |
|---|---|---|---|
| LILYGO LoRa32 T3 V1.6.1 | `lora32` | `firmware-t3-v161` | provata: avvio, OLED, PTT, trasmissione |
| LILYGO T-Beam V1.2 (AXP2101) | `tbeam` | `firmware-tbeam-v12` | **fase 1: provata** (avvio anche a batteria, AXP2101, radio, tasto IO38 come PTT, trasmissione e ricezione, batteria sull'OLED); audio da provare |

I pin e le differenze tra le due sono in [pinout.md](pinout.md).

> **Mai alimentare la scheda senza antenna a 433 MHz (o carico fittizio) collegata**, nemmeno
> solo per compilare o caricare il firmware via USB: l'USB alimenta il modulo radio, che senza
> carico si può danneggiare. Prima l'antenna, poi il cavo USB.

**Stato (5 novembre 2026).** Compila con PlatformIO su GitHub Actions. **Provato sulle schede** (T3 e T-Beam, senza
amplificatore né altoparlante): avvio fino a "ASCOLTO", OLED, PTT, trasmissione e ricezione nei due versi, microfono (livelli),
impostazioni salvate, pagina web di installazione e configurazione, prova di collegamento. **Ancora da provare:** altoparlante e voce a turni (manca l'hardware).
Problemi trovati e risultati sono in [prove.md](prove.md). La logica del protocollo
(`lib/voicecore/`) è testata sul PC. Il workflow `.github/workflows/build.yml` produce
`web_factory.bin` e `ota_update.bin` come artefatti di ogni esecuzione.

## File

| File | Compito |
|---|---|
| `src/main.cpp` | Avvio, pulsanti PTT e volume, ciclo principale |
| `src/radio_lora.*` | SX1278 con RadioLib: parametri LoRa, CAD, invio, ricezione (`IRadio`) |
| `src/codec2_adapter.*` | Codec2 modo 1200 bps (`ICodec`) |
| `src/audio_io.*` | I2S full duplex a 16 kHz, task di cattura, pre-roll, amplificatore, toni (`IAudioIn`/`IAudioOut`) |
| `src/ui.*` | OLED SSD1306 128x64 e segnali acustici (`INotifier`) |
| `src/board_pins.h`, `src/board_pins_t3_v161.h`, `src/board_pins_tbeam_v12.h` | Pin per scheda: `board_pins.h` sceglie il file in base al flag di compilazione |
| `src/settings_nvs.*` | Impostazioni salvate nella memoria non volatile (NVS): nominativo, frequenza, potenza, volume |
| `src/battery.*` | Tensione della batteria: AXP2101 sulla T-Beam, ADC su GPIO35 sulla T3 |
| `lib/voicecore/settings.*` | Validazione delle impostazioni e comandi `CFG` (testati sul PC) |
| `lib/voicecore/tone_meter.*`, `level_stats.*` | Misura di un tono (Goertzel) e statistiche di livello per le prove audio |
| `lib/voicecore/link_test.*` | Prova di collegamento: sequenza nota, confronto, esito |
| `site/index.html`, `tools/make_site.sh` | Pagina web di installazione e configurazione, pubblicata su GitHub Pages dal workflow |
| `src/pmu.*` | Alimentazione: sulla T-Beam inizializza l'AXP2101 (radio e GPS); sulla T3 non fa nulla |
| `src/user_config.h` | Valori predefiniti (nominativo N0CALL, frequenza, SF/BW, parità, AGC del microfono, volume): le impostazioni salvate nella scheda li sostituiscono |

## Compilazione

```
pio run -e lora32            # T3 V1.6.1
pio run -e tbeam             # T-Beam V1.2
pio run -e lora32 -t upload  # carica (togliere la microSD)
pio device monitor           # seriale a 115200
```

Dipendenze (in `platformio.ini`): RadioLib 7.5.0, esp32_codec2 1.0.7, Adafruit SSD1306 e GFX; per la T-Beam anche XPowersLib 0.2.6.

## Ottenere il firmware senza compilare in locale

Il workflow di GitHub Actions compila a ogni modifica. Per averlo col **proprio nominativo**:

1. Fai un **fork** del repository su GitHub.
2. Nel fork modifica `src/user_config.h` e cambia `#define LCR_CALLSIGN "N0CALL"` con il tuo nominativo (puoi farlo dal sito, con il pulsante di modifica del file). Se è la prima volta che usi Actions nel fork, abilitale dalla scheda Actions.
3. Aspetta che l'esecuzione sul tuo ramo diventi verde (circa 2-3 minuti).
4. Aprila e scarica l'artefatto della tua scheda, **`firmware-t3-v161`** (T3) oppure **`firmware-tbeam-v12`** (T-Beam). Serve essere loggati su GitHub; scade dopo circa 90 giorni. Contiene:
   - `web_factory.bin`: immagine completa, da scrivere all'indirizzo `0x0` (prima installazione);
   - `ota_update.bin`: solo l'applicazione, per gli aggiornamenti futuri;
   - `SHA256SUMS`: somme di controllo.

Il firmware di `main` ha il nominativo `N0CALL` e non trasmette: il tuo lo imposti **dalla [pagina web](https://iu5hju.github.io/LoRa-Codec-2-RTX/) o con `CFG SET CALLSIGN`, senza ricompilare**, e resta salvato nella scheda anche dopo un aggiornamento. Non serve più un ramo personale con il nominativo dentro.
Se fai un fork con un nominativo compilato in `src/user_config.h`, non usare i binari di altri radioamatori che ne contengono uno diverso dal tuo.

## Caricare il firmware sulla scheda (Linux, provato su Ubuntu 24.04)

**Prima:** antenna collegata, microSD tolta, PTT non premuto.

1. **esptool** in un ambiente separato (su Ubuntu recenti `pip install` globale è bloccato):
   ```
   sudo apt install python3-venv        # se serve il pacchetto esatto: python3.12-venv
   python3 -m venv ~/esp
   ~/esp/bin/pip install esptool==4.8.1
   ```
2. **Porta seriale.** La scheda compare come `/dev/ttyACM0` (chip USB CH9102, ID `1a86:55d4`, verificabile con `lsusb`).
   Per usarla l'utente deve stare nel gruppo `dialout`:
   ```
   sudo usermod -aG dialout $USER
   newgrp dialout          # oppure esci dalla sessione e rientra
   ```
3. **Prova di collegamento (non scrive nulla):**
   ```
   ~/esp/bin/esptool.py --chip esp32 --port /dev/ttyACM0 chip_id
   ```
   Deve riconoscere `ESP32-PICO-D4`.
4. **Scrittura** (dalla cartella dove hai decompresso lo zip e verificato `SHA256SUMS`):
   ```
   ~/esp/bin/esptool.py --chip esp32 --port /dev/ttyACM0 --baud 460800 write_flash 0x0 web_factory.bin
   ```
5. **Seriale:** `~/esp/bin/python -m serial.tools.miniterm --rts 0 --dtr 0 /dev/ttyACM0 115200` (esci con Ctrl+]).
   Dopo l'avvio compaiono le righe `[avvio] ...` e `IU5HJU pronto, 433.900 MHz, SF8 BW250`.

Il comando `chip_id` e la scrittura sono entrati in modalità di caricamento da soli, senza toccare pulsanti.

## Caricare il firmware sulla scheda (Windows)

**Prima:** antenna collegata, microSD tolta, PTT non premuto. Driver USB CH343/CH34x di WCH installato:
in Gestione dispositivi, sotto "Porte (COM e LPT)", deve comparire la porta della scheda (`COMx`).

- **Dal browser (Chrome o Edge):** apri la pagina esptool-js di Espressif, premi Connect, scegli la porta, imposta indirizzo `0x0` e il file `web_factory.bin`, poi Program.
- **Da riga di comando:**
  ```
  pip install esptool==4.8.1
  esptool.py --chip esp32 --port COMx --baud 460800 write_flash 0x0 web_factory.bin
  ```
- Se la scheda non entra in modalità di scrittura, tieni premuto **RST** all'avvio del comando e rilascia dopo la connessione.

## Pagina web di installazione e configurazione

La pagina in `site/index.html` installa il firmware (esp-web-tools) e imposta nominativo, frequenza, potenza, volume e fondo del microfono con i
comandi `CFG` della sezione seguente, tutto dal browser (Chrome, Edge od Opera, cavo USB). Il workflow
`.github/workflows/build.yml` la pubblica su **GitHub Pages** a ogni push su `main` e a ogni tag `v*`, insieme ai
binari appena compilati (`tools/make_site.sh`). Una volta sola, nel repository: **Settings -> Pages -> Source: GitHub Actions**.
L'indirizzo sarà `https://iu5hju.github.io/LoRa-Codec-2-RTX/`. Il firmware pubblico ha N0CALL: chi lo installa imposta
il proprio nominativo dal passo 2 della pagina. In locale: `tools/make_site.sh <artefatti> <uscita> <versione> <commit>` e un server
statico (Web Serial richiede https o `localhost`).

## Impostazioni senza ricompilare

Nominativo, frequenza e potenza si possono cambiare dalla seriale e restano nella memoria della scheda (NVS).
All'avvio si usano i valori salvati se sono validi, altrimenti quelli di `src/user_config.h`. Una modifica vale
**dal riavvio successivo**. La pagina web di configurazione userà gli stessi comandi.

Su `pio device monitor` (115200 baud) si scrivono righe che iniziano con `CFG`; maiuscole e minuscole sono indifferenti:

| Comando | Risposta | Note |
|---|---|---|
| `CFG GET` | `CFG callsign=IU5HJU freq=433.900 power=10 volume=7 micfloor=-60 board=t3 reboot=0 test=0` | `reboot=1` se i valori salvati differiscono da quelli in uso |
| `CFG VERSION` | `CFG VERSION fw=v0.1.0-3-gabc1234 board=t3` | versione del firmware (da `git describe`, "dev" se compilato senza git) e scheda; la pagina web la confronta con quella pubblicata |
| `CFG SET CALLSIGN iu5hju` | `CFG OK callsign=IU5HJU` | solo A-Z, 0-9 e `/`, massimo 8 caratteri |
| `CFG SET FREQ 433.900` | `CFG OK freq=433.900` | MHz, da 430.000 a 440.000 (70 cm); **segnaposto**: scegli il canale col band plan IARU R1 / ARI |
| `CFG SET POWER 10` | `CFG OK power=10` | dBm, da 2 a 17 |
| `CFG SET VOLUME 7` | `CFG OK volume=7` | volume iniziale dell'altoparlante: 0 muto, 1-10 in passi da 3 dB (10 = massimo); il pulsante su GPIO15 lo cambia poi a scheda accesa, ma allo spegnimento torna a questo valore |
| `CFG SET MICFLOOR -60` | `CFG OK micfloor=-60` | **fondo del microfono** in dBFS (da -80 a -36): livello in silenzio visto dall'AGC; l'AGC lavora sopra fondo + 6 dB. Vale subito e resta salvato; standard -60 |
| `CFG MIC MEASURE` | `CFG OK mic misura in corso...`, dopo 3 s `CFG MIC fondo=-61 min=-63 max=-58 frames=75` | misura il fondo (stare in silenzio): `fondo` è il livello sotto cui sta il 90% dei campioni. Non salva da solo: lo fa la pagina con `CFG SET MICFLOOR` |
| `CFG TEST AUDIO` | `CFG OK audio prova in corso...`, poi `CFG AUDIO f=500 base=-90.1 tone=-52.3` (e 1000, 2000 Hz) e `CFG AUDIO fine` | suona tre toni a volume basso e fisso e misura il microfono alla frequenza del tono ([verifica-hardware-audio.md](verifica-hardware-audio.md)); blocca la scheda per circa 3,5 s |
| `CFG TEST ON` / `CFG TEST OFF` | `CFG OK test=on` | prova di collegamento ([prova-collegamento.md](prova-collegamento.md)), attivabile anche con tre clic veloci sul PTT; non viene salvata, all'accensione è sempre spenta |
| `CFG RESET` | `CFG OK reset` | cancella i valori salvati e torna a quelli di `user_config.h` |
| `CFG REBOOT` | `CFG OK reboot` | riavvia la scheda |

Un valore non valido dà `CFG ERR <parametro> <motivo>` e non viene salvato. Con `N0CALL` la scheda continua a non trasmettere.
All'avvio la seriale stampa la riga `CFG GET` in fondo al log. Il resto dei parametri radio (SF, BW, CR, sync) resta in `user_config.h`.

## Licenza

- Il progetto è rilasciato sotto **GNU GPL versione 3** (file [`LICENSE`](../LICENSE)), Copyright (C) 2026 IU5HJU.
- Motivo: la libreria `esp32_codec2` (sh123) ha nel suo repository licenza **GPL-3.0** e contiene i sorgenti di Codec2 di David Rowe (LGPL-2.1). Un firmware compilato con quella libreria è soggetto alla GPL-3.0 per l'insieme.
- Chi distribuisce il firmware compilato (per esempio le release di GitHub) deve rendere disponibile il sorgente corrispondente e mantenere la stessa licenza.
- Altre librerie usate, con licenze compatibili: RadioLib (MIT), Adafruit SSD1306 e GFX (BSD). Le licenze esatte e le versioni sono da controllare nei rispettivi repository; questo elenco non è un parere legale.
- Il codice di `esp32_loradv` (GPL-2.0) non è stato copiato.

## Prima di trasmettere

1. **Frequenza:** `LCR_FREQUENCY_MHZ` in `src/user_config.h` è un segnaposto. Scegliere il canale col band plan IARU R1 / ARI per i modi digitali sui 70 cm.
2. **Nominativo:** vale di default **`N0CALL`**, il segnaposto "nessun nominativo": con N0CALL (o NOCALL) la scheda **riceve ma non trasmette**, e l'OLED mostra "SOLO RX" all'avvio e "IMPOSTA NOMINATIVO" se premi il PTT. Per trasmettere imposta il tuo (A-Z, 0-9, `/`, massimo 8 caratteri) dalla [pagina web](https://iu5hju.github.io/LoRa-Codec-2-RTX/) o con `CFG SET CALLSIGN` dalla seriale, senza ricompilare. Oppure cambia `LCR_CALLSIGN` in `src/user_config.h` e ricompila.
3. **Antenna** collegata prima di alimentare la scheda.
4. **Potenza:** parte da 10 dBm per i test (`CFG SET POWER`, o `LCR_TX_POWER_DBM`).

## Primo avvio, in ordine

1. **Avvio:** l'OLED mostra "AVVIO" e poi "ASCOLTO". Se compare "ERRORE" con il nome del blocco (Codec2, audio I2S, radio) la riga della seriale dà il codice. Senza OLED il resto funziona e la seriale lo segnala.
2. **Radio:** con due schede accese, premere il PTT su una: l'altra deve passare a "RX <nominativo>" anche con il microfono muto. Controllare RSSI e SNR.
3. **Microfono:** parlare con il PTT premuto e controllare sull'altra scheda se si sente qualcosa. Se si sente solo silenzio, provare `LCR_MIC_SLOT 1` (lo slot sinistro/destro dello stereo dipende dal collegamento di `L/R`).
4. **Altoparlante:** il pulsante volume (GPIO15, opzionale) fa un bip a ogni passo. Se il volume sembra dimezzato, verificare il pin `SD` dell'amplificatore.
5. **Voce:** turno completo di qualche secondo e risposta dopo 5-10 s.
6. **Messa a punto:** guadagno e AGC del microfono (`FrontEndConfig`), AGC e volume in uscita (`BackEndConfig`), pre-roll, 8 o 16 kHz.
7. **Misure:** portata, perdite (OLED: "persi" e "recup." dopo ogni ricezione) e tempo reale in aria; poi decidere SF/BW definitivi.

## Limiti noti

- Una trasmissione LoRa blocca il ciclo principale per circa 0,15 s a pacchetto: il microfono è letto da un task a parte, quindi non si perdono campioni.
- Il display è ridisegnato al massimo 10 volte al secondo perché l'I2C costa circa 25 ms a disegno.
- Il CAD rileva solo preamboli LoRa con gli stessi parametri: altre trasmissioni (FM, FSK) non sono viste.
- Il pulsante PTT su GPIO0 e quello volume su GPIO15 sono pin di avvio: non tenerli premuti durante accensione o reset.
