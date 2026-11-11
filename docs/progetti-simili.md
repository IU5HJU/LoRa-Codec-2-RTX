# Progetti simili e scelte che ne derivano

Ricerca su GitHub del 2 novembre 2026, fatta con la ricerca per repository. Di questi progetti
ho letto nome e descrizione; solo per `esp32_loradv` ho letto anche il codice (README,
`default_config.h`, `audio_task.cpp`, `radio_task.cpp`, wrapper Codec2). Le cifre di
occupazione del canale sono stime con la formula Semtech, da verificare sul campo.

## Progetti trovati

| Progetto | Note |
|---|---|
| [sh123/esp32_loradv](https://github.com/sh123/esp32_loradv) | Walkie-talkie ESP32 con Codec2/OPUS su LoRa o FSK, il più vicino al nostro. GPL-2.0. |
| [sh123/esp32_loraprs](https://github.com/sh123/esp32_loraprs) | Modem LoRa APRS con modalità DV Codec2. Stesso autore. |
| [sh123/codec2_talkie](https://github.com/sh123/codec2_talkie) | App Android, ricetrasmettitore digitale Codec2/OPUS con modem esterno. |
| [dudmuck/lora_codec2](https://github.com/dudmuck/lora_codec2) | Voce su SX1262 con scheda STM32. |
| Atul1037y/LoRa_Voice_Comm, codeWithMaaz-05/LoRaTalk-WalkieTalkie | ESP32 + SX1262 + Codec2, progetti recenti. |
| GlomarGadaffi/fsk-walkie-talkie | Voce su FSK 19,2 kbps con Codec2 3200, ESP32-S3 + SX1262. |
| Meshtastic (`src/modules/esp32/AudioModule.cpp`) | Modulo audio con Codec2 nel firmware ufficiale. |

## Confronto con esp32_loradv

| Aspetto | esp32_loradv | VoLoRa |
|---|---|---|
| Uso | Streaming continuo "come una radio FM" | Turni stile QSO |
| Protocollo | Nessuno, frame Codec2 nudi | Header con versione, sessione, sequenza, flag |
| Nominativo | Assente (solo tra le idee future) | Inizio, fine, ogni ~5 s |
| Fine turno | Nessun marcatore | Flag END |
| Perdite | Nessuna protezione | Parità XOR 1 su 4 |
| Canale libero | Nessun controllo trovato nel codice | Controllo prima del PTT |
| LoRa | SF7, BW 41,7 kHz, CR 4/5, sync 0x12, CRC | SF8, BW 250 kHz, CR 4/5, sync 0x4C |
| Pacchetti | 48 byte = 8 frame = 320 ms | 12 frame = 480 ms + header |

Occupazione del canale (stima):

| Configurazione | Aria per pacchetto | Voce per pacchetto | Occupazione |
|---|---|---|---|
| esp32_loradv, 48 B, SF7/BW41,7 | ~292 ms | 320 ms | ~91 % |
| Nostro, SF8/BW250, senza parità | ~123 ms | 480 ms | ~26 % |
| Nostro, SF8/BW250, con parità | ~154 ms | 480 ms | ~32 % |
| Nostro, SF8/BW125, senza parità | ~246 ms | 480 ms | ~51 % |

## Idee da riprendere (da implementare da zero)

`esp32_loradv` è GPL-2.0: non si copia il suo codice, si riprendono solo idee e parametri.

1. **Audio e radio in task separati con ring buffer.** L'invio radio è bloccante (~0,15 s per
   pacchetto): il microfono va letto in un task a parte, altrimenti si perdono campioni.
   `IAudioIn::readFrame()` è già pensato per essere alimentato da un ring buffer.
2. **Microfono I2S a 16 kHz con downsampling a 8 kHz** (loro segnalano scarsa qualità a 8 kHz): da provare.
3. **Filtro passa-alto ~300 Hz** prima del codec e **controllo automatico del guadagno** in riproduzione.
4. **Sleep leggero in ricezione** per la batteria.
5. **FSK come alternativa** se serve più banda.
6. **Frequenze di RX e TX separate**, per un'eventuale modalità ripetitore.

## Differenze volute

- **Nessuna cifratura:** il "privacy mode" ChaCha20 di `esp32_loradv` è pensato per la banda ISM.
  In banda radioamatoriale i contenuti non si cifrano.
- **Frequenza:** 433,775 MHz è il default di quel progetto, ma in quel punto operano di norma altre
  reti LoRa/APRS. La nostra frequenza va scelta col band plan IARU R1 / ARI.

## Decisioni aperte (con le schede in mano)

- Larghezza di banda LoRa: SF8/BW250 o più stretta, in base alla portata misurata.
- Frequenza di campionamento del microfono: 8 o 16 kHz.
- Filtro passa-alto e AGC: da regolare ad orecchio.
