# Prova di collegamento

Serve a sapere con certezza se **radio e protocollo** funzionano, senza microfono, altoparlante né codec, e a separare
i dubbi: se la prova è perfetta e la voce è brutta, il problema è nell'audio, non nella radio.

## Perché non si usano toni

Codec2 1200 è un codificatore di voce: modella il parlato e non conserva un tono puro. Provato sul PC con la libreria del firmware
(sinusoidi a -12 dBFS): 200 Hz esce a 330 Hz e 17 dB più basso, 440 Hz a 600 Hz e 26 dB più basso, 1000 Hz a 33 dB più basso, 2000 Hz a 53 dB più basso.
Misurare frequenza e ampiezza dopo il decodificatore darebbe falsi allarmi sul codec, quindi la prova usa frame **noti** e li confronta nel dominio dei bit.

## Come funziona

Dal 14/11/2026 la prova di collegamento **è la prova di potenza** (la vecchia prova unica da 12 secondi è stata tolta dal firmware): cerca la potenza minima con cui le due schede si sentono. La descrizione completa è nella sezione [Prova di potenza](#prova-di-potenza-potenza-minima). Ogni passo è una prova breve:

1. Il trasmettitore invia **75 frame (3 s)** non di voce ma pseudocasuali, calcolati da (sessione, numero del frame), con gli stessi pacchetti, la stessa parità XOR e gli stessi tempi della voce (modo 2 nell'intestazione). Il microfono non serve.
2. **Solo una scheda con la prova accesa ascolta le prove**: in ascolto normale (prova spenta) le ignora del tutto, senza schermata, senza misura e senza risposta (la voce normale passa come sempre). Il ricevitore in prova riconosce la prova dal modo del pacchetto, rigenera la sequenza attesa e confronta ogni frame. **Non riproduce nulla** (niente rumore nell'altoparlante).
   Alla fine di ogni passo mostra e stampa: frame arrivati identici, **danneggiati**, **persi** (non recuperabili), pacchetti **recuperati dalla parità**, RSSI e SNR medi.
3. Il ricevitore in prova, dopo circa 0,7 s, **trasmette un pacchetto di risposta** (modo 3) con lo stesso risultato; il trasmettitore lo registra e passa al passo successivo. Se sul ricevitore la prova è spenta (o non ha il nominativo) non vede niente e non risponde: chi trasmette vede "nessuna risposta" e la prova di potenza si ferma dopo due passi muti.

## Come si usa

1. Su **entrambe** le schede attiva la prova in uno di questi tre modi: **tre pressioni veloci del PTT** (due bip acuti e avviso "PROVA ATTIVA"; altre tre la spengono con un bip grave), `CFG TEST ON` dalla seriale, oppure il pulsante "Attiva la prova" nella [pagina web](https://iu5hju.github.io/VoLoRa/). Sull'OLED a riposo compare "PROVA ON".
   La prova **non è salvata**: all'accensione è sempre spenta, così nessuna scheda resta in prova per errore.
2. Premi il PTT su una scheda (serve un nominativo vero: con N0CALL il PTT è bloccato): parte la prova di potenza, vedi sotto.

## Tre clic sul PTT

Una pressione del PTT più breve di **0,3 s** è un "clic" e non trasmette: la trasmissione parte solo se il PTT resta premuto oltre 0,3 s (il microfono è comunque bufferizzato dalla pressione, quindi non si perde l'inizio della frase;
il nominativo parte con 0,3 s di ritardo). Tre clic, ciascuno entro 0,5 s dal precedente, attivano o disattivano la prova. **Con N0CALL (nessun nominativo) la prova non si accende**, né con i tre clic (compare l'avviso "IMPOSTA NOMINATIVO") né da seriale o pagina: partecipare vuol dire rispondere, cioè trasmettere, e senza nominativo non si trasmette. Una scheda senza nominativo non risponde mai alla prova; chi trasmette vede "nessuna risposta". Con una scheda che sta ricevendo i clic sono ignorati.
Una terza pressione subito dopo due clic è sempre il terzo clic, anche se tenuta premuta: per trasmettere dopo due clic basta aspettare mezzo secondo.

## Come leggere l'esito

| Esito | Significato |
|---|---|
| 100%, danneggiati 0, persi 0 | radio, protocollo e parità funzionano nelle condizioni della prova |
| persi > 0, danneggiati 0 | pacchetti non arrivati (distanza, antenna, interferenze, ricevitore troppo lento). "Recuperati" dice quanto ha salvato la parità |
| danneggiati > 0 | frame arrivati ma diversi: non dovrebbe succedere con il CRC della radio; segnalalo come errore |
| nessuna risposta | il ricevitore non ha la prova accesa (le prove le ascolta solo chi è in prova), non ha un nominativo, il canale era occupato, o la risposta non è tornata |

## Prova di potenza (potenza minima)

14/11/2026. Dal firmware di questa data **la prova di collegamento è la prova di potenza**: cerca la potenza minima con cui due schede si sentono, per tutta la gamma della scheda, da sola. (La vecchia prova unica da 12 secondi, 300 frame, è stata tolta dal firmware; i log di prova di prima di questa data in [prove.md](prove.md) sono di quella.)

**Come si fa, su due schede:**
1. Su **entrambe** accendi la prova come sempre: **tre pressioni brevi del PTT** (due bip acuti, "PROVA ATTIVA"), `CFG TEST ON` o il pulsante della pagina.
2. Su **una** delle due, con il nominativo vero (non N0CALL) e in profilo Standard o Range, **premi il PTT** (basta una pressione, non serve tenerlo). Parte la prova di potenza. Un altro PTT la ferma.
3. La scheda che trasmette ripete la prova **dalla potenza massima (17 dBm) alla minima (2 dBm), un dBm alla volta**: a ogni passo una prova breve (75 frame, 3 secondi) e **attesa della risposta** dell'altra scheda. Sul display si vede la potenza in corso (`PROVA DI POTENZA`, `14 dBm`, passo e se l'altra ha risposto); sul registro seriale una riga per passo, per esempio `PROVA POTENZA 11 dBm: risposta, frame ok 100%, RSSI -97 dBm, SNR 8 dB` oppure `... nessuna risposta`.
4. Alla fine, **su entrambe le schede** il display mostra l'esito per 30 secondi: **`MIN 9 dBm`** (la potenza minima con tutti i frame intatti, contando dalla massima senza interruzioni) e **`sente fino a 7 dBm`** (la potenza minima a cui l'altra ha ancora risposto con almeno un frame). Su quella che ha ricevuto compare `POTENZA da <nominativo>`. Se nemmeno alla massima potenza i frame sono tutti intatti si legge `NON PULITA`, se non risponde nessuno `NESSUNA`. Stessa cosa sul registro (`PROVA POTENZA finita ...` e `PROVA POTENZA esito da ...`), con i bip acuti se c'è un minimo pulito, un bip grave altrimenti. Poi la potenza torna a quella impostata.

Come margine conviene lavorare qualche dBm sopra il minimo trovato.

**Come funziona dentro:**
- Si ferma da sola dopo **due passi di fila senza risposta**: dura da circa 20 secondi (collegamento debole) a un minuto e mezzo. Se già alla potenza massima non risponde nessuno, finisce dopo due passi: controlla che sull'altra scheda la prova sia accesa.
- L'**altra scheda** (in prova) risponde a ogni passo come per la prova normale (alla sua potenza di sempre) e per tutto il tempo mostra la schermata di ogni prova breve ("Parz. 75/300": è normale, sono prove da 3 secondi). Alla fine, chi ha trasmesso le **manda l'esito** (due copie, nello stesso pacchetto di risposta, modo 3, con il primo byte maggiore o uguale a 0x80: la risposta normale comincia sempre con 0 o 1). Così lo vedono tutte e due. Se quel pacchetto si perde, l'altra scheda ha comunque le schermate dei singoli passi.
- Si misura **il verso da chi trasmette verso l'altra scheda**: la risposta dell'altra parte alla **sua** potenza normale. Per l'altro verso si ripete la prova a parti invertite.
- Non c'è nel profilo Messaggio (il tempo in aria sarebbe troppo lungo).
- **Da misurare sul campo:** quanto la potenza impostata (`setOutputPower` del SX1278) corrisponda davvero alla potenza in antenna a ogni dBm. La prova dà la potenza *impostata*, non misurata.

## Limiti

- **Non prova il codec né l'audio**: solo radio e protocollo. Autotest del codec e prova dell'altoparlante sono i livelli successivi, con l'hardware audio.
- Le due schede devono avere la **stessa versione** del firmware: una scheda più vecchia scarterebbe i pacchetti di prova (modo sconosciuto).
- La prova è una trasmissione: il nominativo viaggia in chiaro, ci si attiene a frequenza e potenza consentite.
- Misura il collegamento in **un verso** per volta (chi trasmette -> chi riceve). La risposta, che viaggia nel verso opposto, è un solo pacchetto breve: se arriva, il percorso inverso funziona; se non arriva, non si può dire quale dei due pacchetti sia mancato.

## Uscire dalla prova

Si esce con le stesse tre pressioni brevi del PTT che l'hanno accesa, in qualunque momento:
- anche mentre la scheda sta ricevendo (altre schede o rumore in banda): in prova i clic contano sempre;
- anche a prova di potenza in corso: la prima pressione ferma la prova e conta già come primo clic, quindi bastano tre pressioni in tutto;
- in prova il clic non apre né chiude messaggi in arrivo.

## Distanza in km sul display

Dal 14/11/2026 la prova di potenza porta anche la **posizione** (la stessa che viaggia nei turni di voce: GPS della T-Beam oppure posizione fissa `CFG SET QTH`), se sulla scheda che trasmette è acceso l'invio della posizione (`CFG SET POS ON`, **spento di default**: la posizione è un dato personale). Compare nel pacchetto `START` di ogni passo, nella risposta e nell'esito finale.

- La scheda che riceve calcola la distanza dalla **propria** posizione (QTH o GPS), quindi per vederla servono **tutte e due le cose**: l'altra scheda ha `POS ON` e questa ha una posizione (`CFG SET QTH ...` oppure il GPS con fix).
- Nell'esito finale sul display la distanza sta sull'ultima riga: `100% a MIN  5.0km` (potenza minima pulita) oppure `frame persi 5.0km`. Senza posizione resta `(frame 100% a MIN)` / `ma con frame persi`.
- Per avere la distanza su **entrambe** le schede, `POS ON` e una posizione vanno impostate su tutte e due.

