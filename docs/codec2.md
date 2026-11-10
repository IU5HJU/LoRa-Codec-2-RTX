# Codec2: modi, costo e ascolto comparativo (verifiche del 10/11/2026)

Verifiche del punto "Codec2" della [roadmap](roadmap.md), fatte **sul PC con gli stessi sorgenti che girano sulle schede** (`third_party/esp32_codec2-1.0.7`). Nessuna misura su scheda è stata ancora fatta con i modi nuovi: i numeri per l'ESP32 sono **stime**, scalate da una misura vera del modo 1200.

## 1. Modi disponibili

`esp32_codec2` 1.0.7 contiene e compila, tutti attivi, i modi **3200, 2400, 1600, 1400, 1300, 1200, 700C e 450** (più il 450PWB, non esaminato). I numeri vengono da `codec2_bits_per_frame` e `codec2_samples_per_frame`:

| Modo | Frame | Bit per frame | Byte per frame (interi) | Note |
|---|---|---|---|---|
| 3200 | 20 ms | 64 | 8 | il modo di M17, obiettivo della modalità Qualità |
| 2400 | 20 ms | 48 | 6 | ripiego della Qualità |
| 1600 | 40 ms | 64 | 8 | |
| 1400 | 40 ms | 56 | 7 | |
| 1300 | 40 ms | 52 | 7 | |
| **1200** | 40 ms | 48 | 6 | quello di oggi (Standard) |
| 700C | 40 ms | **28** | 4 | non è intero: in pacchetto serve l'impacchettamento a bit |
| 450 | 40 ms | **18** | 3 | idem |

## 2. CPU

Misura sul PC (x86, `-O2`) con un segnale armonico sintetico, non voce vera: **solo i rapporti tra i modi hanno senso**.

| Modo | Codifica per frame | Decodifica per frame | Codifica per secondo di audio | Decodifica per secondo di audio |
|---|---|---|---|---|
| 3200 | 44 µs (20 ms) | 27 µs | 2,2 ms | 1,4 ms |
| 2400 | 42 µs (20 ms) | 25 µs | 2,1 ms | 1,2 ms |
| 1600 | 79 µs | 52 µs | 2,0 ms | 1,3 ms |
| 1200 | 100 µs | 58 µs | 2,5 ms | 1,4 ms |
| 700C | 101 µs | 39 µs | 2,5 ms | 1,0 ms |
| 450 | 99 µs | 42 µs | 2,5 ms | 1,1 ms |

**Conclusione:** per secondo di audio **tutti i modi costano come il 1200** (il 3200 anche un po' meno in codifica; 700C e 450 meno in decodifica). **Il 3200 non è un problema di CPU**.

**Stima sull'ESP32.** Sulla T3 il 1200 in decodifica è stato misurato (8 novembre, [prove.md](prove.md)): **13 ms in media, 16 al massimo, per un frame da 40 ms**, cioè circa il 33% di un core. Sul PC lo stesso lavoro dura 58 µs: la scheda è circa **225 volte** più lenta. Applicando lo stesso fattore agli altri modi si ottengono, per secondo di audio, circa:

| Modo | Decodifica | Codifica (stima) |
|---|---|---|
| 3200 | circa 31% | circa 50% |
| 1200 | circa 33% (misurato) | circa 57% |
| 700C | circa 22% | circa 57% |
| 450 | circa 24% | circa 56% |

Sono stime: il fattore vale per il 1200 e potrebbe cambiare con le altre routine (700C e 450 usano la "newamp"). La codifica del 1200 sulla scheda non è mai stata cronometrata. Da **misurare sulla scheda** quando si scrive il modo Qualità.

**Cadenza a 20 ms:** il 3200 produce un frame ogni 20 ms. La catena audio lavora a blocchi da 40 ms: basta codificare **due blocchi da 160 campioni** per ogni blocco audio, senza cambiare il ritmo del ciclo principale (il ridisegno del display da solo dura circa 25 ms, più di 20).

## 3. Memoria

Memoria dinamica dopo `codec2_create`, misurata sul PC (sulla T3 il 1200 ha tolto 31,4 KB dallo heap all'avvio: il conto torna):

| Modo | Memoria |
|---|---|
| 2400, 1600, 1400, 1300, 1200 | 29,4 KB |
| 3200 | 31,4 KB (+2 KB) |
| 700C, 450 | 32,0 KB (+2,6 KB) |

Con circa 238 KB di heap libero dopo l'avvio del Codec2 non è un problema.

## 4. Tempo in aria (30 s di voce)

Con la stessa struttura di pacchetti di oggi (12 frame per pacchetto, parità 1 su 4, nominativo ogni 10 pacchetti), calcolato con la formula di Semtech come `messageAirtimeMs`:

| Modo | Standard SF8/BW250 | Messaggio SF12/BW125 | Qualità SF7/BW250 | Pacchetto dati |
|---|---|---|---|---|
| 3200 | 22,3 s (**74%**) | 580 s | **12,5 s (42%)** | 192 B |
| 2400 | 17,4 s (58%) | 455 s | 9,9 s (33%) | 144 B |
| 1200 | **10,2 s (34%)** | **271 s** | 5,7 s (19%) | 72 B |
| 700C | 7,3 s (24%) | **194 s** | 3,9 s (13%) | 42 B |
| 450 | 5,8 s (19%) | **156 s** | 3,1 s (10%) | 27 B |

- **Il 3200 sullo Standard (SF8) sfora il limite del 70%**: per questo la Qualità va su SF7/BW250, dove sta al 42% (come previsto dalla roadmap).
- **Messaggio:** il 700C farebbe risparmiare il 28% del tempo in aria rispetto al 1200 (194 s contro 271 s), il 450 il 43% (156 s). Il Messaggio da 30 s di voce resta di minuti.
- **Serve l'impacchettamento a bit.** I numeri del 700C e del 450 sono con i frame impacchettati (28 e 18 bit). A byte interi (4 e 3 byte per frame) il 700C va a 210 s e il 450 a 181 s: perderebbero circa la metà del vantaggio.

## 5. Ascolto comparativo

**Il modo più semplice:** `tools/confronta-codec.sh` guida tutto: Invio per iniziare (conto alla rovescia di 3 secondi), Invio per finire la registrazione, poi confronta con tutti i modi e mostra un menu per ascoltare l'originale, ogni modo e i tre modi compatti con il 5% di frame persi, quante volte si vuole. Le registrazioni restano in `~/confronto-codec/`. Sotto il funzionamento a mano.

Lo strumento **`codec_compare`** (`tools/codec_compare.c`) codifica e decodifica la stessa registrazione con tutti i modi e scrive un file WAV per ciascuno, con la tabella di bit al secondo, byte totali e CPU del PC.

```bash
make codec_compare                                  # serve solo un compilatore C
arecord -f S16_LE -r 8000 -c 1 -d 10 voce.wav      # 10 secondi di voce a 8 kHz (alsa-utils)
build/codec_compare voce.wav confronto             # scrive confronto/originale.wav, 3200.wav, 1200.wav, ...
build/codec_compare voce.wav confronto --modi 1200,700C,450 --perdita 5
ffplay -nodisp -autoexit confronto/1200.wav
```

- Ingresso: WAV PCM a 16 bit, mono, 8000 Hz (`--raw` per s16le grezzo). Altri formati: `ffmpeg -i in.wav -ar 8000 -ac 1 -sample_fmt s16 voce.wav`.
- `--perdita N` butta via il N% dei frame a caso, con frame a zero come fa la ricezione quando un pacchetto non si recupera: serve a sentire **come degrada** ogni modo.
- **Cosa ascoltare:** per il **Messaggio** confronta 1200, 700C e 450 (si capiscono le parole? i nominativi? i numeri?); per la **Qualità** 3200, 2400 e 1200 (quanto è più "vera" la voce?). L'audio di una scheda passa dal microfono INMP441 e dall'AGC: parla a un livello normale (picchi attorno a -6 dBFS) per un confronto equo.
- Lo strumento **non** è nei test automatici di GitHub Actions (non è codice di prodotto): si prova a mano.

## 6. Cosa resta da fare

1. **Ascoltare** i file di confronto con una voce vera e scegliere il codec del Messaggio (1200, 700C o 450) e confermare 3200 o 2400 per la Qualità.
2. **Cronometrare sulla scheda** codifica e decodifica dei modi scelti (stima: circa 50% e 31% per il 3200).
3. Scrivere l'**impacchettamento a bit** (28 e 18 bit per frame) e i **blocchi da 20 ms** del 3200 e del 2400.
