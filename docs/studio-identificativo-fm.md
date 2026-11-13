# Identificativo del ripetitore in FM: Morse a tono (studio di fattibilità e decisione)

**In sintesi (12/11/2026).** Il ripetitore trasmette il **proprio nominativo in Morse, con un tono modulato in FM stretta** (canale da 12,5 kHz: 15 parole al minuto, tono 700 Hz, deviazione ±2,5 kHz), dopo l'identificativo in testo per le schede LoRa (ogni 1-10 minuti, `CFG SET IDINT`; si spegne con `CFG SET IDMORSE OFF`). Chi **non ha schede LoRa** lo sente con un ricevitore FM qualsiasi e capisce che quella frequenza è occupata da un ripetitore LoRa. È già nel firmware del ripetitore; i valori sono da misurare con il firmware di prova `CFG RADIOTEST` (sezioni in fondo). Il CW puro (portante accesa e spenta) è descritto qui come alternativa e resta solo nella prova.


Domanda di partenza: le schede LoRa possono trasmettere un identificativo che **chi non ha una scheda LoRa** capisce con un normale ricevitore (CW puro a banda stretta, oppure FM a 12,5 kHz con un tono Morse), così da sapere che la frequenza è occupata da un dispositivo LoRa? **Solo studio: nessun codice scritto, nessuna prova sul banco.** Quello che segue distingue ciò che ho verificato da ciò che resta da misurare.

## Cosa ho verificato (fonti)

- Il modulo radio delle schede a 433 MHz è un **SX1278** ([LILYGO](https://wiki.lilygo.cc/get_started/en/LoRa_GPS/LoRa32/LoRa32.html)). Oltre a LoRa il chip ha i modi **FSK e OOK**, con un modo "continuo" e una forma di ramp/filtro regolabile nel registro `PaRamp` ([datasheet SX1276/77/78/79](https://cdn.sparkfun.com/assets/7/7/3/2/2/SX1276_Datasheet.pdf)).
- Sulla T3 V1.6.1 standard il piedino **DIO2 è collegato al GPIO32** (DIO0 = 26, DIO1 = 33), secondo la [wiki LILYGO](https://wiki.lilygo.cc/get_started/en/LoRa_GPS/LoRa32/LoRa32.html) e la [scheda di ESPHome](https://devices.esphome.io/devices/lilygo-t3-lora32-v161/). La tabella della variante con TCXO nella wiki è in contrasto con questa: va controllata sul repository ufficiale se la scheda ne ha uno. **Per la T-Beam V1.2 non ho verificato il collegamento di DIO2.**
- **RadioLib** ([documentazione SX127x](https://jgromes.github.io/RadioLib/class_s_x127x.html)): `transmitDirect()` funziona solo in FSK/OOK, usa DIO1 come clock e DIO2 come dato, e in quel modo il modulo non può trasmettere né ricevere pacchetti. `setOOK(true)` passa alla modulazione OOK.
- **Non ho potuto leggere il datasheet** (il suo sito non è raggiungibile da questa sessione): i numeri che servono (deviazione minima e massima, tempi di salita della potenza, tempi di aggancio del PLL, tolleranza del quarzo) sono scritti sotto come **da verificare**, non come fatti.

## Due modi di fare l'identificativo con la radio che c'è

Entrambi usano i pacchetti FSK del SX1278, quindi **non servono fili né DIO2**; la scheda esce da LoRa, trasmette, e torna a LoRa (la stessa cosa che già fa per l'escursione MeshCom, con `begin`/`beginFSK` di RadioLib).

### A. CW puro (portante accesa e spenta, tipo A1A)
Un pacchetto FSK con tutti i bit a 1 (preambolo e parola di sincronismo compresi, CRC spento) è una portante **senza modulazione**, spostata di una deviazione fissa. Un punto = un pacchetto breve, una linea = uno tre volte più lungo, le pause = silenzio.
- Si ascolta con un ricevitore SSB/CW, con un SDR, o a orecchio con un ricevitore con BFO. Un ricevitore FM **sente solo il rumore sparire** (la portante pura non produce suono): per chi ha un walkie-talkie FM non basta.
- Pro: la banda occupata è minima (qualche centinaio di Hz); il Morse è quello vero.
- Da misurare: i "click" del tasto. Il registro `PaRamp` serve a rallentare la salita della potenza; con il valore di partenza lo spettro del tasto può allargarsi. Ramp lunga e punti di 60-80 ms (circa 15-20 parole al minuto) dovrebbero bastare.

### B. Tono Morse in FM stretta (tipo F2A / "MCW")
Un pacchetto FSK con i byte `0xAA` (10101010...) fa alternare la frequenza fra due valori a metà del bit rate: è un **tono quadro**. Con 1400 bit/s il tono è a 700 Hz; con deviazione di ±2,5 kHz (il valore normale di un canale FM da 12,5 kHz) un ricevitore FM lo riproduce come un fischio. Si tasta il Morse con pacchetti di durata diversa, come sopra.
- Pro: **qualunque ricetrasmettitore FM lo sente e lo decodifica a orecchio**, ed è la soluzione che risponde alla tua domanda ("capire che la frequenza è occupata").
- Da misurare: la deviazione reale (il minimo e il massimo del chip, e la forma con il filtro gaussiano) e il contenuto armonico del tono quadro.
- Con il tono sinusoidale vero servirebbe DIO2 pilotato dal microcontrollore a velocità audio; si evita con il trucco del bit rate.

### Cose comuni ai due modi
- **Durata:** un nominativo di 6 caratteri a 15-20 parole al minuto dura circa 5-8 secondi. Durante l'identificativo la scheda TX non trasmette LoRa e non ascolta: la voce che arriva in quel momento può perdere i primi pacchetti (si può anticipare con l'annuncio di "occupato", modo 6, prima di uscire da LoRa).
- **Il CAD delle stazioni LoRa non vede un segnale FSK**: chi vuole trasmettere nei secondi dell'identificativo non se ne accorge. L'annuncio di occupato aiuta, ma non per le stazioni senza firmware aggiornato.
- **Precisione di frequenza** (da misurare): su 433 MHz ogni ppm sono 433 Hz. Con un quarzo normale da ±10 ppm l'errore può arrivare a ±4 kHz, molto per un canale da 12,5 kHz e per un CW che si ascolta a 700 Hz. Non so se la T3 V1.6.1 che hai ha il TCXO (le fonti non concordano). Si misura con l'RTL-SDR come già fatto per MeshCom (correzione in ppm); se l'errore è fisso si può compensare nel firmware.
- **Purezza spettrale e potenza** (da misurare): armoniche e spurie dell'amplificatore, e la potenza reale con i 17 dBm impostati. Le schede non hanno un filtro passa-basso dichiarato; con un ripetitore vero ne servirebbe uno.

## Altre risposte alle tue domande

- **Il bip finale con il Morse:** il problema non c'è. Il bip di fine trasmissione lo suona una scheda LoRa che *riceve* un pacchetto LoRa valido; il Morse FSK/OOK non è un pacchetto LoRa e le schede non lo decodificano. L'identificativo in modo testo (sotto) non suona. Il Morse resta per chi ascolta con un ricevitore normale.
- **Modo testo per le schede, Morse per tutti gli altri:** sono due cose indipendenti e si possono avere insieme. Il testo è già fatto (sotto). Il Morse sarebbe un secondo passo, con la stessa cadenza o più rada.

## Verdetto

**Fattibile con l'hardware attuale** per entrambi i modi, senza fili aggiunti, con la parte difficile spostata sulle misure: precisione di frequenza, deviazione, spettro del tasto. Il modo **B (tono Morse in FM stretta)** è quello che serve a chi ha un ricevitore FM qualsiasi; il **A (CW puro)** a chi ascolta in SSB o con un SDR. Ordine di prova proposto: (1) B su una T3, ascoltato con un walkie-talkie e con RTL-SDR, per misurare la frequenza del tono, la deviazione e l'errore di frequenza; (2) A con diverse rampe; (3) controllo di DIO2 sulla T-Beam solo se serve un tono sinusoidale. Il firmware di prova per le misure (1) e (2) è scritto (sotto); l'identificativo vero del ripetitore in Morse si scrive solo dopo le misure: se lo spettro non è pulito, va ripensato (filtro o amplificatore esterni).

## Firmware di prova (12/11/2026)

Non c'è un firmware a parte: i comandi di prova stanno nel firmware normale (come `CFG TEST AUDIO`), quindi basta il firmware T3 dell'ultimo run verde di [GitHub Actions](https://github.com/IU5HJU/VoLoRa/actions) (artefatto `firmware-t3-v161`). La parte radio è scritta contro **le sorgenti vere di RadioLib 7.5.0** (la stessa versione del progetto: controllato che la sintassi compili con quelle intestazioni); **non è mai stata eseguita su una scheda**.

**Cosa ho potuto verificare dalle sorgenti di RadioLib** (non dal datasheet, che non è raggiungibile da questa sessione):
- deviazione di frequenza: minima 0,6 kHz (il codice la chiama "il valore più basso, necessario per i modi digitali"), massima 200 kHz, con il vincolo deviazione + bit rate/2 ≤ 250 kHz; passo di circa 61 Hz (quarzo da 32 MHz);
- bit rate FSK da 0,5 a 300 kbit/s; preambolo, parola di sincronismo (si può spegnere) e CRC (si può spegnere) sono regolabili, quindi un pacchetto può essere **solo** il suo contenuto;
- filtro gaussiano BT 0,3, 0,5 o 1,0 e salita della potenza stanno nello stesso registro `PaRamp` (0x0A): il filtro nei bit 6-5; per la salita uso i bit 3-0, **con il significato dei valori da verificare sul banco** (parto dal valore 0 e salgo).

**Comandi** (dal terminale seriale a 115200 o dal sito, scheda normale con un nominativo vero; la potenza è quella di `CFG SET POWER`, la frequenza è quella del profilo in uso):

| Comando | Cosa trasmette |
|---|---|
| `CFG RADIOTEST CARRIER 5` | portante pura (tutti i bit a 1) per 5 s: serve a misurare la **frequenza** e la sua deriva |
| `CFG RADIOTEST TONE 5 hz=700 dev=2.5` | tono continuo in FM (bit 1010...) per 5 s: serve a misurare **deviazione, frequenza del tono, armoniche** |
| `CFG RADIOTEST MORSE FM IR5XXX` | l'identificativo in Morse con tono in FM stretta (15 parole al minuto, tono 700 Hz, deviazione ±2,5 kHz) |
| `CFG RADIOTEST MORSE CW IR5XXX` | lo stesso in CW puro (portante accesa e spenta) |
| `CFG RADIOTEST BEACON FM IR5XXX 60` | l'identificativo in FM ripetuto ogni 60 s (da 10 s a un'ora), per vedere nel tempo **frequenza, rientro a LoRa e deriva**; si ferma con un tasto qualsiasi dalla seriale |

Parametri facoltativi di tutti i comandi (dopo i fissi): `wpm=5..30`, `hz=300..2500` (tono, solo FM), `dev=0.6..10` (kHz, solo FM), `shape=0|0.3|0.5|1.0` (filtro gaussiano, solo FM), `ramp=0..15` (salita della potenza). Il testo ha lettere e cifre; `_` vale spazio. Esempio per provare le rampe del tasto: `CFG RADIOTEST MORSE CW IR5XXX ramp=0`, poi `ramp=9`, e confrontare i "click" nello spettro.

**Come lavora:** la scheda esce da LoRa (`beginFSK` di RadioLib), manda pacchetti FSK a lunghezza fissa senza preambolo, sincronismo né CRC (`0xFF` = portante, `0xAA` = tono quadro di bit rate / 2), e torna a LoRa e in ascolto. Un punto e una linea sono pacchetti della durata giusta (in FM a 700 Hz un punto a 15 wpm è di 14 byte a 1,4 kbit/s; in CW 24 byte a 2,4 kbit/s), le pause sono silenzio. Per il CW la frequenza è spostata di -0,6 kHz per compensare la deviazione minima, così la portante cade sulla frequenza del profilo. La scheda **rifiuta** la prova con N0CALL, se sta ricevendo o trasmettendo o se il canale è occupato (CAD). Sulla seriale stampa tutti i parametri usati (`CFG RADIOTEST start ...`), la durata misurata contro quella prevista e il **tempo di cambio LoRa → FSK → LoRa** di ogni ciclo.

**Cosa misurare e annotare** (con RTL-SDR e un walkie-talkie FM sulla frequenza, potenza minima `CFG SET POWER 2`, antenna collegata):
1. *Portante (`CARRIER`)*: frequenza reale contro quella impostata (errore in kHz e ppm), deriva in 5 s e a freddo/caldo. Fra un pacchetto e l'altro da 255 byte (circa 850 ms) la portante si ferma un attimo: quanto dura?
2. *Tono (`TONE`)*: frequenza del tono (deve essere 700 Hz se `hz=700`), deviazione di picco, armoniche del quadro, se il walkie-talkie lo riproduce e a che volume.
3. *Morse FM*: si capisce a orecchio al walkie-talkie? la durata dei punti è giusta (misurata contro quella prevista)? ci sono ritardi all'inizio dei pacchetti (tempo di aggancio e salita)?
4. *Morse CW*: lo stesso in SSB/SDR; i click del tasto con `ramp=0` e `ramp=9`: larghezza di banda occupata.
5. *Beacon* per mezz'ora: la scheda torna a LoRa ogni volta? (si vede dal tempo di ritorno e riascoltando un pacchetto LoRa di un'altra scheda dopo il Morse.)
6. *Spettro*: potenza reale, armoniche e spurie dell'amplificatore (con l'SDR sulla banda di 433 MHz e sui multipli).

Quando hai i numeri, tarare i parametri (tono, deviazione, rampa, wpm) e decidere se e come inserire il Morse nell'identificativo del ripetitore.

## Decisione del 12/11/2026: il Morse in FM entra subito nell'identificativo

Dopo il testo dell'identificativo periodico (e con IDENTIFICA del sysop) il ripetitore manda il nominativo in **Morse con tono in FM stretta**: 15 parole al minuto, 700 Hz, deviazione ±2,5 kHz, `CFG SET IDMORSE ON` (di base acceso). Provato sul PC con un'altra radio finta (`test_tx_scheduler`: piano dei pacchetti, durata, interruzione dalla voce, TX spenta, radio che non va in FSK); **parametri e comportamento della radio vera sono da misurare** con il firmware di prova qui sopra, sulla T3 TX con N0CALL tolto solo a banco e a potenza minima. Il CW puro resta solo nella prova. Se le misure dicono che tono o deviazione vanno cambiati, sono tre costanti nel firmware.


## Prova del 13/11/2026 sul nodo e correzione: portante continua

Prima prova su scheda con un ricevitore FM: l'identificativo si ascolta bene, ma la **portante non è continua**: ogni punto e ogni linea era un pacchetto a sé e nelle pause la radio restava spenta, quindi il ricevitore sentiva la portante comparire **un attimo prima del tono** (lo squelch si apre a ogni elemento) e spegnersi in ogni pausa.

Correzione (nodo e ripetitore, stesso codice di trasmissione):
- **Portante iniziale** di 500 ms (pacchetto a tutti 1, senza modulazione) prima del primo tono: lo squelch del ricevitore si apre prima del Morse.
- **Nelle pause fra i toni la portante continua** (stessi pacchetti a tutti 1, durata uguale alla pausa del Morse) invece di spegnere la radio. Fra un pacchetto e il successivo il chip si rimette in trasmissione: la portante cade solo per quell'istante (da misurare).
- **Coda** di 250 ms dopo l'ultimo tono, poi si torna a LoRa.
- La radio rimane in trasmissione per tutto l'identificativo (circa 0,75 s in più): la durata conta nel limite di occupazione del canale come i toni.

Da verificare sul banco: (1) se il ricevitore non sente più la portante comparire e sparire; (2) se nei passaggi fra tono e pausa si sente un colpo: la portante pura è spostata di +deviazione rispetto alla frequenza media del tono (con ±2,5 kHz sono 2,5 kHz di scalino). Se il colpo disturba, i rimedi sono una deviazione più bassa o il filtro gaussiano; (3) l'eventuale "tick" fra due pacchetti consecutivi.
