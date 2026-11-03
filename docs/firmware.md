# Firmware (LILYGO LoRa32 T3 V1.6.1)

> **Mai alimentare la scheda senza antenna a 433 MHz (o carico fittizio) collegata**, nemmeno
> solo per compilare o caricare il firmware via USB: l'USB alimenta il modulo radio, che senza
> carico si può danneggiare. Prima l'antenna, poi il cavo USB.

**Stato: scritto ma non ancora compilato né provato sulla scheda.** La logica del protocollo
(`lib/voicecore/`) è testata sul PC; gli adattatori hardware in `src/` sono stati controllati
solo contro le intestazioni reali delle librerie (RadioLib 7.5.0, esp32_codec2 1.0.7, driver I2S
di ESP-IDF 4.4.7) e con una compilazione di sola sintassi su stub. La compilazione vera con
PlatformIO non è stata possibile nell'ambiente in cui ho lavorato (registro PlatformIO bloccato).

## File

| File | Compito |
|---|---|
| `src/main.cpp` | Avvio, pulsanti PTT e volume, ciclo principale |
| `src/radio_lora.*` | SX1278 con RadioLib: parametri LoRa, CAD, invio, ricezione (`IRadio`) |
| `src/codec2_adapter.*` | Codec2 modo 1200 bps (`ICodec`) |
| `src/audio_io.*` | I2S full duplex a 16 kHz, task di cattura, pre-roll, amplificatore, toni (`IAudioIn`/`IAudioOut`) |
| `src/ui.*` | OLED SSD1306 128x64 e segnali acustici (`INotifier`) |
| `src/board_pins.h`, `src/user_config.h` | Pin della scheda e impostazioni (nominativo, frequenza, SF/BW, parità) |

## Compilazione

```
pio run -e lora32            # compila
pio run -e lora32 -t upload  # carica (togliere la microSD)
pio device monitor           # seriale a 115200
```

Dipendenze (in `platformio.ini`): RadioLib 7.5.0, esp32_codec2 1.0.7, Adafruit SSD1306 e GFX.

## Licenze (da decidere prima di distribuire)

- Il repository del progetto **non ha ancora un file di licenza**.
- La libreria `esp32_codec2` (sh123) ha nel suo repository licenza **GPL-3.0** e contiene i sorgenti di Codec2 di David Rowe (LGPL-2.1). Distribuire un firmware compilato con quella libreria comporta gli obblighi della GPL-3.0 per l'insieme.
- Per uso personale non cambia nulla; se si vuole pubblicare il firmware con un'altra licenza va scelta un'altra implementazione di Codec2 (da verificarne la licenza) oppure il progetto va rilasciato sotto GPL-3.0.
- Il codice di `esp32_loradv` (GPL-2.0) non è stato copiato.

## Prima di trasmettere

1. **Frequenza:** `LCR_FREQUENCY_MHZ` in `src/user_config.h` è un segnaposto. Scegliere il canale col band plan IARU R1 / ARI per i modi digitali sui 70 cm.
2. **Nominativo:** `LCR_CALLSIGN` deve essere il tuo (A-Z, 0-9, `/`, massimo 8 caratteri).
3. **Antenna** collegata prima di alimentare la scheda.
4. **Potenza:** `LCR_TX_POWER_DBM` parte da 10 dBm per i test.

## Primo avvio, in ordine

1. **Avvio:** l'OLED mostra "AVVIO" e poi "ASCOLTO". Se compare "ERRORE" con il nome del blocco (Codec2, audio I2S, radio) la riga della seriale dà il codice. Senza OLED il resto funziona e la seriale lo segnala.
2. **Radio:** con due schede accese, premere il PTT su una: l'altra deve passare a "RX <nominativo>" anche con il microfono muto. Controllare RSSI e SNR.
3. **Microfono:** parlare con il PTT premuto e controllare sull'altra scheda se si sente qualcosa. Se si sente solo silenzio, provare `LCR_MIC_SLOT 1` (lo slot sinistro/destro dello stereo dipende dal collegamento di `L/R`).
4. **Altoparlante:** il pulsante volume (GPIO15, opzionale) fa un bip a ogni passo. Se il volume sembra dimezzato, verificare il pin `SD` dell'amplificatore.
5. **Voce:** turno completo di qualche secondo e risposta dopo 5-10 s.
6. **Messa a punto:** guadagno e AGC del microfono (`FrontEndConfig`), AGC e volume in uscita (`BackEndConfig`), pre-roll, 8 o 16 kHz.
7. **Misure:** portata, perdite (OLED: "persi" e "recup." dopo ogni ricezione) e tempo reale in aria; poi decidere SF/BW definitivi.

## Limiti noti

- Una trasmissione LoRa blocca il ciclo principale per circa 0,15 s a pacchetto: il microfono è letto da un task a parte, quindi non si perdono campioni.
- Il display è ridisegnato al massimo 10 volte al secondo perché l'I2C costa circa 25 ms a disegno.
- Il CAD rileva solo preamboli LoRa con gli stessi parametri: altre trasmissioni (FM, FSK) non sono viste.
- Il pulsante PTT su GPIO0 e quello volume su GPIO15 sono pin di avvio: non tenerli premuti durante accensione o reset.
