# Modo Messaggio (profilo "Messaggio")

**Stato (10/11/2026): scritto e provato sulle schede** (T3 e T-Beam: registrazione, invio, riproduzione a messaggio completo, percentuale, suono "MSG" in Morse, posizione e distanza; prove del 9-10 novembre in [prove.md](prove.md)). Le prove sul PC passano (`make test`). Manca la misura sul campo (distanze lunghe). Decisioni e ragionamento: [roadmap.md](roadmap.md) ("Tre profili" e "Chi ripete cosa").

## Cos'è

Il profilo **Messaggio** serve per il **lungo raggio estremo**. Non è una conversazione: si **registra un messaggio**, la scheda lo **trasmette molto più lentamente del parlato** e chi riceve lo **memorizza e lo riproduce a ricezione conclusa**.

- Radio: **SF12, banda 125 kHz**, coding rate 4/5 (circa **+13 dB** di sensibilità rispetto allo Standard SF8/BW250).
- Codec2 **1200**: **scelta definitiva** (10/11/2026). Il 450 e il 700C, più compatti (meno 43% e meno 28% di tempo in aria), non si faranno: il 1200 è un compromesso accettabile e il loro suono è molto peggiore ([codec2.md](codec2.md)).
- Durata massima di un messaggio: **15 secondi di voce** (deciso l'11/11/2026; il 09/11 era 30 s, come lo Standard): è quanto il ripetitore ripete, così ciò che si registra viene sempre ripetuto. Lo Standard e la Qualità restano a 30 s.

| Voce | Tempo in aria (stima, Codec2 1200) |
|---|---|
| 3 s | circa 30 s |
| 5 s | circa 48 s |
| 10 s | circa 94 s |
| 15 s (il massimo, dall'11/11) | circa 139 s (2 minuti e 20) |
| 30 s (il massimo fino al 10/11) | circa 271 s (4,5 minuti) |

Il tetto di 15 secondi occupa il canale per più di due minuti con il Codec2 1200 (con i 30 secondi di prima erano quasi cinque minuti) (il 450, che non faremo, ne richiederebbe circa 156): è un limite, non un obiettivo. Per le prove usa messaggi brevi. Il limite in tempo in aria, come il budget di 60 s della roadmap, resta da decidere.

Un pacchetto da 77 byte dura circa 3,4 secondi in aria: per questo il ricevitore aspetta la fine del messaggio (o 12 secondi di silenzio) prima di riprodurlo.

## Come si prova (una prova per volta)

1. Su **tutte e due** le schede, dalla pagina di configurazione: **Profilo di trasmissione = Messaggio**, nominativo vero, stessa frequenza, poi "Salva e riavvia". (Oppure da seriale: `CFG SET PROFILE MSG`, `CFG REBOOT`.) Controlla con `CFG GET`: deve contenere `profile=msg`.
2. All'accensione la seriale scrive `... pronto, profilo Messaggio, ... SF12 BW125` e il display mostra **MESSAGGIO**.
3. Sulla scheda che trasmette: **tieni premuto il PTT e parla** (fino a 15 secondi), poi **rilascia**. Display: `REGISTRA`, poi `ATTESA` (1 secondo), poi `INVIO`.
4. Sulla scheda che riceve: `Ricevo il messaggio` con RSSI e SNR. **Non si sente niente finché il messaggio non è arrivato tutto**; poi `Riproduco messaggio`.
5. Riporta: la durata del messaggio, quanto ha impiegato ad arrivare, le righe `[rx]`, `RX fine:` e `[rx-audio]` del ricevitore e come suona.

Per le prime prove usa messaggi **brevi** (3-5 secondi): bastano 30-50 secondi in aria.

**Frequenza:** il profilo Messaggio ha la **sua frequenza** (`CFG SET FREQMSG`, o il campo nella pagina), che può essere **uguale** a quella dello Standard: nelle prove si usa spesso una frequenza nota. Con la stessa frequenza i due profili non sono un canale condiviso (vedi sotto).

**Attenzione:** le schede con profili diversi **non si sentono**. Se una scheda Standard e una Messaggio sono accese insieme, non c'è collegamento. Il profilo vale dal riavvio.

## Casella dei messaggi (11/11/2026)

Il messaggio vocale ricevuto **non si riproduce più da solo**: finita la ricezione va in una **casella** (in RAM, **un solo messaggio alla volta**: uno nuovo cancella il vecchio) e la scheda aspetta il PTT.

- All'**inizio** della ricezione suona **"MSG" in Morse** (come prima). Alla **fine** il display mostra una **grande icona** (altoparlante per la voce, busta per il testo), la scritta **"MESSAGGIO RICEVUTO"** e **"Premi PTT: ascolta"**.
- Una **pressione breve del PTT** (meno di 300 ms) apre il messaggio: la voce parte (altoparlante e "Riproduco messaggio"). Una **pressione** durante l'ascolto lo ferma. Una pressione **lunga** del PTT trasmette sempre, come prima.
- **Ascoltato (o interrotto), il messaggio si cancella**: niente riascolto. Dopo un riavvio la casella è vuota.
- Se mentre la voce suona arriva un'altra ricezione, la riproduzione si interrompe e il messaggio resta in casella.
- Occupazione: 15 s a Codec2 1200 sono 375 frame da 6 byte (2250 byte) più la mappa dei frame persi (la casella è dimensionata per 30 s, 4500 byte).
- **Testo (modo 7, [protocollo.md](protocollo.md)):** stesso schema, ma all'arrivo suona **"SMS" in Morse** (stessa nota e stessa velocità di "MSG"), il display mostra la **busta** e **"Premi PTT: leggi"**. Il PTT breve apre il testo, che resta in mostra (6 righe da 21 caratteri; con più pagine il **volume breve** passa alla successiva); un'altra pressione del PTT lo chiude e lo cancella.
- **Invio:** menu a bordo (volume lungo) → **"Invia testo"** → PTT per scegliere uno dei testi non vuoti, volume breve per inviare, volume lungo per uscire (10 s senza tasti: esce). I cinque testi si impostano dalla pagina web (passo 2) o dalla pagina Wi-Fi della scheda, o con `CFG SET TEXT1 ...` (massimo 150 caratteri ASCII, senza le parentesi graffe, valgono subito, senza riavvio).
- **Da definire insieme:** i gesti con più messaggi in coda (oggi ce n'è uno solo), l'ingresso nel menu dei messaggi dal tasto volume, e in futuro una tastiera o un'app per telefono (Bluetooth) per scrivere testi liberi.
- **Stato:** scritto e provato sul PC (`make test`, prove della pagina); **da provare su scheda**.

## Cosa cambia per chi usa il profilo Messaggio

- Il modo "registra e trasmetti" è **sempre acceso** (`CFG REC OFF` non lo spegne).
- La **prova di collegamento** (tre clic sul PTT) non c'è: 12 secondi di frame a SF12 sarebbero circa 17 minuti in aria.
- Il limite di durata è quello del profilo (**15 secondi di voce**, dall'11/11/2026; lo Standard ha 30 s), ma il tempo in aria è molto maggiore (vedi sopra).
- **Percentuale di avanzamento (10/11/2026):** un messaggio lungo resta in aria minuti e sembrerebbe bloccato, quindi il display mostra da **1% a 100%**, in invio ("Invio messaggio 45%", con una barra) e in ricezione ("Ricevo messaggio 45%"). Chi trasmette conosce già la lunghezza (il messaggio è registrato) e la dichiara nel pacchetto START (campo **TOTAL**, 1 byte, [protocollo.md](protocollo.md)); chi riceve calcola la percentuale dal pacchetto più avanzato ricevuto, che non torna indietro se un pacchetto si perde, e a 100% alla fine anche se l'ultimo pacchetto è andato perso. La voce Standard non ha la percentuale. **Da provare su scheda.**
- **Riproduzione:** dall'11/11/2026 la voce non parte da sola: va in casella e si apre con il PTT (sezione sopra).
- **Segnale di messaggio in arrivo (10/11/2026):** appena comincia la ricezione di un messaggio la scheda suona **"MSG" in Morse** (M S G, 1000 Hz, circa 1,6 s), come lo squillo degli SMS dei vecchi telefoni, per **attirare l'attenzione** su una ricezione che dura minuti (prima del 10/11, nella versione `cf17027`, suonava alla fine, subito prima della voce, e non serviva). Lo schermo di ricezione compare prima del suono. Col volume a 0 non suona. **Da provare su scheda.**

## Cosa manca (prossimi passi, in ordine)

1. ~~Codec2 450 o 700C con impacchettamento dei bit~~: **scartato il 10/11/2026**, il Messaggio resta a 1200.
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

Un nodo che ripete messaggi in automatico è una **stazione ad azionamento automatico**: la normativa italiana (l'autorizzazione dei ripetitori viene dal **Ministero**, non dall'ARI) va consultata **prima** di qualsiasi prova con ripetizione automatica (identificazione, autorizzazioni, controllo).
