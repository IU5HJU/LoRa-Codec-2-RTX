# Protocollo (v2)

Trasmissione vocale **broadcast** a turni (stile QSO) su LoRa, con voce Codec2 a 1200 bps (profili Standard e Messaggio) o a 3200 bps (profilo Qualità, modo 4).
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
| Radio | SX1278 433 MHz, CR 4/5. Per profilo: **Standard** SF8/BW250, **Messaggio** SF12/BW125, **Qualità** SF7/BW250 (`lib/voicecore/profile.cpp`; i valori storici in `src/user_config.h`) |
| Parità | 1 pacchetto XOR ogni 4 pacchetti dati (disattivabile) |
| Nominativo | in `START`, in `END` e ogni 10 pacchetti (~5 s) |

Sintesi in tre pagine, da girare a chi lo chiede: [formato-pacchetto.pdf](formato-pacchetto.pdf), generata da `tools/make_formato_pacchetto_pdf.py` (`make pdf`): i tempi in aria si calcolano dai parametri radio e il programma si ferma se il protocollo o la radio cambiano rispetto al testo, così il PDF non invecchia in silenzio.

## Formato del pacchetto

```
byte 0     versione (4 bit alti, = 2) | modo (4 bit bassi: 1 = Codec2 1200 bps, 2 = prova di collegamento, 3 = risposta alla prova, 4 = Codec2 3200 bps)
byte 1-2   ID di sessione (casuale per ogni PTT), big endian
byte 3     numero di sequenza (0..127)
byte 4     flag
[byte 5    lunghezza nominativo N (1..8)]      solo se flag CALLSIGN
[N byte    nominativo ASCII, A-Z 0-9 /]        solo se flag CALLSIGN
[7 byte    posizione]                          solo se flag POSITION (solo nel pacchetto START, con il nominativo)
[1 byte    pacchetti totali]                   solo se flag TOTAL (solo nel pacchetto START dei messaggi)
resto      payload
```

Flag: `1 START`, `2 END`, `4 PARITY`, `8 CALLSIGN`, `16 NOFEC` (trasmissione senza parità), `32 POSITION`, `64 TOTAL`, `128 RELAYED`.
**`RELAYED` (bit 7, 10/11/2026):** il pacchetto è stato **ritrasmesso da un ripetitore** ([studio-ripetitore.md](studio-ripetitore.md), `lib/voicecore/repeater.*`). Un ripetitore non ripete mai un pacchetto con questo bit. Il resto del pacchetto è identico all'originale. Con il bit 7 tutti gli 8 bit del byte sono in uso; le schede con la beta.2 scartano i pacchetti con il bit 7 (non sentono le copie ripetute, ma nessun disturbo).

**Blocco posizione** (protocollo 2, `lib/voicecore/position.h`): 7 byte dopo il nominativo, solo nel pacchetto `START` (le trasmissioni sono brevi: una posizione ogni turno basta, anche per un mezzo in movimento).

```
byte 0     stato: bit 0 = 1 posizione da GPS, 0 posizione fissa (QTH) impostata a mano; gli altri bit riservati (zero, ignorati in ricezione)
byte 1-3   latitudine, 24 bit big endian:  (lat + 90) / 180 * 2^24     (passo circa 1,2 m)
byte 4-6   longitudine, 24 bit big endian: (lon + 180) / 360 * 2^24    (passo circa 2,4 m all'equatore)
```

Costo in aria (stima): circa 9 ms a SF8/BW250, circa 0,23 s a SF12/BW125, una volta per turno. **La posizione è disattivata di default** (`CFG SET POS ON` per trasmetterla, `CFG SET QTH lat,lon` per la posizione fissa): è un dato personale e viaggia in chiaro. Chi riceve la usa per mostrare la distanza in km se ha a sua volta una posizione.

Il payload dei pacchetti dati è una sequenza di frame Codec2 da 6 byte (0-12 frame).
Dimensione massima sul canale: 5 + 9 + 7 + 1 + 75 = 97 byte (il pacchetto `START` con la posizione ha solo header, nominativo e posizione: 5 + 9 + 7 = 21 byte).

Il nominativo viaggia in chiaro e il contenuto non è cifrato, come richiesto dalla normativa radioamatoriale.

**Pacchetti totali** (protocollo 2, flag `TOTAL`): 1 byte, solo nel pacchetto `START`, dopo la posizione. Dichiara quanti pacchetti (dati e parità, numeri di sequenza da 0 a totale - 1) trasmetterà il messaggio: chi riceve calcola la percentuale di avanzamento. Lo dichiara solo il modo Messaggio, dove il messaggio è già registrato quando parte l'invio; la voce in tempo reale (Standard) non conosce la lunghezza in anticipo e non lo manda. Valori da 1 a 255; il byte `0` non è valido. Costo: 1 byte.

Il totale è `START + pacchetti pieni (1 ogni 12 frame) + END` più un pacchetto di parità ogni 4 pacchetti dati (l'ultimo gruppo, anche parziale, ne ha uno).

## Pausa tra i pacchetti

Il trasmettitore lascia **80 ms** tra la fine di un pacchetto e l'inizio del successivo (`kInterPacketGapMs`). La radio SX127x ha un solo FIFO: se il pacchetto seguente
(tipicamente la parità, subito dopo il quarto dato) arriva prima che il ricevitore abbia letto il precedente, lo sovrascrive. Il ricevitore impiega fino a circa 27 ms a leggere
un pacchetto quando è occupato (misurato sulla T3); con l'altoparlante sarà di più.

## Prova di collegamento (modi 2 e 3)

Nel modo 2 i frame non sono voce ma una sequenza pseudocasuale determinata da (sessione, numero del frame): il ricevitore la rigenera e la confronta bit per bit, senza riprodurre audio.
Una prova dura 300 frame (12 s) con gli stessi pacchetti e la stessa parità della voce. Con la prova attiva anche sul ricevitore, questo risponde dopo circa 0,7 s con un solo pacchetto di modo 3
(`START`+`END`, nominativo, stessa sessione) che porta 12 byte: frame consegnati, ok, danneggiati, persi, pacchetti recuperati, RSSI e SNR medi. Un ricevitore con una versione precedente scarta i pacchetti di modo sconosciuto.
Dettagli in [prova-collegamento.md](prova-collegamento.md).

## Modo 4: Codec2 3200 bps (profilo Qualità)

Stessa struttura del modo 1, ma ogni frame di 40 ms occupa **16 byte** (due blocchi Codec2 da 20 ms e 8 byte, uno dopo l'altro: il flusso si decodifica con `c2dec 3200`). Un pacchetto dati porta 12 frame = 192 byte (totale 197 byte con intestazione; la parità XOR è lunga come il pacchetto più lungo del gruppo). Un payload che non sia multiplo di 16 byte è scartato. 30 s di voce occupano circa 12,5 s in aria su SF7/BW250 (42 %). Il numero di sequenza resta a 7 bit (massimo 128 pacchetti, cioè 30 s a 3200): per durate maggiori servirà la sequenza a 16 bit.

## Messaggio di testo (modo 7)

Un breve testo stile SMS (**1-150 caratteri**, ASCII stampabile `0x20-0x7E`, senza accenti e **senza le parentesi graffe** `{` `}`). È **un solo pacchetto** con `START`+`END`+`CALLSIGN`: intestazione del modo 7, nominativo di chi scrive e, come payload, il testo (al massimo 150 byte: il pacchetto più lungo è 5 + 1 + 8 + 150 byte). Niente parità, niente posizione, niente totale (flag `POSITION`/`TOTAL`/`PARITY` rifiutati), `seq` 0. Un pacchetto di testo senza nominativo, con un carattere non stampabile, vuoto o più lungo di 150 byte è scartato (anche in trasmissione: `encodePacket` restituisce 0).

**Perché 150 e senza graffe (11/11/2026):** il limite e i caratteri ammessi sono scelti in modo che lo stesso testo si possa inoltrare **senza modifiche** su altre reti di messaggi radioamatoriali a pacchetto (frame massimo di circa 255 byte, con un percorso che cresce a ogni salto; le graffe hanno lì un significato di servizio). Così non ci sono testi validi da noi e rifiutati altrove.

Il mittente lo trasmette **due volte** (stessa sessione, a 1 s di distanza: `kTextRepeatGapMs`), perché non c'è altra protezione; il ricevitore scarta la seconda copia (stessa sessione e stesso nominativo dell'ultimo testo ricevuto). Il testo si può ricevere con qualunque profilo; una scheda con una versione precedente scarta il modo 7 come ogni modo sconosciuto. Non c'è conferma di ricezione.

Nell'escursione **MeshCom** i testi non usano questo modo: viaggiano nel formato delle reti radioamatoriali a pacchetto ([meshcom-testo.md](meshcom-testo.md)), con altri parametri radio.

I testi da inviare sono cinque, predefiniti e impostabili dalla pagina web o dalla pagina Wi-Fi della scheda (`CFG SET TEXTn`, [firmware.md](firmware.md)).

## Comandi del sysop (modo 5)

Un solo pacchetto `START`+`END`+`CALLSIGN` (nominativo di chi manda), `seq` 0, senza parità, posizione o totale. Payload di **15 byte** (comando) o **18 byte** (risposta), tutti i numeri big-endian:

| | Byte |
|---|---|
| comando | `codice` (1), `parametro` (2), `contatore` (4), `firma` (8) |
| risposta | `0x80 \| codice` (1), `esito` (1), `contatore` (4, quello del comando), `dati` (4), `firma` (8) |

**Firma:** HMAC-SHA256 con una chiave condivisa di 16 byte, troncato a **8 byte**, calcolato sul pacchetto come esce da `encodePacket` **senza gli ultimi 8 byte** (intestazione, sessione e nominativo compresi: la sessione di un comando firmato non può essere 0). La firma **non cifra**: il comando resta leggibile.

**Controlli del ripetitore** (la scheda RX): nominativo uguale a quello del sysop configurato, firma valida, **contatore maggiore dell'ultimo accettato** (salvato in memoria: una registrazione riascoltata non funziona, nemmeno la seconda copia dello stesso comando). Altrimenti il comando si ignora in silenzio (nemmeno STATO senza firma). Il comando non si ripete mai.

| Codice | Comando | Parametro | Effetto |
|---|---|---|---|
| 1 | STATO | – | risposta con lo stato |
| 2 | SPEGNI | – | la TX non trasmette più (la RX ascolta sempre) |
| 3 | RIACCENDI | – | torna in servizio |
| 4 | SPEGNI A TEMPO | minuti 1..1440 | spenta per N minuti |
| 5 | RIAVVIA | – | riavvio delle due schede |
| 6 | IDENTIFICA | – | trasmette il nominativo (testo "Ripetitore VoLoRa") |
| 7 | POTENZA | dBm 2..17 | potenza di uscita (fino al riavvio) |

**Risposta:** firmata con la stessa chiave, trasmessa con i parametri della voce, **una volta** (se il sysop non la vede, ripete il comando: l'esito è lo stesso). Esito: 0 ok, 1 parametro fuori limiti, 2 comando sconosciuto, 3 non consentito. Dati: byte 3 = 1 se la TX è spenta, byte 2 = potenza in dBm, byte 1-0 = minuti che restano allo spegnimento a tempo (0xFFFF = senza scadenza). A TX spenta la risposta **non si trasmette**, salvo quella al comando che l'ha appena spenta.

**Sicurezza del ripetitore** (indipendente dai comandi): una trasmissione **continua** (pause fra pacchetti inferiori a 1,5 s) oltre **60 s di voce** o **180 s di messaggio** spegne la TX per 5 minuti; RIACCENDI la riaccende prima.

## Annuncio di ripetitore occupato (modo 6)

Un solo pacchetto `START`+`END`+`CALLSIGN` con il nominativo del **ripetitore** e, come payload, **2 byte big-endian** con i secondi di blocco previsti (**0 = di nuovo libero**). Niente parità, posizione o totale; `seq` 0; payload di lunghezza diversa da 2 e pacchetto senza nominativo sono scartati. Lo manda un ripetitore con i parametri della voce prima di ritrasmettere un messaggio lungo (le stazioni in Standard e Qualità non vedono con il CAD una trasmissione SF12), **due volte a 1 s di distanza** (stessa sessione, la seconda copia si scarta), e una volta con 0 secondi a servizio concluso se la ritrasmissione non è stata interrotta dalla voce.

Le stazioni **lo mostrano sul display** ("RIP. OCCUPATO m:ss" a riposo, con un bip) e **non rifiutano il PTT**: chi vuole parlare può farlo e il ripetitore interrompe il messaggio. L'avviso si cancella da solo allo scadere, con un annuncio a 0, quando si riceve voce o quando si trasmette. Una versione precedente scarta il modo 6 come ogni modo sconosciuto.

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
