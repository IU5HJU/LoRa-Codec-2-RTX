# Display OLED

![Schermate dell'OLED](images/display.png)

Il display è l'SSD1306 128×64 della T3 V1.6.1. L'immagine **non è una foto**: è un mockup generato da
`tools/render_display.py` con lo stesso font 5×7 di Adafruit_GFX, le stesse coordinate e lo stesso ritorno a capo
automatico del codice in `src/ui.cpp`. Sul display reale i colori dipendono dal pannello (di solito bianco o azzurro).

## Struttura di una schermata

```
IU5HJU  433.900 MHz          riga 1: nominativo e frequenza (testo piccolo)
------------------------     linea di separazione
ASCOLTO                      riga 2: stato (testo grande)
Premi PTT per parlare        riga 3: informazioni o avvisi (testo piccolo)
Vol 7/10                     ultima riga: volume
```

## Schermate

| # | Quando compare | Cosa mostra |
|---|---|---|
| 1 | Avvio | "AVVIO" |
| 2 | A riposo, dopo l'avvio | "ASCOLTO", frequenza, "Premi PTT per parlare" |
| 3 | Un'altra stazione trasmette | "RX" e nominativo, **RSSI** (dBm) e **SNR** (dB) dell'ultimo pacchetto |
| 4 | Dopo una ricezione | "Ultimo" e nominativo, frame **persi** e pacchetti **recuperati** dalla parità |
| 5 | PTT premuto | "TX" e nominativo, secondi trascorsi sul massimo ("Trasmissione 12/30 s") |
| 6 | TOT scaduto | Avviso "LIMITE 30 s" |
| 7 | PTT premuto durante una ricezione | Avviso "STAI RICEVENDO" |
| 8 | PTT con canale occupato | Avviso "CANALE OCCUPATO" |
| 9 | Dopo una ricezione con perdite | Come la 4, con "persi" diversi da zero |
| 10 | Avvio con N0CALL | "SOLO RX" e "Imposta il nominativo" |
| 11 | PTT con N0CALL | Avviso "IMPOSTA NOMINATIVO" |
| 12 | Errore all'avvio | "ERRORE" e il nome del blocco: `radio`, `audio I2S`, `Codec2`, `nominativo non valido` |

Gli avvisi 6, 7, 8 e 11 restano a schermo circa 1,5-2 secondi. Ogni trasmissione e ricezione termina anche con un bip.

## Note

- Il "30" di "Trasmissione 12/30 s" e di "LIMITE 30 s" oggi è fisso. Quando il TOT diventerà un'impostazione
  (vedi [roadmap](roadmap.md)) seguirà il valore scelto.
- Se l'OLED non risponde il sistema funziona comunque, con bip e riga seriale.
- Per rigenerare l'immagine dopo una modifica a `src/ui.cpp`: aggiorna le funzioni di `tools/render_display.py` e lancia
  `python3 tools/render_display.py PERCORSO/glcdfont.c docs/images/display.png`. Il file `glcdfont.c` è nel
  repository `adafruit/Adafruit-GFX-Library`. Serve Pillow.
