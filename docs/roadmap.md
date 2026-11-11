# Roadmap e decisioni

Aggiornato il 10 novembre 2026 (sezione "Stato al 10/11/2026" qui sotto; le decisioni dell'8 novembre sono più sotto).

## Stato al 10/11/2026

**Fatto e provato su scheda** (esiti in [prove.md](prove.md)):

| Cosa | Dove |
|---|---|
| Voce a turni in Standard (SF8/BW250, Codec2 1200); causa dei crepitii trovata e corretta | 8 novembre |
| Prima beta `v0.1.0-beta.1`, pagina di installazione con menu delle versioni, copia di sicurezza di `esp32_codec2` in `third_party/` | 8-9 novembre |
| Profilo **Messaggio** (SF12/BW125): registra e invia, riproduce a messaggio completo; provato fino a 30 s, senza riavvii | 9 novembre |
| Frequenza propria del profilo Messaggio (`FREQMSG`), anche uguale a quella dello Standard | 9 novembre |
| **Menu a bordo** (volume lungo, PTT, conferma, riavvio sul profilo scelto) | 9 novembre |
| **Protocollo v2:** posizione nello START (spenta di default), `QTH` fisso, distanza in km sul display in Standard e in Messaggio | 9 novembre |
| **Percentuale 1-100%** del messaggio (campo `TOTAL` nello START) e segnale **"MSG" in Morse** all'inizio della ricezione | 10 novembre |
| **GPS del T-Beam** (`CFG SET GPS ON`): fix con 4 satelliti, posizione dal GPS nel pacchetto START, vista dall'SDR come "(GPS)" | 10 novembre |
| **SDR:** ricezione, decodifica e **ascolto in tempo reale** (`lcr_live`) con icone su Ubuntu MATE | 10 novembre |
| Profilo **Qualità** (Codec2 3200, SF7/BW250, `FREQQUAL`, modo 4 del protocollo): suona meglio del 1200 | 10 novembre |
| **Configurazione dal Wi-Fi della scheda** (voce "Wi-Fi" del menu, modo dedicato all'avvio come nel Tracker di Guzman, password salvata, server asincrono) | 10 novembre |
| **Casella dei messaggi** (11/11/2026): la voce non parte più da sola, serve il PTT breve; **messaggi di testo** (modo 7, max 150 caratteri, "SMS" in Morse, busta sul display), cinque **testi predefiniti** impostabili da pagina web e Wi-Fi, menu "Invia testo". Scritto e provato sul PC; **da provare su scheda**. Restano da decidere i gesti con più messaggi, il menu dei messaggi, tastiera o app Bluetooth, e l'estensione del ripetitore al modo 7 (`Repeater::setModeMask`) | 11 novembre |
| **MeshCom: escursione a tempo** (11/11/2026): dal menu la scheda passa per qualche minuto alle reti di testo radioamatoriali (433,175 MHz), invia un testo con segnaposti a un gruppo, un nominativo o a tutti, ascolta le risposte e torna da sola alla voce; formato dei frame verificato su frame veri, conferme di ricezione; **provato in aria con una T3 e un nodo MeshCom** (invio al gruppo, risposte dirette e di gruppo, rientro alla voce; conferma di ricezione vista dall'app) ([meshcom-testo.md](meshcom-testo.md)) | 11 novembre |
| **Lettera del profilo** (S, M, Q) sul display, **volume con guadagno** (passo 5 = 0 dB, fino a +15 dB), pagina web con messaggio di stato fisso | 10 novembre |
| Prove su scheda per la **beta.2** (versione, display, Wi-Fi, trasmissione nei tre profili): superate; tag **`v0.1.0-beta.2`** pubblicato | 10 novembre |

**Da fare**, in ordine di priorità proposto:

| # | Voce | Note |
|---|---|---|
| 1 | ~~Pagina web: campi `POS`, `QTH` e `GPS`~~ | **fatto il 10/11/2026** (da provare con una scheda vera) |
| 2 | ~~Menu, **passo 3**: voce "Wi-Fi" e pagina interna alla scheda~~ | **fatto e provato il 10/11/2026** (rete `LoRaCodec2-XXXX`, password casuale salvata, pagina su 192.168.4.1, TX e RX spente) |
| 3 | **Verifiche sul Codec2** 3200 (e 450, 700C: scartati per il Messaggio) | **fatta la parte sul PC il 10/11/2026** ([codec2.md](codec2.md): modi disponibili, CPU e RAM stimate, tempo in aria, strumento `codec_compare`); **fatta anche la misura sulla scheda il 10/11/2026**: 3200 in codifica 10,0 ms e in decodifica 13,3 ms per frame (25% e 33% di 40 ms), il 1200 17,4 e 13,3 ms; ascolto del 3200 fatto (suona meglio), il 2400 non serve più |
| 4 | **Modo Qualità** (Codec2 3200, SF7/BW250) | **fatto e provato sulle schede il 10/11/2026** (suona meglio del 1200). L'**ascolto in Qualità con l'SDR** (`lcr_live` ricompilato) è **riuscito il 10/11/2026**. La **CPU è misurata** (vedi riga 3). Resta la sequenza a 16 bit per durate oltre i 30 s |
| 5 | **Misure sul campo:** RSSI, SNR, deriva di frequenza, perdite, anche in movimento | decidono quanta protezione serve |
| 6 | Intestazione del messaggio (identificatore, flag emergenza e ripetibile) | il Messaggio **resta a 1200** (decisione del 10/11/2026, vedi sotto) |
| 7 | Ripetizione a un salto del messaggio; **ripetitore Standard a due schede** (opzione A scelta il 10/11/2026: flag `RELAYED` e logica scritte e provate sul PC; firmware RX/TX scritto e da provare sul banco; mancano i comandi del sysop e l'identificazione; idea dell'11/11: la TX ripete anche i messaggi (15 s, priorità alla voce), archivio dei messaggi scritto e provato sul PC, vedi lo studio) | **sperimentale** a cura dell'operatore (primo ripetitore LoRa in Italia, [studio-ripetitore.md](studio-ripetitore.md)): sono stazioni automatiche |
| 8 | **Memorie** (profilo e frequenze per ogni canale) e frequenze di lavoro col band plan IARU/ARI | |
| 9 | Locatore Maidenhead e ora precisa dal GPS | |
| 10 | **Documentazione e testo per un video esplicativo** del progetto | vedi sotto: da preparare **prima** delle riprese, senza scrivere codice |

**Video esplicativo (da preparare, 10/11/2026):** è in programma un video che racconta il progetto. Serve una documentazione pronta e un **testo da leggere e spiegare passo passo, in forma ridotta (pochi minuti)**. Da fare, senza scrivere codice:
- **Scaletta** in pochi blocchi: l'idea (voce su LoRa a 433 MHz tra radioamatori), la scheda e cosa serve (T3 o T-Beam, microfono, amplificatore, altoparlante), l'installazione dalla pagina web, i tre profili (Standard, Messaggio, Qualità) con la differenza di portata e suono, il menu a bordo e la configurazione, una prova dal vivo, i limiti e cosa manca.
- **Testo parlato** breve, con un linguaggio semplice e le frasi lette ad alta voce senza sigle non spiegate; un paragrafo per blocco, con i tempi indicativi.
- **Materiale di supporto:** schema dei collegamenti, tabella dei profili (portata, tempo in aria, qualità), schermate del display e della pagina, i messaggi da non dimenticare (nominativo, antenna collegata, frequenze, niente cifratura).
- **Prima delle riprese:** le prove su scheda ancora aperte (Wi-Fi, profili, Qualità) e la beta pubblicata, così il video mostra una versione che esiste.

**Piccole cose in sospeso:** **note di rilascio della beta.2 da incollare a mano** nella release (testo in [note-rilascio/v0.1.0-beta.2.md](note-rilascio/v0.1.0-beta.2.md); la release ha ora solo il testo generato) e correggere nelle note della beta.1 "protocollo versione 1" (era già la 2); verifica file per file della licenza di `third_party/` prima di una versione stabile.

**Decisioni di rinuncia:** il **Messaggio resta con il Codec2 1200** (10/11/2026): il 450 farebbe risparmiare circa il 43% del tempo in aria (155,6 s invece di 271 s per 30 s di voce) e il 700C il 28%, ma il 1200 è già "un compromesso accettabile" e il suono dei modi più compatti è molto peggiore; **cade quindi anche l'impacchettamento a bit** (28 e 18 bit per frame), che serviva solo a loro: il 3200 e il 2400 hanno frame di 8 e 6 byte interi. nessun ACK sui messaggi (valutato il 10 novembre: broadcast con risposte che si scontrano, finestre di attesa, trasmissione automatica, nessuna autenticazione); Opus escluso per la modalità Qualità (resta il Codec2 3200).

## 1. Prima trasmissione (fatta; resta la voce)

Procedura in [firmware.md](firmware.md). Stato: **collegamento radio provato nei due versi** tra T-Beam e T3 (5 novembre), microfono e altoparlante provati, **la voce passa da una scheda all'altra e si sente bene** (8 novembre, dopo la correzione dell'amplificatore, vedi [prove.md](prove.md)).
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
- ~~frame di dimensione variabile in bit (700C: 28 bit, 450: 18 bit) con impacchettamento a bit~~: **non serve più** (10 novembre: il Messaggio resta a 1200; il 3200 e il 2400 hanno frame di 8 e 6 byte interi);
- modi a 20 ms (2400, 3200) come due blocchi per frame audio;
- tabella dei profili nel core, guidata da dati, con test sul PC che verifica il limite del 70 % per ogni profilo;
- facoltativo: bip a 10 s dalla fine del TOT;
- il formato dei pacchetti cambia: le due schede devono avere la stessa versione.

### Decisioni dell'8 novembre 2026: profili e modi (ragionamento; Standard, Messaggio e menu a bordo fatti, Qualità e ripetitore ancora da fare)

Definizioni (decise con IU5HJU):

- **Più efficiente** = più portata e più correzione degli errori, **a scapito della qualità audio**.
- **Alta qualità** = brevi distanze e la **massima fedeltà audio possibile**.

Principi, che completano la tabella del 5 novembre:

1. **Due assi indipendenti.** Il **modo del codec** (Codec2 450, 700C, 1200, 2400, 3200...) sta nel pacchetto e il ricevitore lo segue. Il **profilo radio** (SF, banda, coding rate, preambolo) no: il ricevitore LoRa ascolta **un solo** SF e una sola banda e non li riconosce da solo, quindi ogni profilo ha un **canale o una frequenza concordata**. Una scansione automatica dei profili richiede preamboli più lunghi (più tempo in aria): per ora no.
2. **Fino a SF10/BW250 la voce regge in tempo reale; oltre no.** Con SF10/BW125 o SF11-SF12 trasmettere dura più del parlato (tabella sotto). **Il lungo raggio estremo è quindi un messaggio vocale e non una conversazione** (deciso l'8 novembre): si registra il messaggio (la parte di trasmissione c'è già: `CFG REC ON`, vedi [registra-e-trasmetti.md](registra-e-trasmetti.md)), la scheda lo trasmette più lentamente del parlato, e chi riceve lo **memorizza e lo riproduce a ricezione conclusa**. Con il tempo a disposizione si può fare **interleaving** profondo o ripetere i frame in momenti diversi, senza costo di tempo in aria (utile contro ombre da ostacoli e fading).
3. **TOT in tempo in aria**, come già deciso (budget di 60 s in aria per turno). Per il messaggio il limite è sul tempo in aria: a SF12/BW125 sono circa 12 s di voce per ogni minuto in aria.
4. **Qualità massima con il Codec2 = 3200 bps** su **SF7/BW250** (deciso l'8 novembre: la stessa banda di oggi, -2,5 dB di sensibilità, 41 % in aria). La BW500 resta un'opzione per dopo (più tempo libero, ma -5,5 dB e banda larga da verificare con il piano di banda). Oltre il Codec2 (banda stretta, circa 4 kHz) servirebbe **Opus a banda larga** (16-24 kbit/s) su SF6/BW500: da valutare dopo le misure (CPU dell'ESP32). La fedeltà dipende anche dall'hardware: l'altoparlante da 3 W senza contenitore ha una risposta molto irregolare ([prove.md](prove.md)).
5. **Confermato il 09/11/2026: la modalità Qualità usa Codec2 3200, non Opus.** Motivi (stime, non ancora misure su scheda):
   - sul canale della Qualità (SF7/BW250, 41 % in aria) non c'è il bitrate che serve a Opus: sotto i 12-16 kbit/s Opus perde il suo vantaggio e a 6 kbit/s suona peggio del Codec2 3200, mentre sopra i 6 kbit/s non ci sta;
   - il Codec2 gira già sulle schede, ha frame di lunghezza fissa e ognuno si decodifica da solo, quindi la parità XOR (uno su quattro) e la copertura dei buchi restano com'è; Opus ha frame di dimensione variabile e un pacchetto perso rovina la predizione dei successivi, e richiederebbe un altro formato di pacchetto;
   - il 3200 è anche il modo usato da M17, comodo da spiegare e forse un giorno compatibile;
   - **ripiego: Codec2 2400**, da scegliere ad orecchio se la CPU o il tempo in aria non bastano.
   - **Opus si riconsidera solo con molta più banda** (per esempio SF7/BW500), e solo dopo aver provato il 3200 sul campo e misurato dove cade la qualità.
6. **Obiettivo del progetto (09/11/2026): puntare sul Codec2 3200, come fa M17.** Il Codec2 a 1200 bit/s è il punto di partenza: è quello che oggi passa da una scheda all'altra, con il suo timbro caratteristico, ed è abbastanza leggero per il canale LoRa più economico (SF8/BW250). Ma il progetto non vuole fermarsi lì. La direzione è la **voce più chiara che il canale permette**, e per il Codec2 quel punto è il **3200 bit/s**, lo stesso modo che usa M17: una comunità attiva, strumenti già pronti per confrontare le registrazioni e un suono che si avvicina di più a una voce vera. Il 1200 resta per le distanze maggiori e per chi vuole occupare poco il canale; il 3200 è il traguardo della modalità Qualità, e da lì si decide se convenga portarlo anche altrove. In breve: **1200 per arrivare lontano, 3200 per farsi capire bene**, con il 3200 come modo di riferimento a cui il protocollo, i test e le prove sul campo guardano per primi.
7. **Stato dei modi Codec2 e prudenza sul 1200 (ricerca del 09/11/2026).** Il [README di Codec2](https://github.com/drowe67/codec2) dichiara una "major re-development" e mette tra i modi "non più sviluppati attivamente e probabilmente da sostituire in futuro" anche **"Codec 2 modes with the exception of Codec 2 3200 for M17"**: i modi del codec, 1200 compreso, sono quindi congelati e **il 3200 è l'unica eccezione**. Lo stesso README dice che il codice di questi modi "won't be going away any time soon" e che continueranno a includere "any modes/code in popular use". Non esiste un annuncio ufficiale di rimozione del 1200, né una data. Precedenti: i modi 700 e 700B sono stati tolti da libcodec2 (fonte secondaria: un pull request di Meshtastic) e l'applicazione FreeDV ha eliminato 700C, 800XA, 2020 e 2020B (2.0.0) e nascosto 700D, 700E e 1600 (2.2.0): sono modi FreeDV per HF, non i modi del codec. Il [codec2.h](https://raw.githubusercontent.com/drowe67/codec2/main/src/codec2.h) attuale contiene ancora `CODEC2_MODE_1200`.
   - **Per noi il rischio è basso.** Il campo "modo" del nostro pacchetto è indipendente dalla numerazione interna di Codec2, e `platformio.ini` fissa `sh123/esp32_codec2 @ 1.0.7` (3200, 2400, 1600, 1400, 1300, 1200, 700C, 450, 450PWB tutti attivi): nessun aggiornamento esterno può togliere il 1200.
   - **Copia di sicurezza fatta il 09/11/2026:** `third_party/esp32_codec2-1.0.7/` è la copia esatta del tag 1.0.7 (164 file, 4,2 MB, con `SHA256SUMS`), non usata dalla compilazione ([third_party/README.md](../third_party/README.md)). La licenza è stata letta dalle intestazioni: LGPL 2.1 per la maggior parte dei file (di solito senza "o successiva"), GPL per pochi, nessuna intestazione per molte tabelle; la LGPL 2.1 permette di applicare la GPL a una copia, ma **la verifica file per file prima di una versione stabile resta da fare** (non è un parere legale).
   - **Non trovato:** nessuna dichiarazione ufficiale sul futuro del 1200, nessun dato su quanto sia usato. Se serve una risposta certa si può chiedere nel gruppo [digitalvoice](https://groups.google.com/g/digitalvoice) o nelle issue di drowe67/codec2.

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
| **Messaggio** (lungo raggio estremo) | **1200** (deciso il 10 novembre; il 450 e il 700C non servono) | **SF12/BW125** (+13 dB) | registra e invia, ricezione che memorizza e riproduce | **30 s di voce, al momento** (deciso il 9 novembre; a 1200 bps sono circa 271 s in aria): il limite in tempo in aria resta da decidere |
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

#### Chi ripete cosa (deciso l'8 novembre, sera)

| Profilo | Ripetizione | Come |
|---|---|---|
| **Messaggio** | **sì, l'unico dove il messaggio si ripete** | store-and-forward: il nodo memorizza il messaggio completo e lo ritrasmette (stessi bit del Codec2, nessun degrado) |
| **Standard** | da un **futuro ripetitore standard** | **due frequenze**: una in ricezione (ingresso) e una in trasmissione (uscita) |
| **Qualità** | **no: solo collegamento diretto (simplex)** | nessun ripetitore |

Conseguenze da tenere presenti (ragionamento, non ancora progettato):

- **Un ripetitore Standard con due frequenze ascolta e trasmette nello stesso momento**: servono **due radio** (una in RX, una in TX) e un buon isolamento tra le antenne (distanza, filtri). Una sola SX127x non basta in tempo reale. Con una sola radio si potrebbe solo memorizzare il turno e ritrasmetterlo dopo, cioè fare un store-and-forward, che non è un ripetitore.
- **Il ripetitore può rigenerare la protezione:** recupera i pacchetti persi con la parità e trasmette con una parità nuova, quindi ogni tratta ha la sua correzione degli errori.
- **Le schede Standard hanno bisogno di frequenza di ricezione e di trasmissione separate** (scostamento, "shift") come in un ripetitore analogico, oltre al collegamento diretto sulla frequenza simplex: da prevedere nelle impostazioni e nella pagina.
- **Un ripetitore (e un nodo che ripete messaggi) è una stazione ad azionamento automatico**: da autorizzare secondo la normativa italiana (**Ministero**, non ARI) prima di qualsiasi prova con ripetizione automatica; il ripetitore deve anche identificarsi.
- **Frequenze da concordare, almeno cinque:** Messaggio, Standard diretto, ingresso e uscita del ripetitore Standard, Qualità (voce "Frequenza di lavoro" in "Aperto").
- **Messaggio ripetuto:** serve un identificatore univoco del messaggio (nominativo di origine e sessione) per non ripeterlo due volte, un contatore di salti con limite basso, il nominativo di chi ripete e un flag "ripetibile"; ritardi casuali con soppressione se un altro lo ha già ripetuto. Prima un solo salto; la mesh con più salti si valuta dopo le misure sul campo.

#### Ripetitore Standard: due schede collegate (deciso l'8 novembre, sera)

Il ripetitore Standard è fatto da **due schede collegate tra loro**: una **solo in ricezione** sulla frequenza di ingresso e una **solo in trasmissione** sulla frequenza di uscita. Nessun microfono, altoparlante né Codec2: le schede ritrasmettono i pacchetti così come arrivano. Ragionamento, da progettare:

- **Collegamento:** una seriale a 3 fili (TX, RX, massa) tra le due schede basta: i pacchetti sono di circa 90 byte, uno ogni 130-200 ms. La scheda in ricezione passa alla scheda in trasmissione solo i pacchetti con CRC valido, con RSSI e SNR.
- **Ritardo:** ritrasmettendo pacchetto per pacchetto il ritardo è circa un pacchetto (circa 130 ms in aria più pause). Rigenerare la parità e recuperare un pacchetto perso richiede di attendere la parità del gruppo (circa 2 s): la prima versione ritrasmette quello che arriva, la rigenerazione viene dopo.
- **Antiripetizione:** un flag "ripetuto" evita che un pacchetto già ripetuto venga ripetuto ancora; ricezione e trasmissione su frequenze diverse evitano gli anelli.
- **Identificazione:** il ripetitore deve aggiungere il proprio nominativo ai suoi pacchetti e ogni tanto trasmettere un'identificazione (regole da verificare, vedi sopra).
- **TOT e coda:** il ripetitore applica il TOT del profilo (180 s per lo Standard) e interrompe i turni più lunghi.
- **Isolamento:** le due antenne vanno separate (distanza, polarizzazione verticale una sopra l'altra) e va misurato quanto la trasmissione disturba la ricezione (RSSI sulla scheda in ricezione mentre l'altra trasmette) prima di montarlo.
- **Alimentazione:** trasmette di continuo durante un turno (circa metà del tempo): alimentazione fissa e stabile, con la T-Beam e la batteria come riserva.
- **Schede utente:** frequenza di ricezione e di trasmissione separate (scostamento) e controllo "canale occupato" sulla frequenza di uscita: la scheda ascolta sull'uscita, e se sta ricevendo il PTT è rifiutato come oggi.
- **Software:** un ruolo "ripetitore RX" e un ruolo "ripetitore TX" nello stesso firmware, scelti nelle impostazioni, senza audio.

#### Cambio di modalità a bordo e pagina interna (bozza del 9 novembre, da implementare)

Si usano i due tasti che ci sono già. **Deciso:** pressione lunga sul volume apre il menu, PTT sceglie, volume conferma, uscita automatica dopo 10 s senza cambiare.

| Tasto | Fuori dal menu | Dentro il menu |
|---|---|---|
| **Volume**, breve | alza il volume (come oggi) | **conferma** la voce scelta |
| **Volume**, lungo (circa 2 s) | **apre il menu** (bip) | **esce senza cambiare** |
| **PTT**, breve | trasmette (come oggi) | **voce successiva** |

- Il menu si apre **solo a scheda ferma** (né in ricezione né in trasmissione). **Nel menu il PTT non trasmette mai.** Dopo 10 s senza tasti il menu si chiude **senza cambiare**.
- **Canali memorizzati:** non solo "tre modi", ma memorie, ognuna con **profilo e frequenze proprie** (per esempio: Standard diretto, Standard via ripetitore con ingresso e uscita separati, Messaggio, Qualità). Si configurano dalla pagina; il menu a bordo sceglie la memoria e il display ne mostra il nome. Servono quindi **frequenze per profilo** nelle impostazioni (oggi ce n'è una sola).
- **Cambio di memoria:** nel primo passo si **salva la scelta e si riavvia** la scheda (due secondi, semplice e sicuro). Il cambio a caldo della radio viene dopo. La voce "Qualità" è attiva dal 10/11/2026.
- **Pagina di configurazione interna** (stile LoRa APRS di Guzman, CA2RXU: all'avvio la scheda crea un proprio Wi-Fi e serve una pagina su `192.168.4.1`): una **voce "Configurazione Wi-Fi" nello stesso menu**, quindi un solo gesto. Con la pagina attiva **la trasmissione è disattivata**.
  - La rete (`LoRaCodec2-XXXX`) ha una **password casuale mostrata sul display**: le password predefinite uguali per tutti (come in alcuni progetti analoghi) sono note a chiunque.
  - **Stessa pagina di oggi**, che passa i comandi `CFG` alla scheda al posto della porta seriale: l'interprete dei comandi esiste già e ha i test, quindi non c'è un secondo programma da mantenere.
  - Dopo il salvataggio la pagina **rilegge i valori dalla scheda** e li confronta (lo fa già oggi): in alcuni progetti analoghi capitavano impostazioni che non si salvavano senza che nessuno se ne accorgesse.

#### Frequenze non bloccanti (requisito del 9 novembre)

Nel mondo radioamatoriale le prove si fanno spesso su **una frequenza nota**: la scelta delle frequenze dei profili **non deve mai essere bloccante**.

- **Qualunque voce può essere uguale a qualunque altra:** Standard, Messaggio e Qualità possono usare la stessa frequenza, e il ripetitore può avere **ingresso e uscita uguali** (utile in fase di prova con una sola frequenza). Il firmware e la pagina **non rifiutano e non avvisano in modo bloccante** frequenze uguali. Resta il solo controllo sulla banda (430-440 MHz).
- **Fatto il 9 novembre:** frequenza propria del profilo Messaggio (`CFG SET FREQMSG`, campo nella pagina, nessun controllo di uguaglianza, nota non bloccante quando sono uguali). Dal 10 novembre c'è anche `FREQQUAL` per la Qualità; manca il ripetitore, quando esisterà.
- **Predefinito:** tutte le memorie partono con la stessa frequenza (oggi il segnaposto 433,900 MHz); chi aggiorna dalla versione con una sola frequenza la ritrova in tutte le voci.
- **Stessa frequenza non vuol dire che si sentano:** due profili con la stessa frequenza ma SF e banda diversi **non si decodificano a vicenda**. Il controllo "canale occupato" (CAD) vede solo un segnale con il proprio SF, quindi una scheda Standard non si accorge di un messaggio SF12 in corso sulla stessa frequenza, e viceversa. Va bene per le prove, ma non è un canale condiviso: la pagina lo dice in una nota, senza impedirlo.
- **Ripetitore su una sola frequenza: l'antiripetizione diventa obbligatoria.** Con due frequenze diverse la scheda in ricezione non sente mai la propria trasmissione. Con la **stessa** frequenza la sente, la ripete di nuovo e così via: un anello infinito. Servono quindi **sempre**: (1) un flag **"ripetuto"** nei pacchetti ritrasmessi, che la scheda in ricezione del ripetitore ignora; (2) una piccola memoria degli ultimi pacchetti (sessione e numero di sequenza) per non ripetere due volte lo stesso. Chi riceve sia l'originale sia la copia ignora il duplicato, perché la sessione e la sequenza sono le stesse.
- **Schede senza il flag:** il flag è un bit nuovo nelle bandiere; le schede con il firmware precedente scartano i pacchetti con bandiere sconosciute, quindi non sentono le copie ripetute (non c'è rumore, solo silenzio).
- **Prova a frequenza unica:** mentre la scheda in trasmissione del ripetitore trasmette, quella in ricezione è sorda o sovraccarica e può perdere pacchetti degli utenti. È un limite della prova, non un difetto.

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

Sequenza di frame noti confrontata bit per bit, con risposta automatica; vedi [prova-collegamento.md](prova-collegamento.md). Provata sulle schede il 5 novembre. Livelli 2 e 3 (autotest del codec e prova dell'altoparlante): `CFG TEST CODEC` e `CFG TEST AUDIO`, provati il 7-8 novembre ([verifica-hardware-audio.md](verifica-hardware-audio.md)).

## Fondo del microfono: misura a cura dell'operatore (fatto il 5 novembre, da provare)

Il rumore di fondo cambia con l'ambiente (casa, auto, portatile, strada): la soglia dell'AGC non può essere una costante né, per ora, un automatismo. Soluzione semplice: **un valore standard (-60 dBFS) che ognuno misura e corregge**.

- Nella pagina di configurazione il pulsante **"Misura il fondo"**: stare in silenzio, 3 secondi; il valore trovato (90° percentile del livello dell'ingresso dell'AGC) viene salvato e vale subito. Una misura instabile (colpo, voce) non viene salvata.
- Poi **− e +** (e il campo numerico) alzano o abbassano il fondo di 1 dB per adattare l'AGC al proprio hardware, con effetto immediato. Range da -80 a -36 dBFS, standard -60; la soglia dell'AGC è il fondo + 6 dB.
- Firmware: `CFG MIC MEASURE`, `CFG SET MICFLOOR`, impostazione salvata in NVS. Il guadagno massimo (30 dB) resta in `user_config.h`.
- **Da decidere dopo l'ascolto della voce** (serve l'altoparlante): se serve anche la calibrazione automatica o l'adattamento continuo. Vedi [taratura-microfono.md](taratura-microfono.md).

## Verifica dell'hardware audio (scritta il 5 novembre, provata il 7-8 novembre)

Per un collega con una sola scheda: prova del microfono (silenzio contro voce) e prova combinata di altoparlante e microfono (tre toni misurati con Goertzel), dalla pagina di configurazione, con la spiegazione di falsi positivi e negativi.
Vedi [verifica-hardware-audio.md](verifica-hardware-audio.md). Le soglie vanno messe a punto sulle schede vere.

## Versioni (deciso il 7 novembre)

Numerazione semantica con beta e rc ([versioni.md](versioni.md)). La prima etichetta, `v0.1.0-beta.1`, è stata pubblicata l'8 novembre, quando la voce è passata da una scheda all'altra ed è risultata comprensibile.

## 4. Flasher web (dopo la configurazione a runtime)

- Pagina statica con `esp-web-tools`, un manifest per versione e tipo (installazione di fabbrica, aggiornamento), come nell'esempio di CA2RXU.
- I binari li produce il workflow `.github/workflows/build.yml`: `web_factory.bin` (all'indirizzo 0) e `ota_update.bin` (a 0x10000), con tag `v*` anche in una release di GitHub.
- Il nominativo di default è **N0CALL** con trasmissione bloccata, quindi il binario è distribuibile; nominativo e frequenza si impostano con la pagina di configurazione.
- Licenza scelta (GPL-3.0): chi distribuisce i binari deve rendere disponibile il sorgente corrispondente.

## 5. T-Beam V1.2 (batteria e GPS)

- **Fase 1** (fatta e provata il 4 novembre: avvio, AXP2101, radio, tasto IO38, trasmissione): alimentazione AXP2101 (radio accesa, GPS spento), tasto di bordo IO38 come PTT, pin audio propri (microfono su GPIO36, altoparlante su GPIO2), due ambienti `lora32` e `tbeam`, compilazione di entrambi su Actions.
- **Fase 2**: batteria e carica sull'OLED e in seriale, avviso di batteria scarica con un bip.
- **Fase 3** (GPS): lettura del GPS fatta e provata il 10 novembre (vedi sotto). Ancora da fare: locatore Maidenhead e ora precisa, per esempio nell'intestazione di inizio e fine turno.
- **Posizione nei pacchetti (09/11/2026), decisioni:** protocollo **v2** (flag `POSITION`, blocco da 7 byte: stato, latitudine e longitudine a 24 bit); trasmissione **spenta di default**; **solo nel pacchetto `START`** (i passaggi sono brevi: anche un mezzo in movimento aggiorna la posizione a ogni turno). Fatto il 09/11/2026 sul PC e nel firmware: blocco, `CFG SET POS`, `CFG SET QTH` (posizione fissa), distanza in km sul display (se si conosce la propria posizione). **Lettura del GPS della T-Beam: scritta il 10/11/2026, da provare su scheda** (`CFG SET GPS ON`, spento di default; NMEA GGA e RMC a 9600 baud, buffer da 2 KB, alimentazione ALDO3; fix recente = posizione dal GPS, altrimenti il QTH fisso, altrimenti nessuna; interprete e scelta della posizione testati sul PC con le frasi di esempio e con rumore, checksum errati e coordinate non valide).
- Se la scheda risultasse con radio SX1262 serve una variante di `RadioLora`.

## Aperto

- Frequenza di lavoro: ancora il segnaposto 433,900 MHz in `src/user_config.h`, da scegliere col band plan IARU R1 / ARI.
- Licenza: scelta la **GPL-3.0** (file `LICENSE`), coerente con la libreria `esp32_codec2` (vedi [firmware.md](firmware.md)).
- Prove su scheda: collegamento nei due versi tra T-Beam e T3, microfono, prova di collegamento (5 novembre); altoparlante e voce a turni provati l'8 novembre, menu, messaggi, posizione, GPS e SDR il 9-10 novembre (vedi [prove.md](prove.md)).
- **Fatto il 10/11/2026 sul PC e nel firmware, da provare su scheda: percentuale di avanzamento nel modo Messaggio (richiesta del 09/11/2026), con campo `TOTAL` nel pacchetto START, e segnale "MSG" in Morse all'inizio della ricezione del messaggio ([modo-messaggio.md](modo-messaggio.md)).** Richiesta originale: il display deve mostrare da 1% a 100%, in trasmissione e in ricezione, perché un messaggio lungo dura minuti e sembra bloccato. TX: la lunghezza è nota dopo la registrazione. RX: il pacchetto START deve dichiarare quanti pacchetti seguono (campo nell'intestazione del modo Messaggio, protocollo v2).
