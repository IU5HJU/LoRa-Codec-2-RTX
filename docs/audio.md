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
  -> AGC (target -24 dBFS RMS, guadagno da -20 a +30 dB, soglia di rumore -54 dBFS, a riposo 12 dB)
  -> int16 @ 8 kHz, 320 campioni per 40 ms  ->  Codec2 1200
```

Motivi delle scelte:

- **16 kHz e non 8 kHz:** il clock di bit dell'INMP441 deve stare sopra ~0,5 MHz; a 8 kHz con frame a 64 bit si è al limite. `esp32_loradv` segnala qualità scarsa a 8 kHz.
- **Guadagno digitale alto:** il microfono ha sensibilità di circa -26 dBFS a 94 dB SPL, quindi la voce a distanza di parlato arriva intorno a -60 dBFS.
- **Soglia di rumore dell'AGC:** sotto la soglia il guadagno non sale e scende piano verso 12 dB, per non amplificare il fondo durante le pause. I valori della prima versione (36 dB e -70 dBFS) erano sbagliati con la voce vicina; misure e correzione in [taratura-microfono.md](taratura-microfono.md).
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

Fatto il 5 novembre per i livelli: vedi [taratura-microfono.md](taratura-microfono.md). Resta da fare l'ascolto.

- Verificare il livello reale in ingresso e regolare `targetDbfs`, `maxGainDb` e `gateDbfs`.
- Provare il clic del PTT sulla scheda e, se disturba, scartare i primi frame dopo la pressione.
- Confrontare a orecchio 8 e 16 kHz, e AGC attivo o guadagno fisso.

## Amplificatore MAX98357A e altoparlante 3 W 8 Ω

Componenti ordinati: modulo I2S MAX98357A (classe D, 3 pezzi) e altoparlanti da 3 W / 8 Ω con cavetto.
Le note sui pin vengono dalla scheda tecnica e dal modulo standard; vanno confermate sul tuo modulo
quando arriva, perché i cloni possono differire.

| Pin modulo | Collegamento |
|---|---|
| VIN | 5 V se disponibile (più volume), altrimenti 3,3 V. Con USB il pin 5 V della scheda è comodo. |
| GND | massa in comune |
| DIN | dati I2S in uscita dall'ESP32 |
| BCLK | clock di bit I2S |
| LRC | word select I2S |
| GAIN | libero = 9 dB. A GND 12 dB, a VIN 6 dB; con resistenza da 100 kΩ a GND 15 dB, a VIN 3 dB |
| SD | libero va bene sulla maggior parte dei moduli (uscita = media dei due canali). A GND l'amplificatore è spento. |

Note:

- **Potenza:** a 5 V un 8 Ω dà circa 1,5-2 W, a 3,3 V circa 0,7 W. Per una voce parlata a distanza ravvicinata basta, ma conviene limitare il volume nel software per non superare i 3 W nominali dell'altoparlante.
- **Alimentazione:** i picchi di corrente della classe D possono disturbare microfono e radio se condividono lo stesso 3,3 V. Meglio alimentare l'amplificatore da 5 V o dalla batteria, con un condensatore elettrolitico da circa 100 µF vicino a VIN/GND.
- **Cablaggio:** i fili dell'altoparlante e dell'amplificatore lontani da microfono, antenna e fili I2S del microfono.
- **Spegnimento:** se SD è collegato a un GPIO si può spegnere l'amplificatore quando non si ascolta, per eliminare il fruscio e risparmiare corrente. Durante la trasmissione serve comunque spento, perché l'altoparlante deve tacere (half-duplex).
- **Contenitore:** l'altoparlante rende meglio chiuso dietro in un piccolo volume sigillato; per la voce (300-3400 Hz) non servono bassi.
- **Livello I2S:** se l'ESP32 invia solo il canale sinistro e l'amplificatore fa la media dei canali, il livello può risultare dimezzato: da provare e, se serve, inviare lo stesso campione su entrambi i canali.
- **Frequenza di campionamento:** il MAX98357A accetta 8 kHz, quindi si può riprodurre direttamente l'uscita di Codec2; in alternativa raddoppiare a 16 kHz.

## Blocco di uscita (`lib/voicecore/audio_backend.*`)

```
frame Codec2 decodificato (int16 @ 8 kHz, 320 campioni)
  -> AGC in ricezione (target -24 dBFS RMS, guadagno da -20 a +24 dB, soglia -60 dBFS)
  -> volume (0 = muto, 1..10 in passi da 3 dB, 10 = 0 dB; mai oltre 0 dB)
  -> limitatore morbido (da 0,6 del fondo scala, picco massimo 0,9)
  -> opzionale: raddoppio a 16 kHz con FIR (640 campioni)
  -> I2S verso il MAX98357A
```

- **AGC:** stazioni con livelli molto diversi arrivano allo stesso livello. Sotto -60 dBFS, cioè nelle pause, non amplifica il fondo.
- **Volume:** `setVolumeStep()` per i comandi dell'utente (muto, +/-), `setVolumeDb()` per un valore diretto.
- **Limitatore:** protegge amplificatore e altoparlante da picchi forti. Fino a 0,6 del fondo scala il segnale non viene toccato.
- **Frame perso:** `processLost()` scrive silenzio senza alterare AGC e filtri; in futuro si può sostituire con una mascheratura di Codec2.
- **Frequenza di uscita:** a 8 kHz il MAX98357A funziona; a 16 kHz (`upsample2x`) l'immagine spettrale è attenuata di oltre 25 dB. Da confrontare a orecchio.

I valori sono verificati con segnali sintetici sul PC (`tests/test_output.cpp`) e vanno regolati con altoparlante e amplificatore reali.
Il consumo di stack del raddoppio (`Interpolator2`) è circa 3 kB.

## Volume e guadagno (10/11/2026)

Il volume ha ancora i passi da 1 a 10 (0 = muto, passi da 3 dB), ma la scala è stata spostata: il **passo 5 è 0 dB** (il vecchio passo 10) e i **passi 6-10 aggiungono guadagno digitale fino a +15 dB**, perché in uso reale il volume stava sempre al massimo. L'AGC porta il parlato a -24 dBFS, poi il volume lo moltiplica; il **limitatore morbido** (soglia 0,6, picco massimo 0,9 del fondo scala) comprime i picchi, quindi a +15 dB la voce è molto più forte ma più compressa e, sui picchi, può distorcere. I bip e i toni di prova non salgono oltre il livello a 0 dB. Il volume iniziale predefinito è il passo 5; quelli salvati con la scala vecchia vengono convertiti.

**Se serve ancora più volume, una modifica hardware semplice:** il pin `GAIN` del MAX98357A, oggi libero (9 dB), si può collegare a `GND` per **12 dB (+3 dB)** oppure a `GND` con una resistenza da 100 kΩ per **15 dB (+6 dB)**. Non conviene farla prima di aver provato il guadagno software: con l'alimentazione a 3,3 V (a batteria) l'amplificatore ha meno potenza che a 5 V (USB) e il guadagno in più si limita a saturare.
