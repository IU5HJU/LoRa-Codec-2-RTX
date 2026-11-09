# Modo Messaggio (profilo "Messaggio")

**Stato (09/11/2026): primo passo scritto, da provare sulle schede.** Le prove sul PC passano (`make test`, tutti i test); la compilazione per la scheda e la prova radio sono ancora da fare. Decisioni e ragionamento: [roadmap.md](roadmap.md) ("Tre profili" e "Chi ripete cosa").

## Cos'è

Il profilo **Messaggio** serve per il **lungo raggio estremo**. Non è una conversazione: si **registra un messaggio**, la scheda lo **trasmette molto più lentamente del parlato** e chi riceve lo **memorizza e lo riproduce a ricezione conclusa**.

- Radio: **SF12, banda 125 kHz**, coding rate 4/5 (circa **+13 dB** di sensibilità rispetto allo Standard SF8/BW250).
- Codec2 **1200** (per ora: il 450 o il 700C, più compatti, arriveranno nel passo successivo).
- Durata massima di un messaggio: **10 secondi di voce**.

| Voce | Tempo in aria (stima, Codec2 1200) |
|---|---|
| 3 s | circa 30 s |
| 5 s | circa 48 s |
| 10 s | circa 94 s |

Un pacchetto da 77 byte dura circa 3,4 secondi in aria: per questo il ricevitore aspetta la fine del messaggio (o 12 secondi di silenzio) prima di riprodurlo.

## Come si prova (una prova per volta)

1. Su **tutte e due** le schede, dalla pagina di configurazione: **Profilo di trasmissione = Messaggio**, nominativo vero, stessa frequenza, poi "Salva e riavvia". (Oppure da seriale: `CFG SET PROFILE MSG`, `CFG REBOOT`.) Controlla con `CFG GET`: deve contenere `profile=msg`.
2. All'accensione la seriale scrive `... pronto, profilo Messaggio, ... SF12 BW125` e il display mostra **MESSAGGIO**.
3. Sulla scheda che trasmette: **tieni premuto il PTT e parla** (fino a 10 secondi), poi **rilascia**. Display: `REGISTRA`, poi `ATTESA` (1 secondo), poi `INVIO`.
4. Sulla scheda che riceve: `Ricevo il messaggio` con RSSI e SNR. **Non si sente niente finché il messaggio non è arrivato tutto**; poi `Riproduco messaggio`.
5. Riporta: la durata del messaggio, quanto ha impiegato ad arrivare, le righe `[rx]`, `RX fine:` e `[rx-audio]` del ricevitore e come suona.

Per le prime prove usa messaggi **brevi** (3-5 secondi): bastano 30-50 secondi in aria.

**Attenzione:** le schede con profili diversi **non si sentono**. Se una scheda Standard e una Messaggio sono accese insieme, non c'è collegamento. Il profilo vale dal riavvio.

## Cosa cambia per chi usa il profilo Messaggio

- Il modo "registra e trasmetti" è **sempre acceso** (`CFG REC OFF` non lo spegne).
- La **prova di collegamento** (tre clic sul PTT) non c'è: 12 secondi di frame a SF12 sarebbero circa 17 minuti in aria.
- Il limite di durata è quello del profilo (10 secondi), non i 30 secondi dello Standard.

## Cosa manca (prossimi passi, in ordine)

1. **Codec2 450 o 700C** con impacchettamento dei bit (circa il 40 % di tempo in aria in meno): da scegliere ad orecchio.
2. **Intestazione del messaggio** (nel pacchetto START): identificatore del messaggio, flag **emergenza** e **ripetibile**, e la **posizione**.
3. **Ripetizione automatica a un salto** da un altro dispositivo in ascolto sulla stessa frequenza: identificatore per non ripetere due volte, contatore di salti, nominativo di chi ripete, ritardo casuale con soppressione.
4. **GPS** sulla T-Beam (NMEA dal NEO-M8N) e **posizione fissa** da `CFG` per le schede senza GPS.

## Posizione (GPS) nei modi Messaggio e Standard: progetto

- **Messaggio:** il pacchetto START porta la posizione di chi ha registrato (con il tempo): un vero messaggio di emergenza con il punto sulla mappa, che si potrà poi visualizzare. Formato previsto: latitudine e longitudine a **3 byte ciascuna** (risoluzione circa 2,4 m), più qualità del fix e ora UTC.
- **Standard:** la posizione si invia a intervalli e **chi riceve vede sul display la distanza in linea retta in km** (formula di Haversine) dal corrispondente, mentre riceve.
- **Schede senza GPS:** una posizione fissa impostata da `CFG` (ancora da definire il comando).
- **Privacy:** la posizione viaggia **in chiaro**, come il nominativo. Va previsto un interruttore per non trasmetterla.
- **Avvertenza:** il sistema non garantisce la consegna (nessun ACK) e **non sostituisce i servizi di emergenza**: un messaggio con un punto sulla mappa aiuta, ma non si sa se qualcuno lo riceve.

## Ripetizione e normativa

Un nodo che ripete messaggi in automatico è una **stazione ad azionamento automatico**: la normativa italiana e l'ARI vanno consultate **prima** di qualsiasi prova con ripetizione automatica (identificazione, autorizzazioni, controllo).
