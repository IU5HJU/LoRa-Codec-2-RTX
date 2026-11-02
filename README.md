# LoRa-Codec-2-RTX

Messaggi vocali broadcast a turni (stile QSO) tra schede LilyGo LoRa32 a 433 MHz, per radioamatori.
La voce è compressa con Codec2 1200 bps e trasmessa con LoRa.

Funzionamento in breve:

1. Si preme il PTT: il microfono viene bufferizzato subito.
2. Si trasmette il nominativo, poi la voce Codec2 man mano che si parla.
3. Al rilascio del PTT (massimo 30 s) si invia l'ultimo audio e il segnale di fine turno (OVER).
4. Tutte le stazioni in ascolto sulla frequenza ricevono e riproducono. Dopo 5-10 s di attesa si risponde.

Protocollo e parametri: [docs/protocollo.md](docs/protocollo.md).

## Stato

| Parte | Stato |
|---|---|
| Formato pacchetti, parità XOR, sessioni TX/RX | fatto, testato sul PC |
| Macchina a stati (PTT, CAD, TX, RX, limite 30 s) | fatta, testata sul PC con radio e audio simulati |
| Adattatori hardware (RadioLib, Codec2, I2S, OLED, PTT) | da fare, servono le schede |
| Test radio reale, misura di portata e airtime | da fare |

## Struttura

```
lib/voicecore/   logica del protocollo, indipendente dalla scheda
src/             firmware (stub) e impostazioni dell'operatore (user_config.h)
tests/           test sul PC con radio, codec e audio simulati
docs/            protocollo e pinout
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
