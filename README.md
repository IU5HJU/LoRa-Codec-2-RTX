# VoLoRa

*(già LoRa-Codec-2-RTX fino all'11/11/2026: se cerchi il vecchio nome, sei nel posto giusto.)*

Messaggi vocali broadcast a turni (stile QSO) tra schede LilyGo a 433 MHz (LoRa32 T3 V1.6.1 e T-Beam V1.2, quest'ultima anche con il modulo radio SX1262), per radioamatori.
La voce è compressa con Codec2 e trasmessa con LoRa, con tre **profili** da scegliere dal menu della scheda:

| Profilo | Radio | Codec2 | Uso |
|---|---|---|---|
| **Standard** (S) | SF7, banda 250 kHz | 3200 bps | voce in tempo reale più naturale; è il profilo di base, quello del ripetitore e del nodo Internet |
| **Range** (R) | SF8, banda 250 kHz | 1200 bps | voce in tempo reale con un po' più di portata (circa 2,5 dB teorici, non misurati) |
| **Messaggio** (M) | SF12, banda 125 kHz | 1200 bps | lungo raggio: si registra un messaggio (fino a 15 s); chi riceve lo trova in una casella e lo ascolta premendo il PTT, con la percentuale sul display |

**Perché lo Standard è il 3200 (decisione del 12/11/2026):** il guadagno di portata del 1200 (Range) è modesto, circa 2,5 dB teorici e non misurati, mentre la voce a 3200 bit/s suona meglio. Il **Range** resta nel firmware per i collegamenti in condizioni difficili, quando serve quel poco di portata in più a scapito della qualità vocale ([decisione](docs/roadmap.md)).

Funzionamento in breve:

1. Si preme il PTT: il microfono viene bufferizzato subito.
2. Si trasmette il nominativo, poi la voce Codec2 man mano che si parla.
3. Al rilascio del PTT (massimo 30 s) si invia l'ultimo audio e il segnale di fine turno (OVER).
4. Tutte le stazioni in ascolto sulla frequenza ricevono e riproducono. Dopo 5-10 s di attesa si risponde.

> **Progetto sperimentale (beta).** La voce passa da una scheda all'altra ed è **provata sulle schede** (T3 e T-Beam,
> nei tre profili, 10/11/2026: esiti in [docs/prove.md](docs/prove.md)). **Manca ancora la misura sul campo**: distanze
> lunghe, mezzi in movimento. Possono trasmettere solo radioamatori con il proprio nominativo, sulle frequenze e con le
> potenze consentite. Il nominativo viaggia in chiaro e i contenuti non sono cifrati.

> **Nuovo? Parti dalla [guida rapida](docs/guida-rapida.md).** Indice della documentazione: [docs/README.md](docs/README.md).

## Oltre alla voce a turni

Sulle stesse schede, dalla beta.2 (09/11/2026) a oggi (14/11/2026):

- **Testi brevi** (stile SMS, fino a 150 caratteri), **casella dei messaggi**, **MeshCom** (escursione a tempo sulle reti di testo a 433,175 MHz) e **posizione/GPS**: [docs/meshcom-testo.md](docs/meshcom-testo.md), [docs/protocollo.md](docs/protocollo.md).
- **Ripetitore con due T3** (RX e TX) con identificativo periodico, anche in **Morse a tono in FM** per chi non ha le schede LoRa, e comandi firmati del sysop: [docs/ripetitore.md](docs/ripetitore.md).
- **Shift** per operare via ripetitore, libero da 0 a 10 MHz in positivo o negativo (come l'offset di una radio): [docs/firmware.md](docs/firmware.md).
- **Nodo Internet** (due schede su Wi-Fi che si portano i pacchetti, UDP firmato) e **reflector**: un server a cui si collegano nodi e ripetitori **senza aprire porte sul router**, con pagina di amministrazione (password) e pagina pubblica di sola lettura: [docs/reflector.md](docs/reflector.md), [docs/banco-nodo.md](docs/banco-nodo.md).
- **Prova di potenza** fra due schede: cerca da sola la potenza minima con cui si sentono, e l'esito compare sul display di entrambe: [docs/prova-collegamento.md](docs/prova-collegamento.md).
- **T-Beam con radio SX1262** (la stessa scheda col modulo LoRa diverso dall'SX1278): firmware dedicato, scritto ma **non ancora provato** su una scheda: [docs/tbeam-sx1262.md](docs/tbeam-sx1262.md).

## Per provare il firmware

**Occorrente per ogni stazione:**

- scheda **LILYGO LoRa32 T3 V1.6.1** oppure **T-Beam V1.2 (AXP2101)**, a 433 MHz, con **antenna a 433 MHz** (SMA); sulla T-Beam guarda la sigla sul modulo radio (**SX1278** o **SX1262**): servono firmware diversi ([docs/tbeam-sx1262.md](docs/tbeam-sx1262.md));
- un **pulsante PTT** esterno tra GPIO0 e massa;
- per la voce: microfono **INMP441**, amplificatore **MAX98357A** e altoparlante 8 Ω 3 W (per il solo test radio senza audio non servono);
- schema dei collegamenti in [docs/pinout.md](docs/pinout.md) (con le immagini dei collegamenti di T3 e T-Beam) e [docs/audio.md](docs/audio.md).

**Regole di sicurezza:** antenna collegata **prima** del cavo USB, mai alimentare la scheda senza antenna o carico fittizio; microSD tolta durante il caricamento.

**Passi:**

1. **Dal browser (il modo più semplice, anche senza Linux).** Apri la [pagina di installazione](https://iu5hju.github.io/VoLoRa/) con Chrome, Edge od Opera, collega la scheda con il cavo USB, scegli la **versione dal menu a tendina** (quella proposta va bene per chi non sa quale scegliere: stabile, o beta finché non ce n'è una) e premi "Installa il firmware". Poi, nel passo 2 della stessa pagina, imposta **nominativo**, frequenza, potenza e volume: restano salvati nella scheda. Il firmware pubblico ha `N0CALL` e **non trasmette** (riceve soltanto) finché non imposti il tuo nominativo. La configurazione si può fare anche **senza cavo e senza PC**: dal menu della scheda (volume lungo, voce "Wi-Fi") la scheda crea una rete con la sua pagina su `192.168.4.1` ([docs/firmware.md](docs/firmware.md)).
2. **In alternativa.** Scarica i binari dall'esecuzione di GitHub Actions (artefatto `firmware-t3-v161`, `firmware-tbeam-v12` o, per la T-Beam col modulo SX1262, `firmware-tbeam-v12-sx1262`; serve essere loggati; contengono `web_factory.bin` e `ota_update.bin`) e caricali con `web_factory.bin` all'indirizzo `0x0` ([docs/firmware.md](docs/firmware.md), anche per Windows; **così le impostazioni salvate si cancellano**, la pagina invece le conserva negli aggiornamenti), oppure compila in locale con PlatformIO. Il nominativo non richiede di ricompilare: si imposta dalla pagina o con `CFG SET CALLSIGN` e resta salvato. Se preferisci un firmware con il tuo nominativo già dentro: fai un **fork**, cambia `LCR_CALLSIGN` in `src/user_config.h` dal sito GitHub e lascia compilare Actions (non usare binari di altri che ne contengono uno diverso). Dalla seriale si cambia tutto con i comandi `CFG` ([docs/firmware.md](docs/firmware.md)).
3. **Frequenza.** Il valore predefinito è un **segnaposto**: scegli il canale con il band plan IARU R1 / ARI. Le stazioni che si ascoltano devono avere gli **stessi** parametri LoRa e la stessa frequenza.
4. **Prova di potenza (senza microfono né altoparlante).** Su **entrambe** le schede attiva la prova (tre pressioni brevi del PTT, il pulsante nella pagina o `CFG TEST ON`; serve un nominativo vero), poi su una premi il PTT: la scheda ripete la prova di collegamento (sequenze note confrontate bit per bit) dalla potenza massima alla minima, un dBm alla volta, e l'altra risponde ogni volta. Alla fine **tutte e due mostrano sul display** la potenza minima con cui si sentono. Una scheda con la prova spenta non ascolta le prove. Dettagli in [docs/prova-collegamento.md](docs/prova-collegamento.md).

## Per chi collabora

- **`main` è il punto comune di partenza**: è sempre compilabile (GitHub Actions deve essere verde) e ha come nominativo `N0CALL`, quindi non trasmette finché non imposti il tuo.
- **Come contribuire:** fai un fork, lavora su un ramo tuo e proponi le modifiche con una pull request. Prima di proporle esegui `make test`.
- **Prove:** annota ogni prova in [docs/prove.md](docs/prove.md) con il modello indicato. Per un problema da risolvere apri una segnalazione (Issues).
- **Decisioni e ordine dei lavori:** [docs/roadmap.md](docs/roadmap.md). Prossimi passi: misure sul campo (RSSI, SNR, perdite, anche in movimento), intestazione del messaggio, memorie con profilo e frequenze per canale, locatore Maidenhead e ora dal GPS; il ripetitore: prova a banco ([docs/banco-ripetitore.md](docs/banco-ripetitore.md)).

## Licenza

Copyright (C) 2026 IU5HJU. Questo programma è software libero, distribuito sotto la
**GNU General Public License versione 3** (vedi [LICENSE](LICENSE)), senza alcuna garanzia.
La scelta deriva dalla libreria Codec2 per ESP32 usata (GPL-3.0); dettagli sulle librerie in
[docs/firmware.md](docs/firmware.md).

Protocollo e parametri: [docs/protocollo.md](docs/protocollo.md). Audio e collegamenti: [docs/audio.md](docs/audio.md). Taratura del microfono, misure e come adattarla: [docs/taratura-microfono.md](docs/taratura-microfono.md). Firmware e primo avvio: [docs/firmware.md](docs/firmware.md). Decisioni e prossimi passi: [docs/roadmap.md](docs/roadmap.md). Registro delle prove: [docs/prove.md](docs/prove.md). Prova di collegamento: [docs/prova-collegamento.md](docs/prova-collegamento.md). Verifica del montaggio audio con una sola scheda: [docs/verifica-hardware-audio.md](docs/verifica-hardware-audio.md). Ricezione e decodifica con un SDR: [docs/sdr.md](docs/sdr.md). Numerazione delle versioni (beta, rc, stabile): [docs/versioni.md](docs/versioni.md). Alimentazione e batteria della T-Beam: [docs/alimentazione.md](docs/alimentazione.md). Schermate dell'OLED: [docs/display.md](docs/display.md). Modi del Codec2 e confronto: [docs/codec2.md](docs/codec2.md). Profilo Messaggio: [docs/modo-messaggio.md](docs/modo-messaggio.md). Testi su reti a pacchetto (MeshCom): [docs/meshcom-testo.md](docs/meshcom-testo.md). Il ripetitore: [docs/ripetitore.md](docs/ripetitore.md). Nodo Internet e reflector: [docs/reflector.md](docs/reflector.md), [docs/banco-nodo.md](docs/banco-nodo.md). La T-Beam con radio SX1262: [docs/tbeam-sx1262.md](docs/tbeam-sx1262.md). Progetti simili e scelte che ne derivano: [docs/progetti-simili.md](docs/progetti-simili.md).

## Stato

Aggiornato il 14/11/2026.

| Parte | Stato |
|---|---|
| Formato pacchetti (protocollo v2), parità XOR, sessioni TX/RX, pausa tra i pacchetti | fatto, testato sul PC |
| Voce a turni in Range (ex Standard: Codec2 1200, SF8/BW250) | **provata sulle schede** (T3 e T-Beam); cause dei crepitii trovate e corrette |
| Profilo **Messaggio** (SF12/BW125): registra e invia, riproduzione a messaggio completo, percentuale e suono "MSG" in Morse | **provato sulle schede** |
| **Ripetitore:** la scheda RX ripete anche i testi e può avere un **altoparlante di monitor** per ascoltare la voce che riceve (`CFG SET MONITOR ON`) | scritto, provato sul PC; **da provare sul banco** |
| **MeshCom** (11/11/2026): dal menu un'**escursione a tempo** sulle reti di testo radioamatoriali (433,175 MHz) per avvisare o fissare un QSO: testo con segnaposti (`%C`, `%M`, `%F`), destinatario scelto (gruppo, nominativo o tutti), ascolto delle risposte e rientro automatico alla voce ([docs/meshcom-testo.md](docs/meshcom-testo.md)); non sostituisce un nodo | **provato in aria** (T3 e nodo MeshCom, 11/11/2026): invio al gruppo, risposte dirette e di gruppo, rientro alla voce; conferma di ricezione vista dall'app |
| **Casella dei messaggi** (11/11/2026): la voce non parte più da sola, si apre con il PTT breve e si cancella dopo l'ascolto; **messaggi di testo** da 150 caratteri ("SMS", busta sul display), cinque testi predefiniti impostabili da web e Wi-Fi, menu "Invia testo" | scritta e provata sul PC, **da provare su scheda** |
| Profilo **Standard** (ex Qualità: Codec2 3200, SF7/BW250) | **provato sulle schede**: suona meglio del 1200; CPU misurata sulla scheda: codifica 10,0 ms e decodifica 13,3 ms per frame da 40 ms (25% e 33%) |
| Menu a bordo (volume lungo, PTT, conferma; riavvio sul profilo scelto) e lettera S/M/Q sul display | **provato** |
| Configurazione dal Wi-Fi della scheda (voce "Wi-Fi", pagina su 192.168.4.1, password salvata) | **provata** |
| Volume con guadagno (passo 5 = 0 dB, fino a +15 dB) | **provato** all'aperto |
| Posizione (spenta di default), QTH fisso, distanza dalla stazione ricevuta; GPS sulla T-Beam | **provati** (fix con 4 satelliti) |
| Impostazioni nella scheda e comandi `CFG` | fatti e provati; salvate in NVS |
| Pagina web di installazione e configurazione (GitHub Pages, Web Serial) con menu delle versioni | **online** e provata |
| Prova di collegamento (frame noti, confronto bit per bit), dal 13/11/2026 **prova di potenza** dalla massima alla minima con l'esito sul display di entrambe le schede | la prova con frame noti è **provata sulle schede**; la prova di potenza è **provata sul PC** (le due schede vengono simulate) sul display di una scheda e **in aria a 5,02 km il 14/11/2026** (esito MIN 2 dBm, frame 100%; [docs/prove.md](docs/prove.md)); l'esito sul display di **entrambe** le schede non è documentato |
| **Nodo Internet** (due schede su Wi-Fi, UDP firmato con HMAC) con identificativo periodico anche in Morse FM | **provato sulle schede** (12/11/2026: radio, rete, riavvio, router spento, conversazione lunga); il Morse in FM ascoltato con un ricevitore FM (13/11/2026, la portante è stata resa continua dopo la prova, da riprovare) |
| **Reflector** (server `vlreflector`, pagina di amministrazione con password, pagina pubblica, IP del nodo) | **provato** sul PC, su un server Debian di casa e da una rete cellulare; una scheda collegata dal telefono è vista dal reflector (13/11/2026); la **voce attraverso il reflector** e l'IP pubblico dichiarato dalle schede sono **da provare** ([docs/reflector.md](docs/reflector.md)) |
| **Shift** libero da 0 a 10 MHz (direzione e scostamento) per operare via ripetitore | scritto, provato sul PC e dalla pagina; **non ancora provato con un ripetitore** |
| **T-Beam con radio SX1262** | firmware **scritto e compilato** da GitHub Actions; **mai provato su una scheda** ([docs/tbeam-sx1262.md](docs/tbeam-sx1262.md)) |
| Aggiornamento dalla pagina **senza perdere le impostazioni** (dal 13/11/2026) | corretto e coperto da un test; **da confermare su una scheda** |
| Ricezione con SDR (NESDR, GNU Radio): decodifica e ascolto in tempo reale, anche in Standard | fatta e **provata** in Range, in Messaggio e in Standard (provata il 10/11/2026 con i vecchi nomi: Standard, Messaggio, Qualità) |
| Misure sul campo, distanze lunghe, mezzi in movimento | **da fare** |
| **Ripetitore con due T3** (RX e TX collegate con quattro fili): ripetizione della voce e del testo, messaggi vocali in coda, annuncio di occupato, identificativo periodico in testo e in Morse FM, comandi firmati del sysop ([docs/ripetitore.md](docs/ripetitore.md)) | scritto, **provato sul PC** e compilato da GitHub Actions; **mai provato sulle schede**: prima la prova a banco ([docs/banco-ripetitore.md](docs/banco-ripetitore.md)) |
| Memorie, locatore Maidenhead | da fare ([roadmap](docs/roadmap.md)) |

## Struttura

```
lib/voicecore/   logica del protocollo, indipendente dalla scheda
src/             firmware per le schede supportate e impostazioni dell'operatore (user_config.h)
tools/           programmi per il PC: reflector (vlreflector) e nodo finto (vlnode), ricevitore SDR, pagina del sito
tests/           test sul PC con radio, codec e audio simulati
docs/            protocollo, audio, pinout (T3 V1.6.1 e T-Beam V1.2), progetti simili
```

## Test

```
make test
```

Serve solo un compilatore C++17 (g++ o clang++).

La pagina web di installazione e configurazione ha una prova a parte (`make test-page`, che richiede Node e Playwright con Chromium): apre `site/index.html` in un browser con un dispositivo seriale finto e controlla lettura e scrittura delle impostazioni, misura del fondo, verifica audio, messaggi di errore e tema scuro. In GitHub Actions gira a ogni modifica e **blocca la pubblicazione su Pages** se fallisce.

## Note per l'uso in aria

- Il nominativo è trasmesso in chiaro e i contenuti non sono cifrati.
- La frequenza in `src/user_config.h` è un segnaposto: scegliere il canale secondo il band plan
  IARU Regione 1 / ARI per i modi digitali sui 70 cm.
- Collegare l'antenna prima di accendere la scheda.

## Sostieni il progetto

VoLoRa è software libero (GPL-3.0) e lo resterà: il sostegno è del tutto volontario e non dà diritto a nulla in cambio, né funzioni né assistenza. Se ti fa piacere aiutarlo a crescere (schede e componenti per le prove, tempo per lo sviluppo), puoi offrire un caffè con [PayPal.Me](https://www.paypal.me/maccalampo). Grazie, e buoni collegamenti. 73 de IU5HJU
