# Pinout: LILYGO LoRa32 T3 V1.6.1

Scheda confermata dalla foto: serigrafia **T3_V1.6.1**, modulo LoRa 433/470 MHz, **ESP32-PICO-D4**
(4 MB di flash, senza PSRAM), USB micro con CH9102, connettore batteria LiPo, SMA più connettore U.FL.
Mappa in codice: [`src/board_pins.h`](../src/board_pins.h), controllata da `tests/test_pins.cpp`.

## Componenti (ordinati il 2 novembre 2026)

| Funzione | Modello |
|---|---|
| Microfono | INMP441 (modulo rotondo a 6 pin), 3 pezzi |
| Amplificatore | MAX98357A I2S classe D, 3 pezzi |
| Altoparlante | 3 W / 8 Ω con cavetto, 4 pezzi |
| Schede | 2 × LILYGO LoRa32 T3 V1.6.1, 433 MHz |

## Pin già usati dalla scheda

Fonte: documentazione e `utilities.h` del repository ufficiale Xinyuan-LilyGO/LilyGo-LoRa-Series.

| Funzione | GPIO |
|---|---|
| Radio SX1278: SCK / MISO / MOSI / CS | 5 / 19 / 27 / 18 |
| Radio: RST / DIO0 / DIO1 / DIO2 | 23 / 26 / 33 / 32 |
| OLED SSD1306: SDA / SCL | 21 / 22 |
| microSD: CS / MOSI / MISO / SCK | 13 / 15 / 2 / 14 |
| LED utente | 25 |
| ADC batteria | 35 |

La scheda non ha un pulsante utente: c'è solo **RST**.

## Assegnazione per il progetto

| Segnale | GPIO | Collegamento |
|---|---|---|
| I2S BCLK (condiviso) | 14 | INMP441 `SCK` e MAX98357A `BCLK` |
| I2S WS (condiviso) | 13 | INMP441 `WS` e MAX98357A `LRC` |
| Dati microfono | 34 | INMP441 `SD` (GPIO34 è solo ingresso, va bene) |
| Dati altoparlante | 4 | MAX98357A `DIN` |
| Shutdown amplificatore | 25 | MAX98357A `SD` (alto = acceso); è anche il LED di bordo |
| PTT | 0 | Pulsante esterno tra GPIO0 e GND (pull-up interno) |

Collegamenti fissi:

- INMP441: `VDD` a 3,3 V, `GND` a massa, `L/R` a **GND**.
- MAX98357A: `GND` a massa, `VIN` a 5 V (USB) o, a batteria, a 3,3 V; `GAIN` libero.

Perché così:

- **Microfono e amplificatore condividono BCLK e WS:** con una sola porta I2S in full duplex servono 4 pin invece di 6. Entrambi lavorano a **16 kHz** con slot da 32 bit, quindi l'uscita va impostata con `upsample2x = true`. Il microfono resta sempre acceso (pre-roll) anche mentre l'altoparlante suona, quindi serve il full duplex.
- **GPIO13 e GPIO14 sono i pin dello slot microSD:** funzionano solo senza scheda inserita. La documentazione LilyGO avverte inoltre di togliere la microSD quando si carica il firmware.
- **GPIO0 per il PTT:** è anche il pin di boot, quindi il pulsante esterno funziona da PTT e permette di forzare la modalità flash. Non tenerlo premuto durante accensione o reset.
- **GPIO12, 1, 3, 6-11 evitati:** strapping, UART di debug e flash.
- Restano liberi: GPIO15 e GPIO2 (strapping, ma utilizzabili), GPIO35 (ADC batteria), GPIO22/21 se l'OLED non serve.

## Alimentazione

- Il pin `5V` del connettore dà i 5 V solo con USB collegato. Con batteria l'amplificatore va a 3,3 V (circa 0,7 W su 8 Ω); il 3,3 V è condiviso con ESP32 e radio, quindi un condensatore da circa 100 µF vicino all'amplificatore è utile.
- Questa versione non ha un circuito di protezione della batteria: usare una LiPo con protezione.
- Il LED `CHG` indica la carica.

## Antenna

Collegare sempre l'antenna a 433 MHz **prima** di alimentare o trasmettere: senza carico il modulo radio si può danneggiare. Nella foto l'antenna è collegata al connettore SMA.

## Da verificare con le schede in mano

- Che GPIO13/14 siano davvero liberi (microSD non inserita) e che il cablaggio non carichi i segnali.
- Se la scheda ha l'OLED montato: nella foto non è visibile. Non è indispensabile, il feedback può essere acustico (toni dall'altoparlante).
- Il livello I2S dell'amplificatore (possibile dimezzamento con la media dei canali).
