# LoRa-Codec-2-RTX

Messaggi vocali broadcast a turni (stile QSO) tra schede LilyGo a 433 MHz (LoRa32 T3 V1.6.1 e, in fase 1, T-Beam V1.2), per radioamatori.
La voce è compressa con Codec2 1200 bps e trasmessa con LoRa.

Funzionamento in breve:

1. Si preme il PTT: il microfono viene bufferizzato subito.
2. Si trasmette il nominativo, poi la voce Codec2 man mano che si parla.
3. Al rilascio del PTT (massimo 30 s) si invia l'ultimo audio e il segnale di fine turno (OVER).
4. Tutte le stazioni in ascolto sulla frequenza ricevono e riproducono. Dopo 5-10 s di attesa si risponde.

> **Progetto sperimentale.** Il collegamento radio è **provato sulle schede** (T-Beam e T3, nei due versi),
> così come il microfono e la prova di collegamento; **la voce non è ancora stata ascoltata** (amplificatore e
> altoparlante non sono ancora stati collegati). Possono trasmettere solo radioamatori con il proprio
> nominativo, sulle frequenze e con le potenze consentite. Il nominativo viaggia in chiaro e i contenuti non sono cifrati.

## Per provare il firmware

**Occorrente per ogni stazione:**

- scheda **LILYGO LoRa32 T3 V1.6.1** oppure **T-Beam V1.2 (AXP2101)**, a 433 MHz, con **antenna a 433 MHz** (SMA);
- un **pulsante PTT** esterno tra GPIO0 e massa;
- per la voce: microfono **INMP441**, amplificatore **MAX98357A** e altoparlante 8 Ω 3 W (per il solo test radio senza audio non servono);
- schema dei collegamenti in [docs/pinout.md](docs/pinout.md) (con le immagini dei collegamenti di T3 e T-Beam) e [docs/audio.md](docs/audio.md).

**Regole di sicurezza:** antenna collegata **prima** del cavo USB, mai alimentare la scheda senza antenna o carico fittizio; microSD tolta durante il caricamento.

**Passi:**

1. **Dal browser (il modo più semplice, anche senza Linux).** Apri la [pagina di installazione](https://iu5hju.github.io/LoRa-Codec-2-RTX/) con Chrome, Edge od Opera, collega la scheda con il cavo USB e premi "Installa il firmware". Poi, nel passo 2 della stessa pagina, imposta **nominativo**, frequenza, potenza e volume: restano salvati nella scheda. Il firmware pubblico ha `N0CALL` e **non trasmette** (riceve soltanto) finché non imposti il tuo nominativo.
2. **In alternativa.** Scarica i binari dall'esecuzione di GitHub Actions (artefatto `firmware-t3-v161` o `firmware-tbeam-v12`, serve essere loggati; contengono `web_factory.bin` e `ota_update.bin`) e caricali con `web_factory.bin` all'indirizzo `0x0` ([docs/firmware.md](docs/firmware.md), anche per Windows), oppure compila in locale con PlatformIO. Per un firmware con il nominativo già dentro: fai un **fork**, cambia `LCR_CALLSIGN` in `src/user_config.h` dal sito GitHub e lascia compilare Actions. Non usare i rami di prova di altri, che contengono il loro nominativo. Dalla seriale si cambia tutto con i comandi `CFG` ([docs/firmware.md](docs/firmware.md)).
3. **Frequenza.** Il valore predefinito è un **segnaposto**: scegli il canale con il band plan IARU R1 / ARI. Le stazioni che si ascoltano devono avere gli **stessi** parametri LoRa e la stessa frequenza.
4. **Prova di collegamento (senza microfono né altoparlante).** Su due schede attiva la prova (tre pressioni veloci del PTT, il pulsante nella pagina o `CFG TEST ON`) e premi il PTT: la scheda ricevente confronta una sequenza nota bit per bit, mostra l'esito sul display e risponde. Dettagli in [docs/prova-collegamento.md](docs/prova-collegamento.md).

## Per chi collabora

- **`main` è il punto comune di partenza**: è sempre compilabile (GitHub Actions deve essere verde) e ha come nominativo `N0CALL`, quindi non trasmette finché non imposti il tuo.
- **Come contribuire:** fai un fork, lavora su un ramo tuo e proponi le modifiche con una pull request. Prima di proporle esegui `make test`.
- **Prove:** annota ogni prova in [docs/prove.md](docs/prove.md) con il modello indicato. Per un problema da risolvere apri una segnalazione (Issues).
- **Decisioni e ordine dei lavori:** [docs/roadmap.md](docs/roadmap.md). Prossimi passi: audio (altoparlante, voce a turni), TOT fino a 3 minuti con i profili, pagina web servita dalla scheda via Wi-Fi.

## Licenza

Copyright (C) 2026 IU5HJU. Questo programma è software libero, distribuito sotto la
**GNU General Public License versione 3** (vedi [LICENSE](LICENSE)), senza alcuna garanzia.
La scelta deriva dalla libreria Codec2 per ESP32 usata (GPL-3.0); dettagli sulle librerie in
[docs/firmware.md](docs/firmware.md).

Protocollo e parametri: [docs/protocollo.md](docs/protocollo.md). Audio e collegamenti: [docs/audio.md](docs/audio.md). Taratura del microfono, misure e come adattarla: [docs/taratura-microfono.md](docs/taratura-microfono.md). Firmware e primo avvio: [docs/firmware.md](docs/firmware.md). Decisioni e prossimi passi: [docs/roadmap.md](docs/roadmap.md). Registro delle prove: [docs/prove.md](docs/prove.md). Prova di collegamento: [docs/prova-collegamento.md](docs/prova-collegamento.md). Alimentazione e batteria della T-Beam: [docs/alimentazione.md](docs/alimentazione.md). Schermate dell'OLED: [docs/display.md](docs/display.md). Progetti simili e scelte che ne derivano: [docs/progetti-simili.md](docs/progetti-simili.md).

## Stato

| Parte | Stato |
|---|---|
| Formato pacchetti, parità XOR, sessioni TX/RX, pausa tra i pacchetti | fatto, testato sul PC |
| Antirimbalzo del pulsante PTT (pin configurabile in `user_config.h`) | fatto, testato sul PC |
| Catena audio del microfono (passa-alto, decimazione, AGC) e pre-roll | fatta; **microfono INMP441 provato sulla T3** (livelli, soglia e guadagno dell'AGC tarati, vedi `docs/taratura-microfono.md`); audio non ancora ascoltato |
| Blocco di uscita audio (AGC, volume, limitatore, raddoppio) | fatto, testato sul PC con segnali sintetici; amplificatore e altoparlante non ancora collegati |
| Macchina a stati (PTT, CAD, TX, RX, limite 30 s) | fatta, testata sul PC e **provata sulle schede** (trasmissione nei due versi) |
| Mappa dei pin della T3 V1.6.1 e della T-Beam V1.2 con test dei conflitti | fatta (`src/board_pins*.h`, `docs/pinout.md`), con schemi dei collegamenti di entrambe |
| Supporto T-Beam V1.2, fase 1 (alimentazione AXP2101, tasto IO38 come PTT, batteria sull'OLED) | **provato sulla scheda** (avvio anche a batteria, radio, tasto, tensione batteria confrontata col tester). GPS: fase successiva |
| Impostazioni nella scheda (nominativo, frequenza, potenza, volume) e comandi `CFG` | fatti e provati; salvate in NVS |
| Pagina web di installazione e configurazione (GitHub Pages, Web Serial) | **online** e provata con Chrome su Linux |
| Prova di collegamento (frame noti, confronto bit per bit, risposta automatica) | fatta e **provata sulle schede**: funziona; vedi `docs/prova-collegamento.md` |
| Collegamento radio tra le schede | **provato nei due versi** (T-Beam <-> T3, RSSI e SNR in `docs/prove.md`); nessuna perdita a 1-2 m in prove da 30 s |
| Altoparlante, voce a turni, misure di portata | in attesa dell'hardware (amplificatore e altoparlante) |

## Struttura

```
lib/voicecore/   logica del protocollo, indipendente dalla scheda
src/             firmware per le schede supportate e impostazioni dell'operatore (user_config.h)
tests/           test sul PC con radio, codec e audio simulati
docs/            protocollo, audio, pinout (T3 V1.6.1 e T-Beam V1.2), progetti simili
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
