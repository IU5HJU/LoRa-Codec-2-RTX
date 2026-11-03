# LoRa-Codec-2-RTX

Messaggi vocali broadcast a turni (stile QSO) tra schede LilyGo LoRa32 a 433 MHz, per radioamatori.
La voce è compressa con Codec2 1200 bps e trasmessa con LoRa.

Funzionamento in breve:

1. Si preme il PTT: il microfono viene bufferizzato subito.
2. Si trasmette il nominativo, poi la voce Codec2 man mano che si parla.
3. Al rilascio del PTT (massimo 30 s) si invia l'ultimo audio e il segnale di fine turno (OVER).
4. Tutte le stazioni in ascolto sulla frequenza ricevono e riproducono. Dopo 5-10 s di attesa si risponde.

Protocollo e parametri: [docs/protocollo.md](docs/protocollo.md). Audio e collegamenti: [docs/audio.md](docs/audio.md). Firmware e primo avvio: [docs/firmware.md](docs/firmware.md). Decisioni e prossimi passi: [docs/roadmap.md](docs/roadmap.md). Progetti simili e scelte che ne derivano: [docs/progetti-simili.md](docs/progetti-simili.md).

## Stato

| Parte | Stato |
|---|---|
| Formato pacchetti, parità XOR, sessioni TX/RX | fatto, testato sul PC |
| Antirimbalzo del pulsante PTT (pin configurabile in `user_config.h`) | fatto, testato sul PC |
| Catena audio del microfono (passa-alto, decimazione, AGC) e pre-roll | fatta, testata sul PC con segnali sintetici |
| Blocco di uscita audio (AGC, volume, limitatore, raddoppio) | fatto, testato sul PC con segnali sintetici |
| Macchina a stati (PTT, CAD, TX, RX, limite 30 s) | fatta, testata sul PC con radio e audio simulati |
| Mappa dei pin della T3 V1.6.1 con test dei conflitti | fatta (`src/board_pins.h`, `docs/pinout.md`) |
| Adattatori hardware (RadioLib, Codec2, I2S, OLED, PTT, volume) | scritti, **compilano su GitHub Actions**, non ancora provati sulla scheda (`docs/firmware.md`) |
| Test radio reale, misura di portata e airtime | da fare |

## Struttura

```
lib/voicecore/   logica del protocollo, indipendente dalla scheda
src/             firmware per la T3 V1.6.1 e impostazioni dell'operatore (user_config.h)
tests/           test sul PC con radio, codec e audio simulati
docs/            protocollo, audio, pinout (T3 V1.6.1), progetti simili
```

## Test

```
make test
```

Serve solo un compilatore C++17 (g++ o clang++).

## Note per l'uso in aria

- Il nominativo è trasmesso in chiaro e i contenuti non sono cifrati.
- La frequenza in `src/user_config.h` è un segnaposto: scegliere il canale secondo il band plan
  IARU Regione 1 / ARI per i modi digitali sui 70 cm.
- Collegare l'antenna prima di accendere la scheda.
