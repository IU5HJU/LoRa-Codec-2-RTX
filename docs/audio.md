# Audio: microfono INMP441 e catena di elaborazione

## Collegamento del microfono (INMP441, modulo rotondo a 6 pin)

| Pin microfono | Collegamento |
|---|---|
| VDD | 3,3 V (**non** 5 V) |
| GND | massa |
| SCK | clock I2S (BCLK) dell'ESP32 |
| WS | word select (LRCLK) |
| SD | dati verso l'ESP32 |
| L/R | **a GND**: il microfono usa il canale sinistro. Non lasciarlo scollegato. |

Cablaggio e montaggio:

- Fili corti, al massimo 10-15 cm, e un condensatore da 100 nF vicino a VDD e GND.
- Antenna e relativo cavo lontani dal microfono e dai suoi fili: a 433 MHz in trasmissione possono entrare nel segnale digitale come ronzio.
- Il foro del suono va tenuto libero e, se c'è una scatola, sigillato verso l'esterno.
- Nessun problema di effetto Larsen (rientro dell'altoparlante nel microfono): il sistema è a PTT e half-duplex, quindi mentre il microfono è attivo l'altoparlante è muto.
- Il clic del pulsante PTT, se il pulsante è sulla scheda, può finire nel microfono: vedi il pre-roll sotto.

## Catena di elaborazione (`lib/voicecore/audio_frontend.*`)

```
I2S 32 bit @ 16 kHz (24 bit utili, allineati a sinistra)
  -> float normalizzato
  -> passa-alto 300 Hz (Butterworth, 2° ordine)
  -> decimazione per 2 con FIR passa-basso (31 tappe, taglio ~3,6 kHz)
  -> AGC (target -24 dBFS RMS, guadagno da -20 a +36 dB, soglia di rumore -70 dBFS)
  -> int16 @ 8 kHz, 320 campioni per 40 ms  ->  Codec2 1200
```

Motivi delle scelte:

- **16 kHz e non 8 kHz:** il clock di bit dell'INMP441 deve stare sopra ~0,5 MHz; a 8 kHz con frame a 64 bit si è al limite. `esp32_loradv` segnala qualità scarsa a 8 kHz.
- **Guadagno digitale alto:** il microfono ha sensibilità di circa -26 dBFS a 94 dB SPL, quindi la voce a distanza di parlato arriva intorno a -60 dBFS.
- **Soglia di rumore dell'AGC:** sotto -70 dBFS il guadagno non sale, per non amplificare il fondo durante le pause.
- **Rampa del guadagno** dentro il frame, per evitare scalini udibili tra un frame e l'altro.

I valori sono ragionati e verificati con segnali sintetici sul PC (`tests/test_audio.cpp`); vanno
regolati ad orecchio con i microfoni reali (`FrontEndConfig` e `AgcConfig`).

## Microfono sempre acceso con pre-roll (`PrerollMic`)

Il task di cattura alimenta `PrerollMic::feed()` con un frame ogni 40 ms **anche a PTT rilasciato**; il
ring conserva solo gli ultimi 5 frame (200 ms, configurabile). Alla pressione del PTT quei 200 ms
vengono trasmessi per primi:

- la prima sillaba non si perde;
- non servono transitori di avvio del microfono a ogni PTT;
- il microfono resta sempre in funzione: un po' di consumo in più, da valutare con la batteria.

`PrerollMic` non ha lock: sul firmware va chiamata da un solo contesto, oppure l'adattatore deve
aggiungere un mutex tra task di cattura e loop principale.

## Da fare con i microfoni in mano

- Verificare il livello reale in ingresso e regolare `targetDbfs`, `maxGainDb` e `gateDbfs`.
- Provare il clic del PTT sulla scheda e, se disturba, scartare i primi frame dopo la pressione.
- Confrontare a orecchio 8 e 16 kHz, e AGC attivo o guadagno fisso.
