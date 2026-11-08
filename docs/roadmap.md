# Roadmap e decisioni

Aggiornato l'8 novembre 2026 (sezione "Decisioni dell'8 novembre" sotto). #### Tre profili (deciso l'8 novembre, sera)

| Profilo | Codec2 | Radio | Uso | TOT |
|---|---|---|---|---|
| **Messaggio** (lungo raggio estremo) | 450 o 700C, da scegliere ad orecchio | **SF12/BW125** (+13 dB) | registra e invia, ricezione che memorizza e riproduce | limite in tempo in aria (parametro, per esempio 90 s) |
| **Standard** (oggi) | 1200 | SF8/BW250 | tempo reale | 180 s (regola del 5 novembre) |
| **Qualità** | **3200**, ripiego **2400** se la CPU non regge | **SF7/BW250** (-2,5 dB) | tempo reale, breve distanza | 120 s (41 % in aria: 60 s / 0,41 = 146 s, gradino 120) |

- **Il ripiego sul 2400 dipende dalla CPU, non dal tempo in aria**: il 3200 sta nel budget del 70 %. Il codec 3200 usa blocchi da 20 ms: la catena audio va portata a due blocchi per frame da 40 ms.
- **Messaggio:** a SF12/BW125 un messaggio di 10 secondi di voce a 450 bps richiede circa 53 s in aria (65 s a 700C, 91 s a 1200). Con un limite di 90 s in aria la voce massima è circa 17 s a 450 e 14 s a 700C. SF11/BW125 (+10,5 dB) è l'alternativa se serve più voce per messaggio.
- **Tre canali:** una stazione ascolta un solo profilo alla volta, anche tra Standard e Qualità che hanno la stessa banda; servono tre frequenze concordate, da scegliere con il piano di banda (voce "Frequenza di lavoro" in "Aperto").

Ordine di lavoro concordato:

1. **Verifiche** (nessun codice di prodotto): che `esp32_codec2` supporti i modi 450, 700C, 2400 e 3200; CPU e RAM per modo sulla T3 (in particolare 3200 in tempo reale); **ascolto comparativo** dei modi sulla stessa registrazione (estensione di `CFG TEST CODEC`) per scegliere il codec del messaggio.
2. **Qualità in tempo reale** (3200 o 2400): modi con dimensioni di frame diverse e blocchi da 20 ms, impostazioni radio per profilo, sequenza a 16 bit.
3. **Messaggio vocale**: ricezione che memorizza e riproduce, interleaving, limite in tempo in aria.
4. **Misure sul campo** (RSSI, SNR, errore di frequenza e perdite per pacchetto, anche in movimento) per decidere quanta protezione serve davvero.

## 3. Pagina web di configurazione (dopo il TOT)

**Decisione del 4 novembre:** la prima versione passa dal cavo USB (Web Serial, Chrome/Edge) ed è
ospitata su GitHub Pages, insieme al flasher. Il Wi-Fi sulla scheda resta come seconda fase.
Fatto: impostazioni nella memoria della scheda (nominativo, frequenza, potenza) e comandi `CFG` sulla
seriale, con test sul PC ([firmware.md](firmware.md#impostazioni-senza-ricompilare)).
Fatta anche la pagina (`site/index.html`, flasher e configurazione via Web Serial), pubblicata da Actions su GitHub Pages; provata con Chrome su Linux. 

- Entrata con pressione lunga (3 s) sul pulsante volume (GPIO15) a scheda ferma; in quella modalità la **trasmissione è disattivata**.
- La scheda crea un Wi-Fi proprio (`LoRaCodec2-XXXX`, password sull'OLED); pagina su `192.168.4.1`; salvataggio in memoria non volatile e riavvio.
- Impostazioni: nominativo, frequenza (limitata ai 70 cm), SF/BW/CR/potenza/sync, parità, **TOT**, livello/profilo, guadagno microfono, AGC, volume.
- Regole di validazione nel core (testabili sul PC); server web con le librerie già incluse nell'ESP32.

## Prova di collegamento (fatta e provata)

Sequenza di frame noti confrontata bit per bit, con risposta automatica; vedi [prova-collegamento.md](prova-collegamento.md). Provata sulle schede il 5 novembre. Livelli 2 e 3 (autotest del codec, prova dell'altoparlante) da fare con l'hardware audio.

## Fondo del microfono: misura a cura dell'operatore (fatto il 5 novembre, da provare)

Il rumore di fondo cambia con l'ambiente (casa, auto, portatile, strada): la soglia dell'AGC non può essere una costante né, per ora, un automatismo. Soluzione semplice: **un valore standard (-60 dBFS) che ognuno misura e corregge**.

- Nella pagina di configurazione il pulsante **"Misura il fondo"**: stare in silenzio, 3 secondi; il valore trovato (90° percentile del livello dell'ingresso dell'AGC) viene salvato e vale subito. Una misura instabile (colpo, voce) non viene salvata.
- Poi **− e +** (e il campo numerico) alzano o abbassano il fondo di 1 dB per adattare l'AGC al proprio hardware, con effetto immediato. Range da -80 a -36 dBFS, standard -60; la soglia dell'AGC è il fondo + 6 dB.
- Firmware: `CFG MIC MEASURE`, `CFG SET MICFLOOR`, impostazione salvata in NVS. Il guadagno massimo (30 dB) resta in `user_config.h`.
- **Da decidere dopo l'ascolto della voce** (serve l'altoparlante): se serve anche la calibrazione automatica o l'adattamento continuo. Vedi [taratura-microfono.md](taratura-microfono.md).

## Verifica dell'hardware audio (scritta il 5 novembre, da provare)

Per un collega con una sola scheda: prova del microfono (silenzio contro voce) e prova combinata di altoparlante e microfono (tre toni misurati con Goertzel), dalla pagina di configurazione, con la spiegazione di falsi positivi e negativi.
Vedi [verifica-hardware-audio.md](verifica-hardware-audio.md). Le soglie vanno messe a punto sulle schede vere.

## Versioni (deciso il 7 novembre)

Numerazione semantica con beta e rc ([versioni.md](versioni.md)). La prima etichetta, `v0.1.0-beta.1`, si mette quando la voce passa da una scheda all'altra ed e' comprensibile all'ascolto.

## 4. Flasher web (dopo la configurazione a runtime)

- Pagina statica con `esp-web-tools`, un manifest per versione e tipo (installazione di fabbrica, aggiornamento), come nell'esempio di CA2RXU.
- I binari li produce il workflow `.github/workflows/build.yml`: `web_factory.bin` (all'indirizzo 0) e `ota_update.bin` (a 0x10000), con tag `v*` anche in una release di GitHub.
- Il nominativo di default è **N0CALL** con trasmissione bloccata, quindi il binario è distribuibile; nominativo e frequenza si impostano con la pagina di configurazione.
- Licenza scelta (GPL-3.0): chi distribuisce i binari deve rendere disponibile il sorgente corrispondente.

## 5. T-Beam V1.2 (batteria e GPS)

- **Fase 1** (fatta e provata il 4 novembre: avvio, AXP2101, radio, tasto IO38, trasmissione): alimentazione AXP2101 (radio accesa, GPS spento), tasto di bordo IO38 come PTT, pin audio propri (microfono su GPIO36, altoparlante su GPIO2), due ambienti `lora32` e `tbeam`, compilazione di entrambi su Actions.
- **Fase 2**: batteria e carica sull'OLED e in seriale, avviso di batteria scarica con un bip.
- **Fase 3**: GPS: locatore Maidenhead e ora precisa, per esempio nell'intestazione di inizio e fine turno.
- Se la scheda risultasse con radio SX1262 serve una variante di `RadioLora`.

## Aperto

- Frequenza di lavoro: ancora il segnaposto 433,900 MHz in `src/user_config.h`, da scegliere col band plan IARU R1 / ARI.
- Licenza: scelta la **GPL-3.0** (file `LICENSE`), coerente con la libreria `esp32_codec2` (vedi [firmware.md](firmware.md)).
- Prove su scheda: collegamento nei due versi tra T-Beam e T3, microfono, prova di collegamento (5 novembre); altoparlante e voce a turni ancora da provare (vedi [prove.md](prove.md)).
