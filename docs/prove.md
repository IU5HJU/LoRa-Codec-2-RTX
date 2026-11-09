# Registro delle prove

Qui si annotano le prove fatte sulle schede, da chiunque partecipi, in modo che i risultati restino
nel repository. Una prova per sezione, in ordine cronologico, la più recente in fondo.
Per un problema da risolvere apri invece una segnalazione (Issues) e collega la prova.

> **Prima di ogni prova:** antenna a 433 MHz collegata a entrambe le schede **prima** del cavo USB,
> microSD tolta, PTT non premuto all'accensione. Trasmetti solo con il tuo nominativo, su una frequenza
> e con una potenza consentite. Procedura di caricamento in [firmware.md](firmware.md).

## Quanto annotare per ogni prova

| Voce | Esempio |
|---|---|
| Data e luogo | 03/11/2026, casa |
| Chi | IU5HJU |
| Versione del firmware | commit `0466536`, artefatto di Actions o pagina web, ramo `main` |
| Schede | 2 × LoRa32 T3 V1.6.1 |
| Collegato | solo radio / microfono / altoparlante / tutto |
| Frequenza e parametri | 433,900 MHz, SF8, BW250, CR 4/5, 10 dBm, parità on |
| TOT | 30 s |
| Antenne | stilo originale, SMA |
| Distanza e ambiente | 5 m, stessa stanza |
| Risultato | cosa si è visto e sentito |
| Problemi | messaggi dell'OLED, righe della seriale |

## Scaletta consigliata

Ogni livello presuppone che il precedente funzioni.

1. **Avvio.** L'OLED mostra "AVVIO" e poi "ASCOLTO" con nominativo e frequenza. Con N0CALL mostra "SOLO RX".
2. **Solo radio.** Premi il PTT su una scheda (anche senza microfono): l'altra passa a "RX" con il nominativo, l'**RSSI** (dBm) e l'**SNR** (dB) dell'ultimo pacchetto ricevuto. Per un esito certo usa la [prova di collegamento](prova-collegamento.md) (`CFG TEST ON` su entrambe).
3. **Microfono.** Parla con il PTT premuto: sull'altra scheda deve arrivare audio (anche con l'altoparlante non ancora collegato non si rompe nulla; si guardano le statistiche "persi" e "recup." a fine ricezione).
4. **Altoparlante.** Controlla volume, bip di fine trasmissione e di fine ricezione, assenza di fruscio a riposo.
5. **Voce a turni.** Turno di qualche secondo, attesa di 5-10 s, risposta. Qualità e comprensibilità.
6. **Portata.** Allontana le schede a passi, annotando distanza, RSSI, SNR e comprensibilità. Con SF8 il pacchetto si demodula fino a circa -10 dB di SNR (valore teorico): più ci si avvicina a quel limite, più i frame persi aumentano.

## Aiuto per la diagnosi

- Seriale della scheda: `pio device monitor` a 115200 baud, oppure un qualunque terminale seriale.
  A fine di ogni ricezione stampa una riga pronta per il registro, per esempio
  `RX fine: IU5HJU RSSI -92 dBm SNR 7.5 dB, frame persi 0, pacchetti recuperati 1`.
- **Perdite a distanza ravvicinata:** a fine ricezione la seriale stampa anche `[rx] pacchetti letti N, errori CRC N, sovrascritti N, ritardo massimo di lettura N ms`. "Sovrascritti" conta i pacchetti arrivati prima di aver letto il precedente (il FIFO della radio contiene un solo pacchetto): se è maggiore di zero la scheda ricevente è troppo lenta tra un pacchetto e l'altro. "Errori CRC" conta i pacchetti arrivati danneggiati.
- **Voce debole o "gracchiante" (radio a posto, `frame persi 0`):** la riga `[mic] ... uscita al codec X dBFS` della scheda che trasmette dice il livello che va al codec (con la voce deve stare vicino a -24 dBFS, il target dell'AGC); a fine ricezione la scheda che riceve stampa `[rx-audio] N frame: decodificato rms X dBFS (max Y), uscita rms Z dBFS (max W), guadagno AGC A...B dB, limitatore su N frame, volume V dB`. Il parlato decodificato di una voce normale sta tra -40 e -20 dBFS; con il volume a 0 dB l'uscita segue l'AGC di riproduzione (target -24 dBFS). Un decodificato molto basso (sotto -50) indica che il codec ha ricevuto poco segnale (microfono, fondo troppo alto, AGC); un decodificato normale ma un'uscita bassa o un limitatore sempre attivo punta alla riproduzione (volume, AGC di uscita, amplificatore).
- **Riavvio continuo con `A stack overflow in task loopTask has been detected` sulla seriale:** lo stack del task principale è troppo piccolo. Corretto portandolo a 32 kB (3 novembre 2026); se ricompare, indica il punto in cui si ferma con le righe `[avvio]`.
- **"ERRORE" sull'OLED:** il nome indica il blocco (Codec2, audio I2S, radio); il codice è nella seriale.
- **Livello del microfono:** con il PTT premuto la seriale stampa due volte al secondo `[mic] livello -38 dBFS, picco -25 dBFS, offset 0.4%, AGC in -45 dBFS, guadagno 12 dB, errori 0`. Parlando vicino al microfono il picco deve salire a circa -30...-6 dBFS; sotto -80 è silenzio (cavi o `LCR_MIC_SLOT`), oltre -3 è saturazione. Un offset continuo molto alto (decine di %) indica un collegamento errato. **"AGC in"** è il livello che l'AGC vede (dopo il passa-alto): se in silenzio sta sopra la soglia il fondo del microfono (soglia dell'AGC = fondo + 6 dB, standard fondo -60 dBFS: si misura dalla pagina di configurazione) il guadagno sale al massimo e il fondo viene amplificato come voce. Il fondo si misura con "Misura il fondo" nella pagina di configurazione; il guadagno massimo è `LCR_MIC_AGC_MAX_GAIN_DB`.
- **Si sente solo silenzio:** prova `LCR_MIC_SLOT 1` in `src/user_config.h`.
- **Volume dimezzato o basso:** verifica il pin `SD` dell'amplificatore e l'alimentazione (5 V o 3,3 V).
- **"CANALE OCCUPATO" a canale libero:** il rilevamento del canale ha rilevato attività; annota quando e dove.
- **"STAI RICEVENDO":** la scheda sta ancora riproducendo una trasmissione; è normale per qualche secondo.

## Registro

### Modello da copiare

```
### GG/MM/AAAA, luogo, chi
- Firmware: (commit o artefatto)
- Schede e collegamenti:
- Parametri radio (frequenza, SF, BW, CR, potenza, parità, TOT):
- Antenne:
- Distanza e ambiente:
- Livello raggiunto della scaletta (1-6):
- Risultato (cosa si vede e si sente, RSSI, SNR, "persi" e "recup."):
- Problemi (OLED, seriale):
- Da fare / idee:
```

### Prove effettuate

> **Nota sul ramo `prova-iu5hju`.** Le prove fino al 5 novembre 2026 citano il ramo `prova-iu5hju`: era `main` con il nominativo dell'autore compilato nel firmware. Dal 5 novembre il nominativo si imposta dalla pagina web e il ramo, come `tbeam-fase1`, è stato eliminato: la sola differenza con `main` era quella riga. Le voci seguenti restano com'erano scritte.

### 03/11/2026, casa, IU5HJU (prova con una sola scheda)

- Firmware: ramo `prova-iu5hju`, prima versione (avvio bloccato) e poi commit `4e8395a` (con stack a 32 kB).
- Schede e collegamenti: **una** LoRa32 T3 V1.6.1, antenna a 433 MHz, OLED integrato. Nessun microfono, amplificatore o altoparlante collegato. PTT simulato con un filo tra IO0 e GND.
- Parametri radio: 433,900 MHz (segnaposto), SF8, BW250, CR 4/5, 10 dBm, parità on, TOT 30 s.
- Livello raggiunto della scaletta: **1 (avvio)** completato, **2 (solo radio)** a metà: la trasmissione parte, la ricezione non è ancora verificata perché manca la seconda scheda.
- Risultato:
  - versione iniziale: la scheda restava su "AVVIO" e si riavviava in continuo; la seriale mostrava `A stack overflow in task loopTask has been detected`. Causa: stack del task principale di 8 kB, insufficiente per Codec2. **Corretto** portandolo a 32 kB;
  - versione corretta: avvio completo fino a "ASCOLTO" con nominativo, frequenza e volume;
  - PTT col filo: l'OLED mostra "TX IU5HJU" e "Trasmissione 18/30 s", il contatore avanza.
- Problemi: nessuno rimasto aperto con una sola scheda.
- Da fare: seconda scheda in arrivo (4 novembre) con microfono, amplificatore e altoparlante; poi provare ricezione, RSSI, SNR, "persi" e "recup."

### 04/11/2026, casa, IU5HJU (T-Beam V1.2 trasmette, LoRa32 T3 riceve)

- Firmware: ramo `prova-iu5hju` (commit `c110c7c`). T-Beam compilata e caricata in locale con PlatformIO (`pio run -e tbeam -t upload`) da un portatile Ubuntu; firmware della T3 da confermare.
- Schede e collegamenti: T-Beam V1.2 (AXP2101, OLED montato, antenna a 433 MHz) e LoRa32 T3 V1.6.1; nessun microfono, amplificatore o altoparlante collegato. Distanza: 1-2 m.
- Parametri radio: 433,900 MHz (segnaposto), SF8, BW250, CR 4/5, 10 dBm, parità on.
- Livello raggiunto della scaletta: **2 (solo radio)**, in un verso (T-Beam -> T3).
- Risultato:
  - avvio della T-Beam completo, senza errori: `[pmu] AXP2101 ok`, `radio ok`, stack libero minimo del task principale 23.700 byte su 32.768 (margine ampio), heap libero 213 KB dopo l'audio;
  - nessun avviso di pin PTT instabile: il tasto **IO38** funziona come PTT;
  - premuto IO38, la T-Beam mostra "TX IU5HJU" e la T3 riceve con **SNR circa 11 dB** (RSSI da annotare).
- Problemi: la batteria risulta "presente, 1837 mV, 0 %, carica in corso": valore anomalo per una Li-ion (cella molto scarica o lettura spuria), da controllare.
- Da fare: annotare l'RSSI e i valori "persi"/"recup." dopo la ricezione; provare il verso opposto (T3 -> T-Beam); aggiungere microfono, amplificatore e altoparlante.

### 04/11/2026, casa, IU5HJU (video della prova, T-Beam -> T3)

- Dal video: la T-Beam passa da "ASCOLTO" a "TX IU5HJU" con "Trasmissione 0/30 s" premendo IO38; la T3 mostra "RX IU5HJU" con **RSSI circa -28 dBm** e **SNR circa 9,3 dB** e, a fine ricezione, "Ultimo IU5HJU" con **"persi 0, recup. 3"**.
- Osservazione: a 1-2 m il segnale è fortissimo ma **tre pacchetti sono andati persi e sono stati recuperati dalla parità**. La parità funziona, ma le perdite a distanza così breve vanno spiegate. Ipotesi da verificare: i pacchetti di parità partono quasi attaccati al pacchetto dati precedente, e la T3 dopo ogni ricezione riarma la radio e può aver perso l'inizio del pacchetto successivo; oppure il ciclo principale della T3 era occupato (display, riproduzione) quando è arrivato il pacchetto.
- Problema aperto: la T-Beam V1.2 non si accende a batteria, solo da USB (cella a 1837 mV). Analisi in [alimentazione.md](alimentazione.md).

### 04/11/2026, casa, IU5HJU (batteria della T-Beam: lettura e carica)

- Firmware: ramo `prova-iu5hju` con la batteria sul display (`(Batt=x.xxV)` in basso a destra).
- **Lettura confermata dal tester:** la T-Beam mostra **2,90 V** e il tester sulla cella misura **2,90 V**. La misura dell'AXP2101 è accurata.
- **Andamento della carica da USB:** 1,84 V (avvio con il firmware di diagnostica) -> 2,70 V -> 2,90 V. La cella, scesa a 1,8 V, sta risalendo.
- Sotto 3,40 V il testo della batteria è in negativo, come previsto.
- Da fare: attendere la carica completa e provare l'avvio a batteria (tasto PWR); se non parte con una cella carica, leggere la soglia VOFF dalla seriale (`[pmu] soglia di spegnimento VOFF ...`).

### 04/11/2026, casa, IU5HJU (T-Beam V1.2: avvio a batteria)

- Cella carica (circa 4,2 V). **La T-Beam parte anche a batteria**, premendo PWR con l'USB scollegata: il mancato avvio era dovuto alla cella scarica (1,84 V), non al firmware.
- **La tensione mostrata sul display coincide con quella del tester**, anche a cella carica.
- Da fare: prova di durata della batteria in standby (radio in ascolto, OLED acceso), da interrompere quando il testo della batteria va in negativo (sotto 3,40 V) per non scaricare di nuovo la cella; provare una trasmissione col tasto IO38 a batteria; confrontare la lettura della LoRa32 T3 (ADC GPIO35) con il tester.

### 04/11/2026, casa, IU5HJU (verso opposto, T3 -> T-Beam)

- Schede e collegamenti: LoRa32 T3 V1.6.1 trasmette, T-Beam V1.2 riceve; microfono INMP441 arrivato e cablato sulla T3 (amplificatore e altoparlante non ancora arrivati).
- Livello raggiunto della scaletta: **2 (solo radio) completo nei due versi**. La T-Beam riceve correttamente dalla T3.
- Da annotare: RSSI, SNR, "persi" e "recup." di questa prova.
- Da fare: prova del microfono (livello 3); per sentire la voce serve ancora amplificatore e altoparlante.

### 05/11/2026, casa, IU5HJU (microfono INMP441 sulla T3, livelli e AGC)

- Firmware: `main`/`prova-iu5hju` dopo il commit "AGC: sotto la soglia il guadagno torna verso 12 dB", installato dalla pagina web (Chrome su Ubuntu; Chromium snap non vede la porta, serve Chrome `.deb`).
- Schede e collegamenti: LoRa32 T3 V1.6.1 con microfono INMP441 (BCLK 14, WS 13, dati 34, L/R a GND), senza amplificatore e altoparlante. PTT premuto, trasmissione disattivata.
- Livello raggiunto della scaletta: **3 (microfono)**, solo livelli in seriale; l'audio non è ancora stato ascoltato.
- Risultato:
  - prima della correzione, in silenzio: livello -46...-58 dBFS, AGC al massimo (36 dB), fondo amplificato quasi come voce; non dipendeva dalla trasmissione (provato con TX disattivata);
  - dopo (guadagno massimo 30 dB, soglia -54 dBFS, ritorno a 12 dB sotto la soglia): in silenzio "AGC in" -58...-61 dBFS e guadagno 12-13 dB; parlando vicino al microfono "AGC in" -24...-35 dBFS, guadagno 1-9 dB, picchi grezzi -13...-17 dBFS (nessuna saturazione);
  - il livello grezzo in silenzio è alto (-28...-31 dBFS, offset che oscilla fino a +-14%) ma quasi tutto sotto 300 Hz, tolto dal filtro: indica rumore elettrico o meccanico a bassa frequenza.
- Problemi: rumore a bassa frequenza alla sorgente, da capire (massa comune, VDD, lunghezza dei fili).
- Da fare: ascoltare la voce con amplificatore e altoparlante (in arrivo); verificare il rumore a bassa frequenza con fili più corti o un condensatore su VDD.

### 05/11/2026, analisi del codice (nessuna prova): perdite a distanza ravvicinata

Le tre perdite recuperate dalla parità a -28 dBm (prova del 4 novembre) hanno una spiegazione probabile, non ancora verificata sulle schede:

- La parità parte subito dopo il quarto pacchetto dati: nel codice precedente i pacchetti si trasmettevano uno per ciclo, senza pausa.
- Il ricevitore, dopo un pacchetto, deve leggere la radio, ridisegnare l'OLED (circa 25 ms) e scrivere l'audio sull'I2S (bloccante fino a 40 ms). La radio SX127x ha **un solo FIFO**: se il pacchetto seguente arriva prima della lettura, **sovrascrive** quello precedente, che si perde. La parità non viene persa, quindi il gruppo si recupera con la parità: coincide con quanto osservato (persi 0, recuperati 3).
- **Cambiato:** il trasmettitore aspetta 80 ms tra la fine di un pacchetto e l'inizio del successivo (`kInterPacketGapMs`), testato sul PC. Costa circa 80 ms per pacchetto, meno di due frame di voce.
- **Strumenti per verificare:** a fine ricezione la seriale stampa `[rx] pacchetti letti N, errori CRC N, sovrascritti N, ritardo massimo di lettura N ms`. Per la prova si ripete la trasmissione T-Beam -> T3 a 1-2 m: prima con il firmware vecchio se disponibile (sovrascritti > 0 conferma l'ipotesi), poi con quello nuovo (sovrascritti 0).
- Possibile seconda fase se non basta: leggere la radio da un task separato invece che dal ciclo principale.

### 05/11/2026, casa, IU5HJU (perdite a 1-2 m: T-Beam trasmette muta, T3 riceve con la diagnostica)

- Schede: T-Beam V1.2 trasmette senza microfono (PTT fino al TOT di 30 s); LoRa32 T3 V1.6.1 riceve, alimentata dall'USB del portatile, senza amplificatore e altoparlante.
- Risultato (T3, firmware con diagnostica):
  - `RX fine: ... RSSI -37 dBm SNR 12.2 dB, frame persi 0, pacchetti recuperati 0` e `[rx] pacchetti letti 33, errori CRC 0, sovrascritti 0, ritardo massimo di lettura 19 ms`;
  - `RX fine: ... RSSI -34 dBm SNR 12.2 dB, frame persi 0, pacchetti recuperati 0` e `[rx] pacchetti letti 79, errori CRC 0, sovrascritti 0, ritardo massimo di lettura 27 ms`.
- Con il firmware precedente alla pausa tra i pacchetti (T-Beam e T3), 30 s di PTT: nessuna perdita (3 ricezioni, persi 0, recuperati 0, RSSI da -43 a -47 dBm). Le perdite del 4 novembre non si riproducono.
- Il ricevitore impiega fino a circa 27 ms a leggere un pacchetto (ridisegno dell'OLED). Con altoparlante attivo sarà più lento: la pausa di 80 ms tra i pacchetti lascia margine, da ricontrollare con l'audio.
- Batteria T3: la lettura (4,24-4,42 V) è sopra il limite di una LiPo; da chiarire se c'è una batteria collegata (vedi sotto).

- Chiarimenti del 5 novembre: sulla T-Beam c'era il firmware con la pausa di 80 ms tra i pacchetti (nessuna perdita in 30 s, 79 pacchetti, nessuna sovrascrittura); la T3 era alimentata solo dall'USB, senza batteria, quindi i 4,24-4,42 V letti sono la tensione del caricatore e non una batteria. Il firmware ora mostra `(Batt=--)` sopra 4,35 V.

### 05/11/2026, casa, IU5HJU (prova di collegamento T-Beam <-> T3)

- Firmware: `main`/`prova-iu5hju` con la prova di collegamento (`CFG TEST ON`); due schede, nessun microfono, amplificatore o altoparlante.
- Risultato: la prova **funziona** nelle condizioni di prova (distanza ravvicinata): frame confrontati bit per bit, esito sull'OLED e risposta automatica del ricevitore. Valori numerici da aggiungere (RSSI, SNR, percentuale) alla prossima prova.
- Da fare: ripetere allontanando le schede (portata) e nel verso opposto; autotest del codec e prova dell'altoparlante quando arriva l'hardware audio.

### 05/11/2026, casa, IU5HJU (batteria T3, solo USB: il display passava di continuo da tensione a "--")

- Problema: con il limite fisso a 4,35 V la lettura del caricatore (4,14-4,42 V, oscilla) attraversava il limite di continuo e la seriale mostrava `-1.00 V` alternato a `4.33 V`; l'OLED passava da tensione a lineette.
- Decisione: sulla T3 la tensione si mostra **sempre**, senza lineette; la lettura è mediata (circa 2,5 s) per non far ballare le cifre e il testo va in negativo sotto 3,40 V come sulla T-Beam. La soglia a 4,35 V con "--" è stata scartata. Test sul PC con un caricatore simulato che oscilla per 10 minuti: il valore mostrato resta in una banda di meno di 0,06 V.
- Da fare: confrontare la lettura della T3 con una LiPo collegata e il tester.

### 06/11/2026, casa, IU5HJU (altoparlante e amplificatore sulla T3: bip)

- Firmware: `main`. LoRa32 T3 V1.6.1 con microfono INMP441, amplificatore MAX98357A e altoparlante 8 ohm collegati.
- Risultato: con tre clic veloci sul PTT (prova di collegamento attivata) **si sentono i bip** descritti (due acuti all'attivazione). L'amplificatore e l'altoparlante funzionano.
- Livello raggiunto della scaletta: **4 (altoparlante)**, a livello di bip. Da annotare: volume, fruscio a riposo, distorsione.
- Da fare: tabella di "Suona i toni" (sezione 3 della pagina) e prova senza altoparlante per i falsi positivi; poi la voce a turni con il secondo microfono.

### 07/11/2026, casa, IU5HJU (verifica dell'hardware audio sulla T3: microfono e toni)

- Firmware: `main` (pagina web, sezione 3). LoRa32 T3 V1.6.1 con INMP441, MAX98357A e altoparlante 8 ohm 3 W (senza contenitore).
- **Prova del microfono:** silenzio -61 dBFS, voce -24 dBFS, differenza **+37 dB** (il microfono sente).
- **Prova dei toni** (volume fisso, passo 5):

| Tono | Pausa | Tono | Differenza | Esito |
|---|---|---|---|---|
| 500 Hz | -63,3 dBFS | -44,3 dBFS | +19,0 dB | sentito |
| 1000 Hz | -68,8 dBFS | -25,1 dBFS | +43,7 dB | sentito |
| 2000 Hz | -76,4 dBFS | -38,2 dBFS | +38,2 dB | sentito |

- Osservazione: la risposta non è piatta: il tono a 1 kHz arriva 19 dB sopra quello a 500 Hz e 13 dB sopra quello a 2 kHz. È tipico di un altoparlante piccolo senza contenitore (poca resa sui bassi) e della risposta del microfono a breve distanza; per la voce (300-3400 Hz) conviene un contenitore chiuso (vedi [audio.md](audio.md)).
- Da fare: ripetere "Suona i toni" **con l'altoparlante scollegato** (controllo del falso positivo) e annotare se i toni si sentivano a orecchio.

### 08/11/2026, casa, IU5HJU (voce tra due T3: crepitii in ricezione, diagnosi)

- Schede: due LoRa32 T3 V1.6.1 con INMP441, MAX98357A e altoparlante. Firmware `main` (da `62cb3ed` a `9687965`).
- **Sintomo:** la trasmissione arriva intera (CRC ok, 0 frame persi nelle prove buone) ma dall'altoparlante del ricevitore escono **solo crepitii che seguono l'andamento della voce** e un rumore a fine trasmissione.
- **Escluso:** livello in ingresso (parlato a -20/-30 dBFS in uscita al codec, nessun frame perso dal microfono; i due soffi con offset -66% erano soffi, non RF); tempo di decodifica (13 ms in media, 16 ms al massimo, contro 40); radiofrequenza nel microfono (con la radio in trasmissione i toni restano uguali: +15/+21 dB solo sulla pausa, a -60/-80 dBFS); trasmissione in tempo reale (stesso risultato con "registra e poi trasmetti", [registra-e-trasmetti.md](registra-e-trasmetti.md)); **Codec2 sulla scheda e percorso di riproduzione** (CFG TEST CODEC: `raw=-27.2 dBFS`, `dec=-27.8 dBFS`, encms 1308 e decms 996 per 75 frame, **entrambi gli ascolti perfettamente intelligibili**, con il caratteristico suono del Codec2).
- **Ipotesi rimasta:** il ritmo della ricezione. Nel test locale i frame si scrivono uno dietro l'altro e il buffer I2S (80 ms) è sempre pieno; in ricezione il loop scrive un frame da 40 ms ogni 40 ms, quindi **senza alcuna riserva**: un qualsiasi ritardo del loop (decodifica, ridisegno del display da circa 25 ms, lettura della radio) svuota il buffer e l'uscita va a zero a metà parola, con uno scatto proporzionale al livello della voce. La misura `[rx-tempi]` contava solo i ritardi oltre 80 ms, troppo alta per vedere il problema.
- **Correzione tentata:** coda I2S da 200 ms e 120 ms di silenzio scritti all'inizio di ogni ascolto (riserva), soglia dei ritardi a 50 ms. **Non ha cambiato nulla** (la causa era un'altra, vedi sotto): riserva e coda sono state riportate ai valori di prima (coda da 80 ms, nessuna riserva) il 08/11/2026. La soglia dei ritardi in `[rx-tempi]` è rimasta a 50 ms.

- **Aggiornamento (08/11/2026, sera):** con `ff2ee2b` la prova locale (CFG TEST CODEC) suona **perfetta in tutti e quattro gli ascolti**, compresi quelli a ritmo di ricezione e con il display che si ridisegna. **Esclusi anche** i buffer vuoti (riserva di 120 ms: uguale a prima) e la radiofrequenza nel ricevitore (5 metri e potenza minima: uguale a prima). Resta da verificare se i **frame arrivano uguali a come sono stati trasmessi**: il firmware ora stampa `[tx-hex]` a fine trasmissione e `[rx-hex]` a fine ricezione, per il confronto byte per byte.

- **Causa trovata (08/11/2026, notte):** non era la radio, il Codec2, il ritmo di scrittura, il display né la RF. `[tx-hex]` e `[rx-hex]` risultano **identici byte per byte** (71 frame) e il Codec2 di riferimento decodifica quei frame come la scheda (rms -27,8 contro -28 dBFS). Il difetto era nello **spegnimento dell'amplificatore**: `AudioIo::loop(now)` riceve l'ora presa all'inizio del giro del loop, ma la riproduzione di un frame aggiorna `lastPlayMs_` con un'ora **più recente**; `now - lastPlayMs_` diventava negativo e, con numeri senza segno, enorme (maggiore di 400 ms), quindi il MAX98357A veniva **spento subito dopo ogni frame** e riacceso al successivo: voce a pezzi, scatti che seguono la voce e rumore a fine ricezione (e il bip di fine trasmissione quasi muto). Le prove locali (CFG TEST AUDIO e CFG TEST CODEC) non lo vedevano perché sono bloccanti e non richiamano `loop()` durante l'ascolto. Corretto con un confronto con segno (`amp_hold.h`, `tests/test_amp.cpp`); da verificare sulle schede.

- **Esito (08/11/2026):** con `d4ad6ff` (confronto con segno dell'ora per lo spegnimento dell'amplificatore) **la voce si sente benissimo** sul ricevitore. Prima prova riuscita di voce tra due T3.

### 08/11/2026, casa, IU5HJU (ricezione SDR: NESDR SMArt v5, prima prova reale)

- Ricevitore: portatile Ubuntu 20.04, chiavetta Nooelec NESDR SMArt v5 (R820T), antenna a circa 17 cm (quarto d'onda), GNU Radio 3.10.11 e `gr-lora_sdr` in un ambiente conda (`lora`, python 3.12). Scheda trasmittente con nominativo vero e antenna, a **circa 1 m** dalla chiavetta. Parametri: 433,900 MHz, SF8, BW 250 kHz, CR 4/5, sync word 0x4C. Guadagno: quello predefinito di `lora_rx.py`. Deriva in frequenza (`--ppm`): **non misurata**.
- **Prova di collegamento** (sessione 16333): 34 pacchetti con CRC ok, sequenza completa, `lcr_decode` riporta **ok 300/300 (100%)**, 0 danneggiati, 0 persi, 0 recuperati dalla parità.
- **Voce** (sessione 49160): 12 pacchetti con CRC ok, 89 frame (3,6 s), 0 persi. Decodificata con `c2dec 1200` e `ffmpeg` fino al WAV: **voce intelligibile, senza crepitii**. Questo conferma, con un ricevitore indipendente, che i frame trasmessi sono corretti e che i crepitii in ricezione sulla scheda non nascevano nella trasmissione.
- Problemi incontrati:
  - Il `rtl_test` e il ricevitore dell'ambiente conda non vedevano la chiavetta (`No supported devices found`, `Wrong rtlsdr device index`), mentre `/usr/bin/rtl_test` sì: la `libudev` di conda non enumera i dispositivi USB su Ubuntu 20.04. Soluzione: avviare il ricevitore con `LD_PRELOAD="/lib/x86_64-linux-gnu/libudev.so.1 /usr/lib/x86_64-linux-gnu/libusb-1.0.so.0 /usr/lib/x86_64-linux-gnu/librtlsdr.so.0"` (script `~/lora_rx.sh`).
  - `c2dec` 0.9.2 (Ubuntu 20.04) con un file che finisce in `.c2` cerca un'intestazione che `lcr_decode` non scrive e decodifica **solo una parte dei frame** (21120 byte invece di 56960). Soluzione: copiare il file con un'altra estensione (per esempio `.bin`) prima di `c2dec`.
  - `PLL not locked!` all'avvio dell'R820T compare sempre ed è innocuo.
- Da fare: misurare la deriva in frequenza (`--ppm`), provare con distanze maggiori e abbassare il guadagno a distanza ravvicinata.

### 09/11/2026, casa, IU5HJU (menu a bordo, modo Messaggio, posizione nel pacchetto START)

- Schede: due, collegate al PC una alla volta con PuTTY (115200). La scheda A è una LoRa32 T3 V1.6.1 con firmware `ea0f806` (dalla riga di avvio, protocollo 2); il modello e la versione della B non sono stati letti dalla seriale. Potenza della A 2 dBm, schede vicine.
- **Menu a bordo** (volume lungo 2 s a scheda ferma, PTT = voce successiva, volume breve = conferma): **funziona**. Il cambio di profilo con salvataggio e riavvio è avvenuto: dopo la prova la A risultava sul profilo Messaggio (`profile=msg`, SF12 BW125). I singoli passi non sono stati annotati uno per uno (apertura, salto di Qualità, chiusura dopo 10 s).
- **Modo Messaggio** (SF12/BW125): i messaggi trasmessi **arrivano corretti** alla scheda ricevente. Prova con un **messaggio lungo di 30 s**: la scheda trasmittente **non si è riavviata** (il dubbio sul watchdog con l'invio bloccante da circa 3,4 s a pacchetto è chiuso per questa durata) e il messaggio è **arrivato intero**. Non sono stati annotati tempi in aria misurati, RSSI, SNR né frame persi.
- **Posizione nel pacchetto START** (protocollo 2):
  - A: `CFG SET QTH 43.7696,11.2558`, `CFG SET POS ON`, riavvio. B: `CFG SET QTH 43.8000,11.2000`, profilo Standard, riavvio (coordinate di fantasia, non il QTH vero). Le risposte `CFG OK qth=…` e `CFG OK profile=std` sono arrivate.
  - Con il PTT premuto sulla A, sul display della B è comparso **"Distanza 5.6km"**, uguale al valore calcolato tra i due punti (5,61 km). Il blocco da 7 byte, la sua decodifica e il calcolo della distanza funzionano su due schede vere.
  - **Provato dopo, sempre il 09/11/2026:** con `CFG SET POS OFF` sulla A la distanza **non compare** sulla B (la posizione spenta di default funziona), e a fine ricezione la riga **"Ultimo NOMINATIVO 5.6km"** compare con la posizione accesa. Entrambi **funzionano**.
  - **Non ancora provato:** la distanza in modo Messaggio e il GPS della T-Beam.
- Problemi incontrati:
  - PuTTY dava "impossibile aprire la porta": la pagina web del progetto era aperta in **due schede di Chrome** e teneva occupata la porta seriale. Chiuse le schede, la porta si è aperta. **La seriale si apre da un solo programma alla volta.**
  - In PuTTY il controllo di flusso era su XON/XOFF: per la scheda va messo su **None**.
  - Due `CFG ERR comando sconosciuto` per un comando digitato male (`CFG REBOOT` con caratteri sbagliati): nessun effetto sulle impostazioni.
  - Il terminale non copia sempre la riga intera: la riga di `CFG GET` è stata troncata a `pos=on qth`.
- **Richiesta dell'operatore (da fare dopo le prove):** nel modo Messaggio il display deve mostrare una **percentuale stimata di avanzamento da 1% a 100%** sia in trasmissione sia in ricezione, perché un messaggio lungo dura minuti e l'utente medio penserebbe che la scheda sia bloccata. Lato TX la lunghezza è nota dopo la registrazione; lato RX serve che il pacchetto START dichiari quanti pacchetti seguono (modifica del protocollo v2).
