# LoRa-Codec-2-RTX

Messaggi vocali broadcast a turni (stile QSO) tra schede LilyGo a 433 MHz (LoRa32 T3 V1.6.1 e T-Beam V1.2), per radioamatori.
La voce è compressa con Codec2 e trasmessa con LoRa, con tre **profili** da scegliere dal menu della scheda:

| Profilo | Radio | Codec2 | Uso |
|---|---|---|---|
| **Standard** (S) | SF8, banda 250 kHz | 1200 bps | voce in tempo reale, come una conversazione |
| **Messaggio** (M) | SF12, banda 125 kHz | 1200 bps | lungo raggio: si registra un messaggio (fino a 30 s) e chi riceve lo ascolta a ricezione conclusa, con la percentuale sul display |
| **Qualità** (Q) | SF7, banda 250 kHz | 3200 bps | voce in tempo reale più naturale, meno portata |

Funzionamento in breve:

1. Si preme il PTT: il microfono viene bufferizzato subito.
2. Si trasmette il nominativo, poi la voce Codec2 man mano che si parla.
3. Al rilascio del PTT (massimo 30 s) si invia l'ultimo audio e il segnale di fine turno (OVER).
4. Tutte le stazioni in ascolto sulla frequenza ricevono e riproducono. Dopo 5-10 s di attesa si risponde.

> **Progetto sperimentale (beta).** La voce passa da una scheda all'altra ed è **provata sulle schede** (la T3 con microfono e
> altoparlante, nei tre profili, in collegamento con la T-Beam; 10/11/2026: esiti in [docs/prove.md](docs/prove.md)). **La T-Beam di
> prova non ha ancora microfono né altoparlante**: la sua parte audio non è provata. **Manca ancora la misura sul campo**: distanze
> lunghe, mezzi in movimento. Possono trasmettere solo radioamatori con il proprio nominativo, sulle frequenze e con le
> potenze consentite. Il nominativo viaggia in chiaro e i contenuti non sono cifrati.

> **Nuovo? Parti dalla [guida rapida](docs/guida-rapida.md).** Indice della documentazione: [docs/README.md](docs/README.md).

## Per provare il firmware

**Occorrente per ogni stazione:**

- scheda **LILYGO LoRa32 T3 V1.6.1** oppure **T-Beam V1.2 (AXP2101)**, a 433 MHz, con **antenna a 433 MHz** (SMA);
- un **pulsante PTT** esterno tra GPIO0 e massa;
- per la voce: microfono **INMP441**, amplificatore **MAX98357A** e altoparlante 8 Ω 3 W (per il solo test radio senza audio non servono);
- schema dei collegamenti in [docs/pinout.md](docs/pinout.md) (con le immagini dei collegamenti di T3 e T-Beam) e [docs/audio.md](docs/audio.md).

**Regole di sicurezza:** antenna collegata **prima** del cavo USB, mai alimentare la scheda senza antenna o carico fittizio; microSD tolta durante il caricamento.

**Passi:**

1. **Dal browser (il modo più semplice, anche senza Linux).** Apri la [pagina di installazione](https://iu5hju.github.io/LoRa-Codec-2-RTX/) con Chrome, Edge od Opera, collega la scheda con il cavo USB, scegli la **versione dal menu a tendina** (quella proposta va bene per chi non sa quale scegliere: stabile, o beta finché non ce n'è una) e premi "Installa il firmware". Poi, nel passo 2 della stessa pagina, imposta **nominativo**, frequenza, potenza e volume: restano salvati nella scheda. Il firmware pubblico ha `N0CALL` e **non trasmette** (riceve soltanto) finché non imposti il tuo nominativo. La configurazione si può fare anche **senza cavo e senza PC**: dal menu della scheda (volume lungo, voce "Wi-Fi") la scheda crea una rete con la sua pagina su `192.168.4.1` ([docs/firmware.md](docs/firmware.md)).
2. **In alternativa.** Scarica i binari dall'esecuzione di GitHub Actions (artefatto `firmware-t3-v161` o `firmware-tbeam-v12`, serve essere loggati; contengono `web_factory.bin` e `ota_update.bin`) e caricali con `web_factory.bin` all'indirizzo `0x0` ([docs/firmware.md](docs/firmware.md), anche per Windows), oppure compila in locale con PlatformIO. Il nominativo non richiede di ricompilare: si imposta dalla pagina o con `CFG SET CALLSIGN` e resta salvato. Se preferisci un firmware con il tuo nominativo già dentro: fai un **fork**, cambia `LCR_CALLSIGN` in `src/user_config.h` dal sito GitHub e lascia compilare Actions (non usare binari di altri che ne contengono uno diverso). Dalla seriale si cambia tutto con i comandi `CFG` ([docs/firmware.md](docs/firmware.md)).
3. **Frequenza.** Il valore predefinito è un **segnaposto**: scegli il canale con il band plan IARU R1 / ARI. Le stazioni che si ascoltano devono avere gli **stessi** parametri LoRa e la stessa frequenza.
4. **Prova di collegamento (senza microfono né altoparlante).** Su due schede attiva la prova (tre pressioni veloci del PTT, il pulsante nella pagina o `CFG TEST ON`) e premi il PTT: la scheda ricevente confronta una sequenza nota bit per bit, mostra l'esito sul display e risponde. Dettagli in [docs/prova-collegamento.md](docs/prova-collegamento.md).

## Per chi collabora

- **`main` è il punto comune di partenza**: è sempre compilabile (GitHub Actions deve essere verde) e ha come nominativo `N0CALL`, quindi non trasmette finché non imposti il tuo.
- **Come contribuire:** fai un fork, lavora su un ramo tuo e proponi le modifiche con una pull request. Prima di proporle esegui `make test`.
- **Prove:** annota ogni prova in [docs/prove.md](docs/prove.md) con il modello indicato. Per un problema da risolvere apri una segnalazione (Issues).
- **Decisioni e ordine dei lavori:** [docs/roadmap.md](docs/roadmap.md). Prossimi passi: misure sul campo (RSSI, SNR, perdite, anche in movimento), intestazione del messaggio, memorie con profilo e frequenze per canale, locatore Maidenhead e ora dal GPS; la ripetizione dei messaggi solo dopo la verifica normativa.

## Licenza

Copyright (C) 2026 IU5HJU. Questo programma è software libero, distribuito sotto la
**GNU General Public License versione 3** (vedi [LICENSE](LICENSE)), senza alcuna garanzia.
La scelta deriva dalla libreria Codec2 per ESP32 usata (GPL-3.0); dettagli sulle librerie in
[docs/firmware.md](docs/firmware.md).

Protocollo e parametri: [docs/protocollo.md](docs/protocollo.md). Audio e collegamenti: [docs/audio.md](docs/audio.md). Taratura del microfono, misure e come adattarla: [docs/taratura-microfono.md](docs/taratura-microfono.md). Firmware e primo avvio: [docs/firmware.md](docs/firmware.md). Decisioni e prossimi passi: [docs/roadmap.md](docs/roadmap.md). Registro delle prove: [docs/prove.md](docs/prove.md). Prova di collegamento: [docs/prova-collegamento.md](docs/prova-collegamento.md). Verifica del montaggio audio con una sola scheda: [docs/verifica-hardware-audio.md](docs/verifica-hardware-audio.md). Ricezione e decodifica con un SDR: [docs/sdr.md](docs/sdr.md). Numerazione delle versioni (beta, rc, stabile): [docs/versioni.md](docs/versioni.md). Alimentazione e batteria della T-Beam: [docs/alimentazione.md](docs/alimentazione.md). Schermate dell'OLED: [docs/display.md](docs/display.md). Modi del Codec2 e confronto: [docs/codec2.md](docs/codec2.md). Profilo Messaggio: [docs/modo-messaggio.md](docs/modo-messaggio.md). Progetti simili e scelte che ne derivano: [docs/progetti-simili.md](docs/progetti-simili.md).

## Stato

Aggiornato il 10/11/2026.

| Parte | Stato |
|---|---|
| Formato pacchetti (protocollo v2), parità XOR, sessioni TX/RX, pausa tra i pacchetti | fatto, testato sul PC |
| Voce a turni in Standard (Codec2 1200, SF8/BW250) | **provata** sulla T3 (con microfono e altoparlante); la T-Beam di prova non ha ancora l'audio; cause dei crepitii trovate e corrette |
| Profilo **Messaggio** (SF12/BW125): registra e invia, riproduzione a messaggio completo, percentuale e suono "MSG" in Morse | **provato sulle schede** |
| Profilo **Qualità** (Codec2 3200, SF7/BW250) | **provato sulle schede**: suona meglio del 1200; CPU misurata sulla scheda: codifica 10,0 ms e decodifica 13,3 ms per frame da 40 ms (25% e 33%) |
| Menu a bordo (volume lungo, PTT, conferma; riavvio sul profilo scelto) e lettera S/M/Q sul display | **provato** |
| Configurazione dal Wi-Fi della scheda (voce "Wi-Fi", pagina su 192.168.4.1, password salvata) | **provata** |
| Volume con guadagno (passo 5 = 0 dB, fino a +15 dB) | **provato** all'aperto |
| Posizione (spenta di default), QTH fisso, distanza dalla stazione ricevuta; GPS sulla T-Beam | **provati** (fix con 4 satelliti) |
| Impostazioni nella scheda e comandi `CFG` | fatti e provati; salvate in NVS |
| Pagina web di installazione e configurazione (GitHub Pages, Web Serial) con menu delle versioni | **online** e provata |
| Prova di collegamento (frame noti, confronto bit per bit) | fatta e **provata sulle schede** |
| Ricezione con SDR (NESDR, GNU Radio): decodifica e ascolto in tempo reale, anche in Qualità | fatta e **provata** in Standard, in Messaggio e in Qualità (10/11/2026) |
| Misure sul campo, distanze lunghe, mezzi in movimento | **da fare** |
| Ripetizione dei messaggi, memorie, locatore Maidenhead | da fare ([roadmap](docs/roadmap.md)) |

## Struttura

```
lib/voicecore/   logica del protocollo, indipendente dalla scheda
src/             firmware per le schede supportate e impostazioni dell'operatore (user_config.h)
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

LoRa-Codec-2-RTX è software libero (GPL-3.0) e lo resterà: il sostegno è del tutto volontario e non dà diritto a nulla in cambio, né funzioni né assistenza. Se ti fa piacere aiutarlo a crescere (schede e componenti per le prove, tempo per lo sviluppo), puoi offrire un caffè con [PayPal.Me](https://www.paypal.me/maccalampo). Grazie, e buoni collegamenti. 73 de IU5HJU
