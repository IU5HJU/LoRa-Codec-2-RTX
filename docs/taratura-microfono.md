# Taratura del microfono: percorso, misure e come adattarla

Resoconto del 5 novembre 2026 sulla messa a punto dell'ingresso audio (INMP441 sulla LoRa32 T3 V1.6.1),
scritto anche per chi usa un **microfono diverso** o una scheda diversa e vuole rifare le misure.
Stato: **solo livelli letti dalla seriale, l'audio non è ancora stato ascoltato** (amplificatore e
altoparlante non sono ancora arrivati). Le conclusioni sulla qualità percepita vanno quindi verificate ad orecchio.
Catena e collegamenti in [audio.md](audio.md); registro delle prove in [prove.md](prove.md).

## In breve

- Il microfono funziona: nessun errore I2S, livelli coerenti, nessuna saturazione.
- La regolazione automatica del guadagno (AGC) era pensata per un parlante lontano. Con la bocca vicina e una stanza non silenziosa
  **amplificava il rumore di fondo fino a livelli quasi da voce**. Corretto con tre interventi: guadagno massimo ridotto, soglia di rumore alzata, ritorno del guadagno a riposo in assenza di voce.
- Il fondo grezzo è alto (circa -30 dBFS) ma quasi tutto **sotto i 300 Hz**, dove il filtro passa-alto lo elimina. Resta da capire se sia ambiente, cablaggio o alimentazione.
- Il fondo si misura e si regola dalla pagina web (e il guadagno massimo è in `src/user_config.h`). Per un altro microfono basta rifare tre misure e scegliere due numeri (procedura più sotto).

## Catena di elaborazione e punti di misura

```
I2S 32 bit @ 16 kHz (24 bit utili)
   │   [A] misura grezza:  "livello", "picco", "offset"   (per frame da 40 ms, senza la media del frame)
   ▼
passa-alto 300 Hz (Butterworth 2° ordine)  ->  decimazione 2x (FIR 31 tappe)  ->  8 kHz
   │   [B] misura "AGC in": livello RMS del frame all'ingresso dell'AGC
   ▼
AGC: guadagno da -20 a +30 dB verso un target di -24 dBFS RMS
   ▼
int16 @ 8 kHz, 320 campioni  ->  Codec2 1200 bps
```

Con il PTT premuto la seriale stampa due volte al secondo (`pio device monitor`, 115200 baud, oppure la finestra nera della
[pagina web](https://iu5hju.github.io/VoLoRa/)):

```
[mic] livello -30 dBFS, picco -25 dBFS, offset 3.6%, AGC in -61 dBFS, guadagno 12 dB, errori 0
```

| Voce | Cosa misura |
|---|---|
| livello | RMS grezzo, in dB rispetto al fondo scala I2S (0 dBFS = massimo), **prima** del filtro, senza il valore medio |
| picco | massimo del frame, stessa scala |
| offset | valore medio (componente continua) in % del fondo scala |
| AGC in | RMS **dopo** il passa-alto e la decimazione: è il valore che l'AGC confronta con la soglia |
| guadagno | guadagno attuale dell'AGC in dB |
| errori | letture I2S fallite (devono restare 0) |

Con la trasmissione disattivata (nominativo N0CALL) la riga si stampa lo stesso: utile per misurare senza che la radio disturbi.

## Cronologia delle misure (T3 + INMP441, stanza di casa)

| Condizione | livello | picco | AGC in | guadagno | Note |
|---|---|---|---|---|---|
| Silenzio, prima della correzione (max 36 dB, soglia -70) | -43...-69 | -37...-60 | n.d. | 30-36 dB | fondo amplificato verso il target |
| Silenzio, TX disattivata | -46...-58 | -42...-52 | n.d. | 36 dB | **non** dipende dalla radio |
| Silenzio, dopo la correzione (max 30, soglia -54, riposo 12 dB) | -28...-31 | -24...-27 | -58...-61 | 12-13 dB | uscita circa -48 dBFS |
| Voce vicina al microfono, dopo la correzione | -22...-30 | -13...-26 | -24...-35 | 1-9 dB | nessuna saturazione |

(dBFS. Le prime due righe sono precedenti all'introduzione di "AGC in".)

## Cosa è andato storto e perché

1. **Ipotesi di progetto sbagliata.** Il commento originale assumeva voce a distanza di parlato intorno a -60 dBFS, quindi
   guadagno fino a 36 dB e soglia di rumore a -70 dBFS. Nella realtà la voce vicina arriva a **-24...-35 dBFS all'ingresso
   dell'AGC**, e il fondo della stanza a **-61 dBFS**, sopra la soglia di -70: l'AGC lo trattava come voce debole e saliva al massimo.
2. **Soglia che "congelava" il guadagno invece di abbassarlo.** Sotto la soglia il guadagno non saliva più, ma restava al valore
   raggiunto durante la voce: il rumore tra una frase e l'altra veniva amplificato come prima. Ora sotto la soglia il guadagno
   torna piano (costante di tempo circa 2 s) verso 12 dB.
3. **Livello grezzo alto, ma a bassa frequenza.** Il livello grezzo in silenzio è -28...-31 dBFS, mentre dopo il filtro è -61 dBFS: 30 dB
   di differenza, quindi quasi tutta l'energia è sotto i 300 Hz. L'offset che oscilla fino a +-14% da un frame all'altro va nella stessa direzione
   (variazioni lente, non fruscio). Non è acustica a bassa frequenza e non dipende dalla trasmissione. Cause possibili, **non verificate**: urti ai
   cavi, ronzio di alimentazione, massa comune, fili lunghi, rumore sul 3,3 V.

## Valori attuali

| Parametro | Valore | Dove |
|---|---|---|
| Guadagno massimo AGC | 30 dB (era 36) | `LCR_MIC_AGC_MAX_GAIN_DB` in `src/user_config.h` |
| Fondo del microfono (soglia AGC = fondo + 6 dB) | -60 dBFS standard, quindi soglia -54 (era -70) | pagina di configurazione o `CFG SET MICFLOOR`; standard in `LCR_MIC_FLOOR_DBFS` |
| Guadagno a riposo | 12 dB, costante di tempo circa 2 s | `AgcConfig::idleGainDb`, `idleAlpha` |
| Target | -24 dBFS RMS | `AgcConfig::targetDbfs` |
| Guadagno minimo | -20 dB | `AgcConfig::minGainDb` |
| Filtro passa-alto | 300 Hz, 2° ordine | `FrontEndConfig::hpfHz` |

La soglia è circa 7 dB sopra il fondo misurato (-61) e circa 20 dB sotto la voce vicina (-35): c'è margine anche per chi parla più piano o più lontano.
Il guadagno si riduce a 1-9 dB con voce vicina, quindi il limite di 30 dB serve solo a chi parla distante.

## Come adattarla a un altro microfono

Servono la scheda, il cavo USB e un monitor seriale; la radio non trasmette (nominativo N0CALL, oppure semplicemente non si usa la voce).

1. **Controlli di base.** Sulla seriale `errori` deve restare 0. Silenzio digitale (picco sotto -80 dBFS) significa cavi o canale I2S sbagliato: provare `LCR_MIC_SLOT 1`.
   Un `offset` di decine di % costanti indica un collegamento errato.
2. **Silenzio.** Scheda ferma su un piano, senza toccare i cavi, PTT premuto 10 secondi. Annotare il valore tipico di **AGC in** (non il livello grezzo).
3. **Voce.** Frasi normali alla distanza d'uso reale (per esempio 5, 15, 30 cm). Annotare **AGC in** e il **picco** grezzo: non deve avvicinarsi a 0 dBFS.
4. **Scelta.**
   - **Fondo:** si misura dalla [pagina di configurazione](https://iu5hju.github.io/VoLoRa/) con "Misura il fondo" (un conto alla rovescia di 3 secondi per lasciare il mouse e la tastiera, poi 3 secondi di silenzio) e si corregge a mano con − e +. La soglia dell'AGC è il fondo più 6 dB e deve stare sotto la voce più debole che deve passare: con meno di 15 dB tra fondo e voce, il microfono o il cablaggio sono troppo rumorosi.
   - `LCR_MIC_AGC_MAX_GAIN_DB`: quanto serve a portare la voce più debole a -24 dBFS, quindi `-24 - (AGC in della voce debole)`, con un po' di margine.
5. **Verifica.** Ripetere 2 e 3: in silenzio il guadagno deve scendere verso 12 dB e `AGC in` restare sotto la soglia; parlando deve salire sopra e il guadagno stabilizzarsi.
6. **Ascolto.** Con amplificatore e altoparlante: fruscio nelle pause, sillabe tagliate all'inizio dopo una pausa lunga (il guadagno risale in circa mezzo secondo), pompaggio.

Nota per i microfoni analogici o a elettrete: l'adattatore I2S del firmware è per microfoni digitali (INMP441 e simili). Altri richiederebbero un ingresso diverso.

## Una conferma dal PC (10/11/2026)

Provando i modi del Codec2 con la voce registrata da un normale portatile ([codec2.md](codec2.md)) è emerso quanto il **livello dell'ingresso sia critico**: con il microfono troppo basso la voce era quasi impercettibile e il Codec2 suonava "pessimo", con il microfono troppo alto la registrazione saturava, e solo in una finestra stretta di livelli il risultato era buono, **molto peggio di quanto fanno le schede**. Il motivo è che le schede portano la voce al livello giusto con un **filtro passa-alto a 300 Hz e un AGC a −24 dBFS, tarati con il fondo del microfono**: è la stessa sensibilità ai livelli che si conosce da FreeDV, e non va sottovalutata. Chi vuole adattare il progetto a un altro microfono deve ripartire da qui. Lo strumento di confronto dei codec ha una normalizzazione che rimedia sul PC e misura il livello della prova.

## Domande aperte per chi se ne intende di audio

- **Passa-alto a 300 Hz:** adatto per Codec2 1200 e per il rumore osservato, o conviene una frequenza diversa? Sotto i 300 Hz il codec non ha comunque contenuto utile.
- **AGC contro guadagno fisso:** con l'AGC le voci forti e deboli arrivano allo stesso livello; con guadagno fisso si evita il pompaggio. Da giudicare ad orecchio.
- **Rumore tra le frasi:** con Codec2 a 1200 bps il rumore di fondo diventa un "ronzio" o "borbottio" del vocoder. È meglio una soglia più alta (silenzio vero) o un abbassamento morbido?
- **Rumore a bassa frequenza alla sorgente:** servono massa a stella, condensatore su VDD o altro? Quali misure consigliate (oscilloscopio sul 3,3 V e sui fili I2S)?
- **Posizione e foro del microfono:** il foro del suono deve restare libero; la capsula non va incollata né coperta.
- **Clic del PTT:** da verificare ad orecchio; se disturba, si scartano i primi frame dopo la pressione.

Chi misura con un microfono diverso può annotare i risultati in [prove.md](prove.md) con i valori di silenzio e di voce, e proporre i due numeri scelti.
