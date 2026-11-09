# Protocollo (v2)

Trasmissione vocale **broadcast** a turni (stile QSO) su LoRa, con voce Codec2 1200 bps.
Nessun indirizzamento, nessun ACK: chiunque sia in ascolto sulla frequenza decodifica.

## Sequenza di un turno

| # | Evento |
|---|---|
| 1 | Pressione del PTT. Una pressione più breve di 0,3 s è un "clic" e non trasmette (tre clic veloci attivano la prova di collegamento); se il PTT resta premuto oltre 0,3 s e il canale è occupato (CAD) o si sta ricevendo, il PTT è rifiutato. |
| 2 | Il microfono viene bufferizzato **subito** (dalla pressione, anche durante i 0,3 s di attesa). |
| 3 | Primo pacchetto: `START` + nominativo (nessun frame audio). |
| 4 | Codifica Codec2 e trasmissione man mano, con la bufferizzazione che continua. |
| 5 | Rilascio del PTT (o 30 s): stop del microfono, si svuota il buffer residuo e si invia `END` + nominativo (OVER) e l'ultima parità. |
| 6 | Si torna in ascolto. L'operatore attende 5-10 s e risponde: non esiste una fase di protocollo per il "bianco". |

Durata massima di un turno: **30 s** (750 frame). Al limite la trasmissione si chiude da sola.

## Parametri

| Parametro | Valore |
|---|---|
| Codec | Codec2 1200 bps: 6 byte ogni 40 ms (320 campioni a 8 kHz) |
| Pacchetto dati | 12 frame = 480 ms = 72 byte |
| Radio | SX1278 433 MHz, SF8, BW250, CR 4/5 (impostazioni in `src/user_config.h`) |
| Parità | 1 pacchetto XOR ogni 4 pacchetti dati (disattivabile) |
| Nominativo | in `START`, in `END` e ogni 10 pacchetti (~5 s) |

Sintesi in due pagine, da girare a chi lo chiede: [formato-pacchetto.pdf](formato-pacchetto.pdf), generata da `tools/make_formato_pacchetto_pdf.py` (`make pdf`): i tempi in aria si calcolano dai parametri radio e il programma si ferma se il protocollo o la radio cambiano rispetto al testo, così il PDF non invecchia in silenzio.

## Formato del pacchetto

```
byte 0     versione (4 bit alti, = 2) | modo (4 bit bassi: 1 = Codec2 1200 bps, 2 = prova di collegamento, 3 = risposta alla prova)
byte 1-2   ID di sessione (casuale per ogni PTT), big endian
byte 3     numero di sequenza (0..127)
byte 4     flag
[byte 5    lunghezza nominativo N (1..8)]      solo se flag CALLSIGN
[N byte    nominativo ASCII, A-Z 0-9 /]        solo se flag CALLSIGN
[7 byte    posizione]                          solo se flag POSITION (solo nel pacchetto START, con il nominativo)
resto      payload
```

Flag: `1 START`, `2 END`, `4 PARITY`, `8 CALLSIGN`, `16 NOFEC` (trasmissione senza parità), `32 POSITION`.
Bit 6-7 riservati: i pacchetti che li usano vengono scartati.

**Blocco posizione** (protocollo 2, `lib/voicecore/position.h`): 7 byte dopo il nominativo, solo nel pacchetto `START` (le trasmissioni sono brevi: una posizione ogni turno basta, anche per un mezzo in movimento).

```
byte 0     stato: bit 0 = 1 posizione da GPS, 0 posizione fissa (QTH) impostata a mano; gli altri bit riservati (zero, ignorati in ricezione)
byte 1-3   latitudine, 24 bit big endian:  (lat + 90) / 180 * 2^24     (passo circa 1,2 m)
byte 4-6   longitudine, 24 bit big endian: (lon + 180) / 360 * 2^24    (passo circa 2,4 m all'equatore)
```

Costo in aria (stima): circa 9 ms a SF8/BW250, circa 0,23 s a SF12/BW125, una volta per turno. **La posizione è disattivata di default** (`CFG SET POS ON` per trasmetterla, `CFG SET QTH lat,lon` per la posizione fissa): è un dato personale e viaggia in chiaro. Chi riceve la usa per mostrare la distanza in km se ha a sua volta una posizione.

Il payload dei pacchetti dati è una sequenza di frame Codec2 da 6 byte (0-12 frame).
Dimensione massima sul canale: 5 + 9 + 7 + 75 = 96 byte (il pacchetto `START` con la posizione ha solo header, nominativo e posizione: 5 + 9 + 7 = 21 byte).

Il nominativo viaggia in chiaro e il contenuto non è cifrato, come richiesto dalla normativa radioamatoriale.

## Pausa tra i pacchetti

Il trasmettitore lascia **80 ms** tra la fine di un pacchetto e l'inizio del successivo (`kInterPacketGapMs`). La radio SX127x ha un solo FIFO: se il pacchetto seguente
(tipicamente la parità, subito dopo il quarto dato) arriva prima che il ricevitore abbia letto il precedente, lo sovrascrive. Il ricevitore impiega fino a circa 27 ms a leggere
un pacchetto quando è occupato (misurato sulla T3); con l'altoparlante sarà di più.

## Prova di collegamento (modi 2 e 3)

Nel modo 2 i frame non sono voce ma una sequenza pseudocasuale determinata da (sessione, numero del frame): il ricevitore la rigenera e la confronta bit per bit, senza riprodurre audio.
Una prova dura 300 frame (12 s) con gli stessi pacchetti e la stessa parità della voce. Con la prova attiva anche sul ricevitore, questo risponde dopo circa 0,7 s con un solo pacchetto di modo 3
(`START`+`END`, nominativo, stessa sessione) che porta 12 byte: frame consegnati, ok, danneggiati, persi, pacchetti recuperati, RSSI e SNR medi. Un ricevitore con una versione precedente scarta i pacchetti di modo sconosciuto.
Dettagli in [prova-collegamento.md](prova-collegamento.md).

## Controllo hardware (modo 15)

Il modo 15 è usato solo da "Suona i toni" della pagina (`CFG TEST AUDIO`): la scheda trasmette pacchetti dati in continuo, con payload fisso e il nominativo, mentre suona i toni, per vedere se la radiofrequenza rientra nel microfono o nell'amplificatore. Non è audio e nessun ricevitore lo elabora: i modi sconosciuti vengono scartati.

## Parità XOR

Sequenza con parità attiva: `D D D D P D D D D P ...`, cioè i pacchetti con `seq % 5 == 4`
sono parità del gruppo precedente. L'ultimo gruppo può essere parziale: la parità segue
subito il pacchetto `END` e dichiara quanti pacchetti copre.

Payload di parità: `[count][xorFrames][xorFlags][XOR dei payload dati, azzerati a destra]`.
Con un solo pacchetto mancante nel gruppo, il ricevitore ricostruisce payload, lunghezza
e flag `START`/`END`. Con due o più mancanti il gruppo non è recuperabile: ogni pacchetto perso
diventa 12 frame "persi" (silenzio, in futuro mascherati da Codec2).

Costo: +25 % di pacchetti, circa il 30 % del tempo di voce di occupazione del canale a SF8/BW250.

## Ricezione

- Si adotta la prima trasmissione sentita. Pacchetti di un'altra sessione sono ignorati finché la
  prima è in corso; dopo 3 s di silenzio o dopo `END` una nuova sessione la sostituisce.
- Chi si sintonizza a trasmissione iniziata parte dal primo pacchetto ricevuto: il nominativo
  arriva al successivo pacchetto che lo contiene.
- La riproduzione parte dopo 1 s di audio contiguo (o `END`, o 2 s dal primo pacchetto).
  Un pacchetto mancante è dichiarato perso solo dopo l'arrivo della sua parità.
- La trasmissione si chiude con `END` oppure per inattività (3 s).

## Accesso al canale

Prima del PTT si esegue il CAD. Se rileva attività il PTT è rifiutato con segnalazione.
Le collisioni non sono rilevabili durante la trasmissione, come in una normale fonia.
