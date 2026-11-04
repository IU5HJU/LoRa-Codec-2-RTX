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
| Data e luogo | 2026-11-03, casa |
| Chi | IU5HJU |
| Versione del firmware | commit `0466536`, artefatto di Actions, ramo `prova-iu5hju` |
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
2. **Solo radio.** Premi il PTT su una scheda (anche senza microfono): l'altra passa a "RX" con il nominativo, l'**RSSI** (dBm) e l'**SNR** (dB) dell'ultimo pacchetto ricevuto.
3. **Microfono.** Parla con il PTT premuto: sull'altra scheda deve arrivare audio (anche con l'altoparlante non ancora collegato non si rompe nulla; si guardano le statistiche "persi" e "recup." a fine ricezione).
4. **Altoparlante.** Controlla volume, bip di fine trasmissione e di fine ricezione, assenza di fruscio a riposo.
5. **Voce a turni.** Turno di qualche secondo, attesa di 5-10 s, risposta. Qualità e comprensibilità.
6. **Portata.** Allontana le schede a passi, annotando distanza, RSSI, SNR e comprensibilità. Con SF8 il pacchetto si demodula fino a circa -10 dB di SNR (valore teorico): più ci si avvicina a quel limite, più i frame persi aumentano.

## Aiuto per la diagnosi

- Seriale della scheda: `pio device monitor` a 115200 baud, oppure un qualunque terminale seriale.
  A fine di ogni ricezione stampa una riga pronta per il registro, per esempio
  `RX fine: IU5HJU RSSI -92 dBm SNR 7.5 dB, frame persi 0, pacchetti recuperati 1`.
- **Riavvio continuo con `A stack overflow in task loopTask has been detected` sulla seriale:** lo stack del task principale è troppo piccolo. Corretto portandolo a 32 kB (3 novembre 2026); se ricompare, indica il punto in cui si ferma con le righe `[avvio]`.
- **"ERRORE" sull'OLED:** il nome indica il blocco (Codec2, audio I2S, radio); il codice è nella seriale.
- **Si sente solo silenzio:** prova `LCR_MIC_SLOT 1` in `src/user_config.h`.
- **Volume dimezzato o basso:** verifica il pin `SD` dell'amplificatore e l'alimentazione (5 V o 3,3 V).
- **"CANALE OCCUPATO" a canale libero:** il rilevamento del canale ha rilevato attività; annota quando e dove.
- **"STAI RICEVENDO":** la scheda sta ancora riproducendo una trasmissione; è normale per qualche secondo.

## Registro

### Modello da copiare

```
### AAAA-MM-GG, luogo, chi
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

### 2026-11-03, casa, IU5HJU (prova con una sola scheda)

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

### 2026-11-04, casa, IU5HJU (T-Beam V1.2 trasmette, LoRa32 T3 riceve)

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

### 2026-11-04, casa, IU5HJU (video della prova, T-Beam -> T3)

- Dal video: la T-Beam passa da "ASCOLTO" a "TX IU5HJU" con "Trasmissione 0/30 s" premendo IO38; la T3 mostra "RX IU5HJU" con **RSSI circa -28 dBm** e **SNR circa 9,3 dB** e, a fine ricezione, "Ultimo IU5HJU" con **"persi 0, recup. 3"**.
- Osservazione: a 1-2 m il segnale è fortissimo ma **tre pacchetti sono andati persi e sono stati recuperati dalla parità**. La parità funziona, ma le perdite a distanza così breve vanno spiegate. Ipotesi da verificare: i pacchetti di parità partono quasi attaccati al pacchetto dati precedente, e la T3 dopo ogni ricezione riarma la radio e può aver perso l'inizio del pacchetto successivo; oppure il ciclo principale della T3 era occupato (display, riproduzione) quando è arrivato il pacchetto.
- Problema aperto: la T-Beam V1.2 non si accende a batteria, solo da USB (cella a 1837 mV). Analisi in [alimentazione.md](alimentazione.md).

### 2026-11-04, casa, IU5HJU (batteria della T-Beam: lettura e carica)

- Firmware: ramo `prova-iu5hju` con la batteria sul display (`(Batt=x.xxV)` in basso a destra).
- **Lettura confermata dal tester:** la T-Beam mostra **2,90 V** e il tester sulla cella misura **2,90 V**. La misura dell'AXP2101 è accurata.
- **Andamento della carica da USB:** 1,84 V (avvio con il firmware di diagnostica) -> 2,70 V -> 2,90 V. La cella, scesa a 1,8 V, sta risalendo.
- Sotto 3,40 V il testo della batteria è in negativo, come previsto.
- Da fare: attendere la carica completa e provare l'avvio a batteria (tasto PWR); se non parte con una cella carica, leggere la soglia VOFF dalla seriale (`[pmu] soglia di spegnimento VOFF ...`).

### 2026-11-04, casa, IU5HJU (T-Beam V1.2: avvio a batteria)

- Cella carica (circa 4,2 V). **La T-Beam parte anche a batteria**, premendo PWR con l'USB scollegata: il mancato avvio era dovuto alla cella scarica (1,84 V), non al firmware.
- **La tensione mostrata sul display coincide con quella del tester**, anche a cella carica.
- Da fare: prova di durata della batteria in standby (radio in ascolto, OLED acceso), da interrompere quando il testo della batteria va in negativo (sotto 3,40 V) per non scaricare di nuovo la cella; provare una trasmissione col tasto IO38 a batteria; confrontare la lettura della LoRa32 T3 (ADC GPIO35) con il tester.

### 2026-11-04, casa, IU5HJU (verso opposto, T3 -> T-Beam)

- Schede e collegamenti: LoRa32 T3 V1.6.1 trasmette, T-Beam V1.2 riceve; microfono INMP441 arrivato e cablato sulla T3 (amplificatore e altoparlante non ancora arrivati).
- Livello raggiunto della scaletta: **2 (solo radio) completo nei due versi**. La T-Beam riceve correttamente dalla T3.
- Da annotare: RSSI, SNR, "persi" e "recup." di questa prova.
- Da fare: prova del microfono (livello 3); per sentire la voce serve ancora amplificatore e altoparlante.
