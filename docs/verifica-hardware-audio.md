# Verifica dell'hardware audio con una sola scheda

Per chi ha montato microfono (INMP441), amplificatore (MAX98357A) e altoparlante e vuole sapere se il montaggio è a posto, **senza strumenti e con una sola scheda**.
Si fa dalla [pagina di configurazione](https://iu5hju.github.io/LoRa-Codec-2-RTX/) (sezione "3. Verifica dell'hardware audio"), con Chrome o Edge e il cavo USB. Gli stessi comandi sono sulla seriale (`CFG TEST AUDIO`, `CFG MIC MEASURE`).

**Prima:** antenna collegata, microSD tolta, collegamenti secondo [pinout.md](pinout.md) e [audio.md](audio.md). Installa il firmware e collega la scheda (passo 2 della pagina).

## 1. Microfono

Pulsante **"Prova il microfono"**: 3 secondi di silenzio, poi 3 secondi in cui parli vicino al microfono (per esempio "prova, uno, due, tre"). La pagina confronta i due livelli (quello visto dall'AGC, dopo il filtro passa-alto).

| Esito | Significato |
|---|---|
| differenza di almeno 15 dB | il microfono sente la voce |
| 6-15 dB | differenza piccola: avvicina il microfono, parla più forte, controlla i collegamenti |
| meno di 6 dB | non distingue la voce dal silenzio: controlla VDD a 3,3 V, GND, `L/R` a GND, `SCK`/`WS`/`SD` e prova `LCR_MIC_SLOT 1` |
| silenzio instabile | hai parlato o toccato il microfono durante i primi 3 secondi: ripeti |

## 2. Altoparlante e microfono insieme

Pulsante **"Suona i toni"**: la scheda suona tre toni (500, 1000 e 2000 Hz, mezzo secondo l'uno) a **volume basso e fisso** (indipendente dal volume impostato). **Ascoltali**: devono essere tre toni distinti, chiari e senza fruscio.
Nello stesso tempo il microfono misura quanto sente ogni tono, **solo a quella frequenza** (algoritmo di Goertzel), in confronto a una breve pausa prima del tono.

La tabella mostra pausa, tono e differenza in dB. Esito per tono: **sentito** (almeno +10 dB), **dubbio** (da +3 a +10), **non sentito** (sotto +3).
Giudizio complessivo: due o tre toni sentiti = catena viva; nessuno = vedi sotto; altro = dubbio, ripeti avvicinando il microfono all'altoparlante.

### Fase 2: gli stessi toni con la radio in trasmissione

La prova ha **due fasi**. La fase 1 è quella appena descritta: tre toni, senza radio. Nella **fase 2** la scheda suona **gli stessi tre toni mentre trasmette in continuo** (pacchetti di controllo, modo 15 del protocollo: i ricevitori li ignorano). Se la radiofrequenza rientra nel microfono, nei fili dell'amplificatore o nell'alimentazione, lo senti **a orecchio** (ronzii, scatti, fruscio sopra i toni) e lo vedi **nei numeri**: la tabella mostra le due fasi affiancate e, per ogni tono, di quanti dB cambia il livello della pausa (microfono in silenzio) quando la radio trasmette. **Serve l'antenna collegata e un nominativo vero** (con N0CALL la fase 2 è saltata, come ogni trasmissione). Non parlare e non toccare il microfono finché la prova non è finita (circa 10 secondi).

Esito: se il livello del microfono (prima e dopo il filtro passa-alto, in massimo) o dell'offset **sale di 10 dB o più** con la radio accesa, la RF rientra nei fili del microfono (accorcia i fili, allontanali dall'antenna, intrecciali o schermali; prova una potenza più bassa); da 5 a 10 dB, o toni che si sentono peggio di 6 dB, è dubbio e va ripetuto; sotto 5 dB nessun rientro. Le soglie sono provvisorie: annota i valori in [prove.md](prove.md).
Nel registro seriale le righe sono `CFG AUDIO f=…` (fase 1), `CFG AUDIO rf f=…` (fase 2), `CFG AUDIO m1/m2 rms=… agc=… dc=…` (massimi del microfono nelle due fasi) e `CFG AUDIO rf pkts=…`.

## 3. Codec2 sulla scheda, senza radio

Comando `CFG TEST CODEC` dalla seriale (il pulsante nella pagina web è stato tolto per semplificarla; si recupera dalla cronologia di git): dopo un bip **parli per 3 secondi** vicino al microfono; la scheda registra, poi riproduce **due volte** la stessa registrazione dal proprio altoparlante, entrambe volte **dallo stesso percorso della ricezione** (AGC, volume, limitatore, raddoppio a 16 kHz):

1. **ascolto 1:** l'audio del microfono così com'è, senza Codec2;
2. **ascolto 2:** lo stesso audio dopo codifica e decodifica Codec2 (display: "ASCOLTO 1" e "ASCOLTO 2");
3. **ascolto 3:** gli stessi frame decodificati, ma scritti **al ritmo della ricezione** (un frame ogni 40 ms, come fa il loop) **con il display** che si ridisegna come in ricezione;
4. **ascolto 4:** come il 3 ma **senza display**.

Non c'è radio, quindi servono una scheda sola e nessuna antenna. La pagina mostra i livelli e i picchi prima e dopo il Codec2, e i tempi di codifica e decodifica. I frame codificati vengono stampati nel registro come righe `CFG CODEC hex ...`: copiandole si possono decodificare sul PC con il Codec2 di riferimento.

Gli ascolti 3 e 4 servono a capire se il guasto è nelle **condizioni della ricezione** (ritmo di scrittura, display) invece che nel Codec2 o nell'altoparlante: se il 2 è chiaro e il 3 o il 4 no, è lì.

Come leggere l'ascolto:

- **1 chiaro, 2 chiaro ma "robotico":** tutto normale (il Codec2 a 1200 bps suona così). Il guasto è altrove: trasmissione, ricezione o radio.
- **1 chiaro, 2 solo crepitii (che magari seguono l'andamento della voce):** il difetto è nel Codec2 sulla scheda (codifica o decodifica).
- **Anche 1 con crepitii:** il difetto è nel percorso di riproduzione che il parlato attraversa e i toni no (AGC, limitatore o raddoppio della frequenza), oppure nell'altoparlante con segnali più complessi dei toni.

## Falsi positivi e falsi negativi

- **Falso positivo:** il microfono "sente" i toni anche senza che l'altoparlante li emetta, perché il rumore dell'amplificatore (classe D) entra nel microfono dai fili o dall'alimentazione (condividono 5 V, massa e i clock BCLK/WS). **Controllo:** ripeti la prova **con l'altoparlante scollegato**: se il microfono sente lo stesso, il collegamento è elettrico, non acustico, e il risultato non vale.
- **Falso negativo:** l'altoparlante suona ma il microfono non sente i toni, perché è lontano, coperto o il volume è basso. **Controllo:** se li hai sentiti a orecchio l'altoparlante funziona; avvicina il microfono e ripeti.
- La prova è **indicativa**: dice se la catena è viva, non com'è la qualità della voce, che si giudica ascoltando.

## Se non si sente nulla

Controlla in ordine: `VIN` dell'amplificatore a 5 V (USB), `SD` su GPIO25 (alto = acceso), `DIN` su GPIO4, `BCLK` su GPIO14 e `LRC` su GPIO13 (condivisi con il microfono), altoparlante ai morsetti `+`/`-`, e che `GAIN` sia libero. Un fruscio costante a riposo indica una massa o un'alimentazione disturbate.

## Soglie

Le soglie dell'esito (15 dB per il microfono; 10 e 3 dB per i toni) sono provvisorie e vanno messe a punto sulle schede vere: annota i valori che vedi in [prove.md](prove.md).
