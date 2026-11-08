# Roadmap e decisioni

Aggiornato l'8 novembre 2026 (sezione "Decisioni dell'8 novembre" sotto). Ordine di lavoro concordato: prima la voce a turni (manca l'hardware audio), poi TOT e profili.
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

### Decisioni dell'8 novembre 2026: profili e modi (ragionamento, non ancora implementato)

Definizioni (decise con IU5HJU):

- **Più efficiente** = più portata e più correzione degli errori, **a scapito della qualità audio**.
- **Alta qualità** = brevi distanze e la **massima fedeltà audio possibile**.

Principi, che completano la tabella del 5 novembre:

1. **Due assi indipendenti.** Il **modo del codec** (Codec2 450, 700C, 1200, 2400, 3200...) sta nel pacchetto e il ricevitore lo segue. Il **profilo radio** (SF, banda, coding rate, preambolo) no: il ricevitore LoRa ascolta **un solo** SF e una sola banda e non li riconosce da solo, quindi ogni profilo ha un **canale o una frequenza concordata**. Una scansione automatica dei profili richiede preamboli più lunghi (più tempo in aria): per ora no.
2. **Fino a SF10/BW250 la voce regge in tempo reale; oltre no.** Con SF10/BW125 o SF11-SF12 trasmettere dura più del parlato (tabella sotto). **Il lungo raggio estremo è quindi un messaggio vocale e non una conversazione** (deciso l'8 novembre): si registra il messaggio (la parte di trasmissione c'è già: `CFG REC ON`, vedi [registra-e-trasmetti.md](registra-e-trasmetti.md)), la scheda lo trasmette più lentamente del parlato, e chi riceve lo **memorizza e lo riproduce a ricezione conclusa**. Con il tempo a disposizione si può fare **interleaving** profondo o ripetere i frame in momenti diversi, senza costo di tempo in aria (utile contro ombre da ostacoli e fading).
3. **TOT in tempo in aria**, come già deciso (budget di 60 s in aria per turno). Per il messaggio il limite è sul tempo in aria: a SF12/BW125 sono circa 12 s di voce per ogni minuto in aria.
4. **Qualità massima con il Codec2 = 3200 bps** su **SF7/BW250** (deciso l'8 novembre: la stessa banda di oggi, -2,5 dB di sensibilità, 41 % in aria). La BW500 resta un'opzione per dopo (più tempo libero, ma -5,5 dB e banda larga da verificare con il piano di banda). Oltre il Codec2 (banda stretta, circa 4 kHz) servirebbe **Opus a banda larga** (16-24 kbit/s) su SF6/BW500: da valutare dopo le misure (CPU dell'ESP32). La fedeltà dipende anche dall'hardware: l'altoparlante da 3 W senza contenitore ha una risposta molto irregolare ([prove.md](prove.md)).

Numeri calcolati con la formula di Semtech (stime, rumore di 6 dB, 12 frame per pacchetto, parità 1 su 4, pause da 80 ms). "In aria" e "ciclo" sono percentuali del tempo reale della voce; la sensibilità è rispetto al profilo di oggi (1200, SF8/BW250):

| Profilo | Codec2 | LoRa | Sensibilità | In aria | Ciclo | Uso |
|---|---|---|---|---|---|---|
| Lungo raggio | 450 | SF9/BW250 | +2,5 dB | 34 % | 55 % | tempo reale |
| Lungo raggio | 450 | SF10/BW250 | +5 dB | 64 % | 85 % | tempo reale, al limite |
| Messaggio | 450 | SF10/BW125 | +8 dB | 129 % | 149 % | messaggio |
| Messaggio | 450 | SF11/BW125 | +10,5 dB | 274 % | 295 % | messaggio |
| Messaggio | 450 | SF12/BW125 | +13 dB | 506 % | 526 % | messaggio |
| **Standard (oggi)** | 1200 | SF8/BW250 | 0 dB | 33 % | 54 % | tempo reale |
| **Qualità (scelta)** | 3200 | SF7/BW250 | -2,5 dB | 41 % | 62 % | breve distanza, tempo reale |
| Qualità (ripiego) | 2400 | SF7/BW250 | -2,5 dB | 33 % | 53 % | breve distanza, tempo reale |
| Qualità | 3200 | SF8/BW500 | -3 dB | 37 % | 58 % | breve distanza |
| Qualità | 3200 | SF7/BW500 | -5,5 dB | 21 % | 42 % | breve distanza |

Le **pause da 80 ms** tra i pacchetti valgono circa il 21 % del tempo di voce (5 pause ogni 1,92 s): ridurle vale quanto cambiare codec. Più protezione costa tempo: con CR 4/8 e 2 pacchetti di parità su 4, SF9/BW250 sale all'81 % e SF10/BW250 al 131 % (non regge). Quanta protezione serve dipende da come sono fatte le perdite sul campo.

Differenze rispetto alla tabella del 5 novembre: il livello Qualità diventa 3200 su SF7/BW250 (non 2400 su BW250, né BW500) e il livello 1 (450 su SF9/BW125) è sostituito dal profilo Messaggio qui sotto. **Le bande strette (62,5 kHz e meno) richiedono un quarzo preciso (TCXO)** da verificare sulle schede; la **BW500**, se mai servirà, va verificata con il piano di banda.

#### Tre profili (deciso l'8 novembre, sera)

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
