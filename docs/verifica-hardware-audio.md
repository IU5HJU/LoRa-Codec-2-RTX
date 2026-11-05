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

## Falsi positivi e falsi negativi

- **Falso positivo:** il microfono "sente" i toni anche senza che l'altoparlante li emetta, perché il rumore dell'amplificatore (classe D) entra nel microfono dai fili o dall'alimentazione (condividono 5 V, massa e i clock BCLK/WS). **Controllo:** ripeti la prova **con l'altoparlante scollegato**: se il microfono sente lo stesso, il collegamento è elettrico, non acustico, e il risultato non vale.
- **Falso negativo:** l'altoparlante suona ma il microfono non sente i toni, perché è lontano, coperto o il volume è basso. **Controllo:** se li hai sentiti a orecchio l'altoparlante funziona; avvicina il microfono e ripeti.
- La prova è **indicativa**: dice se la catena è viva, non com'è la qualità della voce, che si giudica ascoltando.

## Se non si sente nulla

Controlla in ordine: `VIN` dell'amplificatore a 5 V (USB), `SD` su GPIO25 (alto = acceso), `DIN` su GPIO4, `BCLK` su GPIO14 e `LRC` su GPIO13 (condivisi con il microfono), altoparlante ai morsetti `+`/`-`, e che `GAIN` sia libero. Un fruscio costante a riposo indica una massa o un'alimentazione disturbate.

## Soglie

Le soglie dell'esito (15 dB per il microfono; 10 e 3 dB per i toni) sono provvisorie e vanno messe a punto sulle schede vere: annota i valori che vedi in [prove.md](prove.md).
