# LoRa-Codec-2-RTX

Messaggi vocali broadcast a turni (stile QSO) tra schede LilyGo LoRa32 a 433 MHz, per radioamatori.
La voce è compressa con Codec2 1200 bps e trasmessa con LoRa.

Funzionamento in breve:

1. Si preme il PTT: il microfono viene bufferizzato subito.
2. Si trasmette il nominativo, poi la voce Codec2 man mano che si parla.
3. Al rilascio del PTT (massimo 30 s) si invia l'ultimo audio e il segnale di fine turno (OVER).
4. Tutte le stazioni in ascolto sulla frequenza ricevono e riproducono. Dopo 5-10 s di attesa si risponde.

> **Progetto sperimentale.** Il firmware compila e la logica è testata sul PC, ma **non è ancora stato
> provato su hardware**. Possono trasmettere solo radioamatori con il proprio nominativo, sulle
> frequenze e con le potenze consentite. Il nominativo viaggia in chiaro e i contenuti non sono cifrati.

## Per provare il firmware

**Occorrente per ogni stazione:**

- scheda **LILYGO LoRa32 T3 V1.6.1** a 433 MHz con **antenna a 433 MHz** (SMA);
- un **pulsante PTT** esterno tra GPIO0 e massa;
- per la voce: microfono **INMP441**, amplificatore **MAX98357A** e altoparlante 8 Ω 3 W (per il solo test radio senza audio non servono);
- schema dei collegamenti in [docs/pinout.md](docs/pinout.md) e [docs/audio.md](docs/audio.md).

**Regole di sicurezza:** antenna collegata **prima** del cavo USB, mai alimentare la scheda senza antenna o carico fittizio; microSD tolta durante il caricamento.

**Passi:**

1. **Nominativo.** Il firmware predefinito ha `N0CALL` e **non trasmette** (riceve soltanto). Ogni stazione deve usare il proprio nominativo in `LCR_CALLSIGN` (`src/user_config.h`). Il modo più semplice: fai un **fork** del repository, modifica quella riga direttamente dal sito GitHub e lascia che GitHub Actions compili nel tuo fork. Non usare i rami di prova di altri, che contengono il loro nominativo.
2. **Scarica i binari** dall'esecuzione di GitHub Actions: artefatto `firmware-t3-v161` (serve essere loggati). Contiene `web_factory.bin` e `ota_update.bin`.
3. **Carica `web_factory.bin` all'indirizzo `0x0`** sulla scheda: istruzioni per Windows in [docs/firmware.md](docs/firmware.md). In alternativa compila in locale con PlatformIO.
4. **Frequenza.** Il valore predefinito in `src/user_config.h` è un **segnaposto**: scegli il canale con il band plan IARU R1 / ARI. Le stazioni che si ascoltano devono avere gli **stessi** parametri LoRa e la stessa frequenza.

**Licenza:** non ancora scelta (vedi [docs/firmware.md](docs/firmware.md)). Il codice è visibile ma, finché non c'è una licenza, non è libero da riusare.

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
