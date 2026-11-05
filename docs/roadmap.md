# Roadmap e decisioni

Aggiornato il 5 novembre 2026. Ordine di lavoro concordato: prima la voce a turni (manca l'hardware audio), poi TOT e profili.
Già fatti, in anticipo: pagina web di installazione e configurazione, impostazioni nella scheda, prova di collegamento.

## 1. Prima trasmissione (fatta; resta la voce)

Procedura in [firmware.md](firmware.md). Stato al 5 novembre: **collegamento radio provato nei due versi** tra T-Beam e T3, microfono provato sulla T3, prova di collegamento funzionante. Manca la voce: servono amplificatore e altoparlante (spedizione parziale in attesa).
Regola ferma: **mai alimentare una scheda senza antenna**, nemmeno via USB per caricare il firmware.

## 2. TOT fino a 3 minuti e profili (dopo la prima trasmissione)

Decisioni prese:

- Il TOT diventa un'impostazione a gradini: **30 s, 60 s, 120 s, 180 s**. Per le prove resta 30 s.
- **Budget di tempo in aria per turno: 60 s.** Occupazione massima del canale: **70 %** del tempo di voce, parità compresa.
- Regola: TOT massimo = il gradino più grande che sta sotto `60 s / occupazione`, con tetto 180 s.
- Il livello è un accordo di rete: i parametri LoRa devono essere uguali tra le stazioni sulla stessa frequenza. Il modo Codec2 è nell'header e il ricevitore si adatta da solo.

Scala a gradini proposta (stime teoriche, da verificare sul campo):

| Livello | Codec2 | LoRa | Sensibilità vs SF7/BW250 | Occupazione | TOT max |
|---|---|---|---|---|---|
| 1 Massima copertura | 450 (**provvisorio**) | SF9/BW125 | +8,0 dB | 69 % | 60 s |
| 2 Efficienza | 700C | SF8/BW125 | +5,5 dB | 48 % | 120 s |
| 3 Standard | 1200 | SF8/BW250 | +2,5 dB | 33 % | 180 s |
| 4 Qualità | 2400 | SF7/BW250 | 0 dB | 33 % | 180 s |

**Livello 1 da decidere ad orecchio.** L'esperienza con FreeDV dice che già il 700C è molto "robotico" e il 450 lo sarà ancora
di più. Alternative da confrontare sul campo: 450 con parità, 700C a SF9/BW125 senza parità (68 %), 700C a SF8/BW125.
"700D" non è un modo di Codec2 ma un modem FreeDV che usa il vocoder 700C.

Modifiche al codice previste:

- sequenza dei pacchetti a 16 bit e finestra scorrevole in ricezione (al posto dei 128 slot fissi);
- frame di dimensione variabile in bit (700C: 28 bit, 450: 18 bit) con impacchettamento a bit;
- modi a 20 ms (2400, 3200) come due blocchi per frame audio;
- tabella dei profili nel core, guidata da dati, con test sul PC che verifica il limite del 70 % per ogni profilo;
- facoltativo: bip a 10 s dalla fine del TOT;
- il formato dei pacchetti cambia: le due schede devono avere la stessa versione.

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
