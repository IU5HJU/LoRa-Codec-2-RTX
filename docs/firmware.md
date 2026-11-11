# Firmware

Schede supportate (un ambiente PlatformIO e un artefatto di Actions ciascuna):

| Scheda | Ambiente | Artefatto di GitHub Actions | Stato |
|---|---|---|---|
| LILYGO LoRa32 T3 V1.6.1 | `lora32` | `firmware-t3-v161` | **provata** (tre profili, menu, Wi-Fi di configurazione, volume; 10/11/2026) |
| LILYGO T-Beam V1.2 (AXP2101) | `tbeam` | `firmware-tbeam-v12` | **provata** (avvio anche a batteria, AXP2101, radio, tasto IO38 come PTT, batteria sull'OLED, GPS con fix, trasmissione tra T3 e T-Beam nei tre profili; 10/11/2026) |

I pin e le differenze tra le due sono in [pinout.md](pinout.md).

> **Mai alimentare la scheda senza antenna a 433 MHz (o carico fittizio) collegata**, nemmeno
> solo per compilare o caricare il firmware via USB: l'USB alimenta il modulo radio, che senza
> carico si può danneggiare. Prima l'antenna, poi il cavo USB.

**Stato (10/11/2026).** Compila con PlatformIO su GitHub Actions. **Provato sulle schede** (T3 e T-Beam): voce a turni nei tre profili (Standard, Messaggio, Qualità), microfono e altoparlante, menu a bordo, configurazione dal Wi-Fi della scheda, volume con guadagno, posizione e GPS, impostazioni salvate, pagina web di installazione e configurazione, prova di collegamento. **Ancora da misurare:** distanze lunghe, mezzi in movimento. **CPU del Codec2 3200 misurata:** codifica 10,0 ms e decodifica 13,3 ms per frame da 40 ms ([codec2.md](codec2.md)).
Problemi trovati e risultati sono in [prove.md](prove.md). La logica del protocollo
(`lib/voicecore/`) è testata sul PC. Il workflow `.github/workflows/build.yml` produce
`web_factory.bin` e `ota_update.bin` come artefatti di ogni esecuzione.

## File

| File | Compito |
|---|---|
| `src/main.cpp` | Avvio, pulsanti PTT e volume, ciclo principale |
| `src/radio_lora.*` | SX1278 con RadioLib: parametri LoRa, CAD, invio, ricezione (`IRadio`) |
| `src/codec2_adapter.*` | Codec2 a 1200 bps o a 3200 bps (due blocchi da 20 ms per frame) (`ICodec`) |
| `src/wifi_config.*`, `src/wifi_page.h` | Configurazione dal Wi-Fi della scheda: rete con password salvata, server asincrono, pagina su 192.168.4.1 |
| `src/gps.*`, `lib/voicecore/nmea.*`, `position.*` | GPS della T-Beam (letture NMEA), posizione, QTH e distanza |
| `lib/voicecore/profile.*`, `menu.*` | Profili (Standard, Messaggio, Qualità) e menu a bordo |
| `lib/voicecore/morse.*` | Segnali "MSG" (voce) e "SMS" (testo) in Morse all'arrivo di un messaggio |
| `lib/voicecore/meshcom_frame.h`, `text_template.*`, `choice_picker.*` | Escursione MeshCom: frame di testo (codifica, decodifica, FCS), segnaposti `%C %M %F` nei testi, scelta di testo e destinatario dal menu |
| `lib/voicecore/inbox.*` | Casella dei messaggi ricevuti: un solo messaggio (voce o testo), cancellato dopo l'ascolto |
| `lib/voicecore/text_picker.*` | Scelta del testo da inviare (voce "Invia testo" del menu) |
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

Il firmware di `main` ha il nominativo `N0CALL` e non trasmette: il tuo lo imposti **dalla [pagina web](https://iu5hju.github.io/VoLoRa/) o con `CFG SET CALLSIGN`, senza ricompilare**, e resta salvato nella scheda anche dopo un aggiornamento. Non serve più un ramo personale con il nominativo dentro.
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
L'indirizzo sarà `https://iu5hju.github.io/VoLoRa/`. Il firmware pubblico ha N0CALL: chi lo installa imposta
il proprio nominativo dal passo 2 della pagina. In locale: `tools/make_site.sh <artefatti> <uscita> <versione> <commit>` e un server
statico (Web Serial richiede https o `localhost`).

## Impostazioni senza ricompilare

Nominativo, frequenza e potenza si possono cambiare dalla seriale e restano nella memoria della scheda (NVS).
All'avvio si usano i valori salvati se sono validi, altrimenti quelli di `src/user_config.h`. Una modifica vale
**dal riavvio successivo**. La pagina web di configurazione userà gli stessi comandi.

Su `pio device monitor` (115200 baud) si scrivono righe che iniziano con `CFG`; maiuscole e minuscole sono indifferenti:

| Comando | Risposta | Note |
|---|---|---|
| `CFG GET` | `CFG callsign=IU5HJU freq=433.900 freqmsg=433.900 power=10 volume=7 micfloor=-60 profile=std board=t3 reboot=0 test=0` | `reboot=1` se i valori salvati differiscono da quelli in uso |
| `CFG VERSION` | `CFG VERSION fw=v0.1.0-beta.2 board=t3 proto=2` | versione del firmware (da `git describe`, "dev" se compilato senza git), scheda e **versione del protocollo** ([versioni.md](versioni.md)); la pagina web la confronta con quella pubblicata |
| `CFG SET CALLSIGN iu5hju` | `CFG OK callsign=IU5HJU` | solo A-Z, 0-9 e `/`, massimo 8 caratteri |
| `CFG SET FREQ 433.900` | `CFG OK freq=433.900` | MHz, da 430.000 a 440.000 (70 cm); **segnaposto**: scegli il canale col band plan IARU R1 / ARI |
| `CFG SET POWER 10` | `CFG OK power=10` | dBm, da 2 a 17 |
| `CFG SET VOLUME 7` | `CFG OK volume=7` | volume iniziale dell'altoparlante: 0 muto, 1-10 in passi da 3 dB (10 = massimo); il pulsante su GPIO15 lo cambia poi a scheda accesa, ma allo spegnimento torna a questo valore **Dal 10/11/2026 il passo 5 è 0 dB (il vecchio massimo) e i passi 6-10 danno guadagno digitale fino a +15 dB** (il limitatore morbido tiene i picchi sotto il fondo scala); il passo 1 è -12 dB. I volumi salvati con la scala vecchia si convertono da soli (vecchio 10 → 5). |
| `CFG SET FREQMSG 433.900` | `CFG OK freqmsg=433.900` | **frequenza del profilo Messaggio** (MHz, 430.000-440.000), come `FREQ`. **Può essere uguale** a `FREQ`: nessun controllo (nelle prove si usa spesso una frequenza nota). La scheda usa la frequenza del profilo scelto |
| `CFG SET FREQQUAL 433.900` | `CFG OK freqqual=433.900` | **frequenza del profilo Qualità**, come `FREQMSG` (può essere uguale alle altre) |
| `CFG SET PROFILE MSG` | `CFG OK profile=msg` | **profilo di trasmissione**: `STD` (Standard, voce in tempo reale, SF8/BW250) `QUA` (Qualità: Codec2 3200, SF7/BW250, voce in tempo reale, meno portata) oppure `MSG` (Messaggio, lungo raggio, SF12/BW125, registra e invia: [modo-messaggio.md](modo-messaggio.md)). Vale dal riavvio; le schede con profili diversi non si sentono |
| *Pagina web* | | dal 10/11/2026 la pagina di configurazione ha una sezione **"Posizione (facoltativa)"** con la trasmissione della posizione (`POS`), la posizione fissa (`QTH`: latitudine e longitudine, si accetta anche la virgola, "Cancella" per toglierla) e il GPS (solo T-Beam). I campi si spengono con un firmware più vecchio che non li conosce; dopo il salvataggio la pagina rilegge i valori dalla scheda |
| `CFG SET TEXT1 Arrivo tra 10 minuti` | `CFG OK text1=Arrivo tra 10 minuti` | **testi predefiniti da inviare** (`TEXT1`...`TEXT5`): messaggi di testo stile SMS, 1-150 caratteri ASCII, senza `{` e `}`, **il testo è il resto della riga** (può avere spazi). `NONE` svuota il posto. Valgono **subito**, senza riavvio (non cambiano `reboot=`). All'inizio ci sono cinque testi di esempio; il primo è `%C QRV su VoLoRa modo %M %F`. Nel testo valgono i **segnaposti** `%C` (il tuo nominativo), `%M` (la lettera del modo in uso: S, M, Q) e `%F` (la frequenza del profilo in uso), sostituiti all'invio ([meshcom-testo.md](meshcom-testo.md)). Si inviano dal menu a bordo, voce "Invia testo" ([modo-messaggio.md](modo-messaggio.md)) |
| `CFG GET TEXT1` | `CFG TEXT1=Arrivo tra 10 minuti` | legge il testo 1-5 (vuoto: `CFG TEXT1=`). Un comando per testo: `CFG GET` non li contiene |
| `CFG SET MCDEST1 22251` | `CFG OK mcdest1=22251` | **escursione MeshCom** ([meshcom-testo.md](meshcom-testo.md)): `MCDEST1`…`MCDEST5` sono i destinatari tra cui scegliere (un gruppo, un nominativo-SSID o `*`; `NONE` svuota); `CFG SET MCSSID 99`, `CFG SET MCWINDOW 300` (secondi di ascolto, 30-1800), `CFG SET FREQMC 433.175`; `CFG GET MC` → `CFG MC freqmc=433.175 ssid=99 window=300`, `CFG GET MCDEST1`; valgono subito |
| `CFG SET GPS ON` | `CFG OK gps=on` | **accende il GPS della T-Beam** (alimentazione ALDO3 e lettura NMEA a 9600 baud su GPIO34). **Spento di default**: consuma decine di mA. Vale dal riavvio. Sulle altre schede risponde `CFG ERR gps questa scheda non ha il GPS`. Con un fix recente (meno di 15 s) la posizione trasmessa nel pacchetto START (`CFG SET POS ON`) e quella per la distanza sul display vengono dal GPS; senza fix si usa la posizione fissa (`CFG SET QTH`); senza nemmeno quella, nessuna posizione. In seriale: `[gps]` con fix, satelliti e frasi lette; sul display a riposo "GPS fix, N satelliti" oppure "GPS cerco satelliti" |
| `CFG SET POS ON` | `CFG OK pos=on` | **trasmette la posizione** nel pacchetto `START` (protocollo 2, [protocollo.md](protocollo.md)). **Spenta di default**: la posizione è un dato personale e viaggia in chiaro. Se non c'è una posizione (QTH non impostato) non parte nulla. `OFF` la spegne |
| `CFG SET QTH 43.7696,11.2558` | `CFG OK qth=43.76960,11.25580` | **posizione fissa** (latitudine,longitudine in gradi decimali, senza spazi). Serve anche a mostrare la **distanza in km** dalle stazioni che trasmettono la loro posizione, anche con `POS OFF`. `NONE` la cancella |
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
2. **Nominativo:** vale di default **`N0CALL`**, il segnaposto "nessun nominativo": con N0CALL (o NOCALL) la scheda **riceve ma non trasmette**, e l'OLED mostra "SOLO RX" all'avvio e "IMPOSTA NOMINATIVO" se premi il PTT. Per trasmettere imposta il tuo (A-Z, 0-9, `/`, massimo 8 caratteri) dalla [pagina web](https://iu5hju.github.io/VoLoRa/) o con `CFG SET CALLSIGN` dalla seriale, senza ricompilare. Oppure cambia `LCR_CALLSIGN` in `src/user_config.h` e ricompila.
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

## Configurazione dal Wi-Fi della scheda (voce "Wi-Fi" del menu)

Senza cavo e senza PC con Chrome: tieni premuto il **volume** (circa 2 s) per aprire il menu, scegli **Wi-Fi** con il PTT e conferma con il volume breve. La scheda **si riavvia in un modo dedicato** (come nel Tracker LoRa APRS di Guzman): all'avvio, **prima di Codec2, audio e radio**, crea la rete `VoLoRa-XXXX` (XXXX dall'indirizzo della scheda; fino all'11/11/2026 si chiamava `LoRaCodec2-XXXX`: sul telefono dimentica la rete vecchia). L'installazione dalla pagina `iu5hju.github.io/VoLoRa/` con la versione di sviluppo è stata provata su una T3 l'11/11/2026 con una **password casuale di 8 cifre**, generata la prima volta e **poi sempre la stessa** (salvata nella scheda; cambia solo con i valori di fabbrica), mostrata sul display e sulla seriale. In questo modo la scheda **non trasmette e non riceve**. Collegati con telefono o computer e apri **http://192.168.4.1**: è la stessa configurazione della pagina web (nominativo, profilo, tre frequenze, potenza, volume, fondo del microfono, posizione, GPS), con gli stessi comandi `CFG`. Dopo "Salva" la pagina **rilegge i valori dalla scheda** e segnala quelli che non coincidono. Per uscire: pulsante "Riavvia la scheda" nella pagina, pressione lunga del volume, oppure **5 minuti senza client collegati**. Le prove audio e la misura del microfono non sono disponibili da qui (restano nella pagina con il cavo). Non provato su scheda al 10/11/2026.

## Riquadro del profilo sul display

In alto a destra, su ogni schermata, una lettera grande (bianca su nero, 10x9 punti) indica il profilo in uso: **S** Standard, **M** Messaggio, **Q** Qualità. La riga del nominativo è stata accorciata (`IU5HJU 433.900MHz`) per fargli posto.

Se il telefono dice "nessun Internet" e la pagina non si apre: scegli di **restare collegato** a quella rete, **spegni i dati mobili** e apri a mano `http://192.168.4.1` (con `http://`, non `https://`). La seriale stampa righe `[wifi]` (client collegato, indirizzo assegnato, richiesta della pagina) per capire a che punto si ferma. La configurazione del Wi-Fi segue quella del Tracker LoRa APRS di Guzman (modo NULL, poi AP, `softAP(nome, password)` predefinito, nessun DNS) e usa lo stesso server asincrono (ESPAsyncWebServer + AsyncTCP, stesse versioni).

## Ripetitore a due schede (ruoli RX e TX, 10/11/2026)

Studio e decisioni: [studio-ripetitore.md](studio-ripetitore.md). **Scritto, non ancora provato sulle schede** (compila solo se Actions è verde; la logica è provata sul PC).

**Il ripetitore** è fatto da **due schede uguali** (T3 o T-Beam, **senza audio**; la RX può avere un altoparlante di monitor, vedi sotto): la scheda **RX** ascolta la frequenza di ingresso e manda i pacchetti da ripetere alla scheda **TX**, che li trasmette sulla frequenza di uscita. Ripete il **modo del profilo scelto** (di base lo Standard, Codec2 1200) e i **messaggi di testo** (modo 7, dall'11/11/2026: un pacchetto solo, la seconda copia del mittente non si ripete) e aggiunge ai pacchetti il flag `RELAYED`. Un testo non fa parte del turno vocale in corso e non ne cambia la durata. Ogni scheda ha la **sua antenna**: la separazione fra le due antenne (distanza, filtri, cavità) è a cura dell'operatore.

**Impostazioni** (stesse per le due schede salvo il ruolo, dalla seriale USB; valgono dal riavvio):

```
CFG SET CALLSIGN IU5HJU       nominativo del ripetitore (con N0CALL la scheda TX non trasmette)
CFG SET PROFILE STD           profilo ripetuto (STD, e in teoria QUA o MSG: il modo ripetuto e' quello del profilo)
CFG SET FREQ 433.900          frequenza di INGRESSO (la usa la scheda RX; per MSG e QUA FREQMSG e FREQQUAL)
CFG SET FREQOUT 434.400       frequenza di USCITA (la usa la scheda TX)
CFG SET ROLE RPTRX            sulla scheda RX   (oppure RPTTX sulla scheda TX; OFF torna alla scheda normale)
CFG REBOOT
```

**Collegamento fra le due schede** (seriale a 115200, 3,3 V, nessun altro filo oltre alla massa): **pin 14 e pin 13** di ciascuna (gli stessi dell'I2S, liberi senza audio):

| Scheda RX | Scheda TX |
|---|---|
| pin 14 (TX) | pin 13 (RX) |
| pin 13 (RX) | pin 14 (TX) |
| GND | GND |

Sulla T3 i pin 14 e 13 sono sullo slot microSD: **toglierla**. Il display di ciascuna scheda mostra lo stato (pacchetti ricevuti, ripetuti, scartati, coda della TX, collegamento). La scheda TX trasmette ogni pacchetto della coda con la pausa minima di 80 ms. Per uscire dal ruolo: `CFG SET ROLE OFF` e `CFG REBOOT`.

**Monitor audio sulla scheda RX (11/11/2026):** con un **altoparlante** (amplificatore MAX98357A come sulle schede normali) la scheda RX fa ascoltare la voce che riceve sulla frequenza di ingresso, per sorvegliare il traffico. Si accende con `CFG SET MONITOR ON` (e `CFG REBOOT`; `CFG GET MONITOR` lo legge; di base è spento). Si ascolta solo la voce del modo del profilo (non il testo né le prove), al ritmo normale e con la parità che recupera i pacchetti persi, come una scheda normale; nel profilo Messaggio la voce parte a messaggio concluso. Il monitor non cambia cosa viene ripetuto e non trasmette mai. Volume iniziale: `CFG SET VOLUME`; il pulsante volume (GPIO15) lo alza a scheda accesa. Sul display, a voce in corso, compare "RX" con il nominativo di chi parla.

**Attenzione ai fili:** l'altoparlante usa l'I2S, cioè i pin 14 e 13 che la seriale del ripetitore usa di solito. Con il monitor acceso la scheda RX usa **altri due pin** per la seriale (la TX resta sui pin 14 e 13):

| | T3 V1.6.1 (RX) | T-Beam V1.2 (RX) | Scheda TX |
|---|---|---|---|
| ricezione della RX | GPIO34 | GPIO36 | pin 14 (TX) → qui |
| trasmissione della RX | GPIO2 | GPIO12 | qui → pin 13 (RX) |

Sulla RX **non va montato il microfono** (GPIO34/36 sono i pin dei suoi dati) e sulla T3 la microSD va tolta (GPIO2 è il MISO dello slot); sulla T-Beam il GPS deve restare spento. **Da verificare sul banco**: la scelta dei pin è teorica, i pin sono nei file `board_pins_*.h` (`kLinkRxMon`, `kLinkTxMon`) se vanno cambiati.

**Cosa non c'è ancora:** comandi del sysop (modo 5), identificazione periodica del ripetitore, ripetizione di più profili (solo uno per ripetitore, opzione A dello studio).
