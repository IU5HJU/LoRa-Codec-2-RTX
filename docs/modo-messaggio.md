# Modo Messaggio (profilo "Messaggio")

**Stato (09/11/2026): primo passo scritto, da provare sulle schede.** Le prove sul PC passano (`make test`, tutti i test); la compilazione per la scheda e la prova radio sono ancora da fare. Decisioni e ragionamento: [roadmap.md](roadmap.md) ("Tre profili" e "Chi ripete cosa").

## Cos'è

Il profilo **Messaggio** serve per il **lungo raggio estremo**. Non è una conversazione: si **registra un messaggio**, la scheda lo **trasmette molto più lentamente del parlato** e chi riceve lo **memorizza e lo riproduce a ricezione conclusa**.

- Radio: **SF12, banda 125 kHz**, coding rate 4/5 (circa **+13 dB** di sensibilità rispetto allo Standard SF8/BW250).
- Codec2 **1200** (per ora: il 450 o il 700C, più compatti, arriveranno nel passo successivo).
- Durata massima di un messaggio: **30 secondi di voce** (deciso il 09/11/2026, "al momento": come lo Standard).

| Voce | Tempo in aria (stima, Codec2 1200) |
|---|---|
| 3 s | circa 30 s |
| 5 s | circa 48 s |
| 10 s | circa 94 s |
| **30 s (il massimo)** | **circa 271 s (4,5 minuti)** |

Il tetto di 30 secondi occupa il canale per quasi cinque minuti con il Codec2 1200 (circa 160 s con il 450, quando ci sarà): è un limite, non un obiettivo. Per le prove usa messaggi brevi. Il limite in tempo in aria, come il budget di 60 s della roadmap, resta da decidere.

Un pacchetto da 77 byte dura circa 3,4 secondi in aria: per questo il ricevitore aspetta la fine del messaggio (o 12 secondi di silenzio) prima di riprodurlo.

## Come si prova (una prova per volta)

1. Su **tutte e due** le schede, dalla pagina di configurazione: **Profilo di trasmissione = Messaggio**, nominativo vero, stessa frequenza, poi "Salva e riavvia". (Oppure da seriale: `CFG SET PROFILE MSG`, `CFG REBOOT`.) Controlla con `CFG GET`: deve contenere `profile=msg`.
2. All'accensione la seriale scrive `... pronto, profilo Messaggio, ... SF12 BW125` e il display mostra **MESSAGGIO**.
3. Sulla scheda che trasmette: **tieni premuto il PTT e parla** (fino a 30 secondi), poi **rilascia**. Display: `REGISTRA`, poi `ATTESA` (1 secondo), poi `INVIO`.
4. Sulla scheda che riceve: `Ricevo il messaggio` con RSSI e SNR. **Non si sente niente finché il messaggio non è arrivato tutto**; poi `Riproduco messaggio`.
5. Riporta: la durata del messaggio, quanto ha impiegato ad arrivare, le righe `[rx]`, `RX fine:` e `[rx-audio]` del ricevitore e come suona.

Per le prime prove usa messaggi **brevi** (3-5 secondi): bastano 30-50 secondi in aria.

**Frequenza:** il profilo Messaggio ha la **sua frequenza** (`CFG SET FREQMSG`, o il campo nella pagina), che può essere **uguale** a quella dello Standard: nelle prove si usa spesso una frequenza nota. Con la stessa frequenza i due profili non sono un canale condiviso (vedi sotto).

**Attenzione:** le schede con profili diversi **non si sentono**. Se una scheda Standard e una Messaggio sono accese insieme, non c'è collegamento. Il profilo vale dal riavvio.

## Cosa cambia per chi usa il profilo Messaggio

- Il modo "registra e trasmetti" è **sempre acceso** (`CFG REC OFF` non lo spegne).
- La **prova di collegamento** (tre clic sul PTT) non c'è: 12 secondi di frame a SF12 sarebbero circa 17 minuti in aria.
- Il limite di durata è quello del profilo (30 secondi di voce, come lo Standard), ma il tempo in aria è molto maggiore (vedi sopra).
- **Percentuale di avanzamento (10/11/2026):** un messaggio lungo resta in aria minuti e sembrerebbe bloccato, quindi il display mostra da **1% a 100%**, in invio ("Invio messaggio 45%", con una barra) e in ricezione ("Ricevo messaggio 45%"). Chi trasmette conosce già la lunghezza (il messaggio è registrato) e la dichiara nel pacchetto START (campo **TOTAL**, 1 byte, [protocollo.md](protocollo.md)); chi riceve calcola la percentuale dal pacchetto più avanzato ricevuto, che non torna indietro se un pacchetto si perde, e a 100% alla fine anche se l'ultimo pacchetto è andato perso. La voce Standard non ha la percentuale. **Da provare su scheda.**
- **Segnale di messaggio arrivato:** quando il messaggio è arrivato per intero e sta per essere riprodotto, la scheda suona **"MSG" in Morse** (M S G, 1000 Hz, circa 1,6 s), come lo squillo degli SMS dei vecchi telefoni; poi parte la riproduzione. Col volume a 0 non suona. **Da provare su scheda.**

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
