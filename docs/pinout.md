# Pinout

**Da completare all'arrivo delle schede.**

## Componenti ordinati (2 novembre 2026)

| Funzione | Modello |
|---|---|
| Microfono | INMP441 (modulo rotondo a 6 pin), 3 pezzi |
| Amplificatore | MAX98357A I2S classe D, 3 pezzi |
| Altoparlante | 3 W / 8 Ω con cavetto, 4 pezzi |
| Schede | 2 × LILYGO LoRa32 433 MHz (revisione da leggere sulla serigrafia) |

## Ancora da stabilire

- revisione della LoRa32 e relativi pin radio (NSS, RST, DIO0/DIO1, SPI);
- GPIO per I2S del microfono (BCLK, WS, DIN), I2S dell'amplificatore (BCLK, WS, DOUT) e per il PTT, evitando i pin usati da radio, OLED e slot SD e GPIO0 per l'I2S;
- se SD dell'amplificatore va a un GPIO.

Dettagli di collegamento in [audio.md](audio.md).

Collegare sempre l'antenna a 433 MHz **prima** di alimentare la scheda.
