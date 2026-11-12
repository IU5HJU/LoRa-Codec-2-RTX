# Studio di fattibilità: identificativo del ripetitore in CW o in FM stretta

12/11/2026. Domanda: le schede LoRa possono trasmettere un identificativo che **chi non ha una scheda LoRa** capisce con un normale ricevitore (CW puro a banda stretta, oppure FM a 12,5 kHz con un tono Morse), così da sapere che la frequenza è occupata da un dispositivo LoRa? **Solo studio: nessun codice scritto, nessuna prova sul banco.** Quello che segue distingue ciò che ho verificato da ciò che resta da misurare.

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

**Fattibile con l'hardware attuale** per entrambi i modi, senza fili aggiunti, con la parte difficile spostata sulle misure: precisione di frequenza, deviazione, spettro del tasto. Il modo **B (tono Morse in FM stretta)** è quello che serve a chi ha un ricevitore FM qualsiasi; il **A (CW puro)** a chi ascolta in SSB o con un SDR. Ordine di prova proposto: (1) B su una T3, ascoltato con un walkie-talkie e con RTL-SDR, per misurare la frequenza del tono, la deviazione e l'errore di frequenza; (2) A con diverse rampe; (3) controllo di DIO2 sulla T-Beam solo se serve un tono sinusoidale. Non scrivo il codice prima della prova (1): se lo spettro non è pulito, l'identificativo in aria va ripensato (filtro o amplificatore esterni).
