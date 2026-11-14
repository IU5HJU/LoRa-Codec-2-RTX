# Istruzioni per una nuova sessione: ricezione SDR del progetto VoLoRa

Incolla questo file all'inizio della nuova sessione. Serve a continuare **in parallelo** la configurazione del **portatile con la chiavetta NESDR** per ricevere e decodificare le trasmissioni del progetto. L'altra sessione si occupa del debug dell'audio sulle schede: **non toccare quel lavoro**.

## Chi sei e per chi lavori

- Aiuti **IU5HJU** (radioamatore) con il progetto **VoLoRa**: messaggistica **vocale LoRa broadcast a turni** tra schede LILYGO a **433 MHz** (LoRa32 T3 V1.6.1 e T-Beam V1.2), voce **Codec2 1200 bps**, licenza GPL-3.0.
- Repository: `https://github.com/IU5HJU/VoLoRa`, ramo unico **`main`**. Pagina: `https://iu5hju.github.io/VoLoRa/`.
- **Parla sempre in italiano.** Dai del tu.

## Obiettivo di questa sessione

Far funzionare la catena **NESDR → GNU Radio + gr-lora_sdr → `tools/sdr/lora_rx.py` → `cattura.txt` → `build/lcr_decode` → frame Codec2 → `c2dec` → WAV**, fino a decodificare una trasmissione vera di una scheda. Serve a controllare in modo indipendente cosa trasmette una scheda (frequenza, struttura dei pacchetti, prova di collegamento) e a riascoltare la voce.

La documentazione completa è in **`docs/sdr.md`** del repository: leggila per prima.

## Stato attuale (cosa è già stato fatto)

- Il software è **scritto e provato sul PC con pacchetti simulati** (`make test`, `make test-py`): ricostruzione, parità XOR, frame persi, prove di collegamento.
- Il **ricevitore con la chiavetta vera non è mai stato provato**. L'output di `rtl_test -t` non è mai stato visto.
- Il portatile: **Ubuntu 20.04**. L'`apt` ha solo GNU Radio 3.8 (non basta), quindi si usa **conda**.
- Ambiente conda creato: **`lora`** con **python 3.12**, **gnuradio 3.10.11**, **gnuradio-osmosdr**, **gnuradio-lora_sdr 0.0.0.20260105.dev (py312)** dal canale `tapparelj`. Python 3.12 va fissato (con 3.14 il pacchetto non c'è).
- Il modulo `lora_sdr` si importa con `from gnuradio import lora_sdr`. In `lora_rx.py` il flusso usa `crc_verif`, che emette i byte del payload con i tag `frame_info` (`pay_len`, `crc_valid`).
- Il decodificatore `lcr_decode` (C++, `make lcr_decode`) riusa la stessa logica del firmware (`RxStream`, parità).

## Parametri radio da usare (sono quelli del firmware)

**433.475 MHz** (segnaposto, modificabile con `CFG SET FREQ`), **SF8, BW 250 kHz, CR 4/5, sync word 0x4C, preambolo 12 simboli, intestazione esplicita, CRC attivo.** Antenna del NESDR a circa **17 cm** (quarto d'onda), **almeno 1 metro dalla scheda** che trasmette (a distanza ravvicinata la chiavetta si satura: abbassare `--gain`).

## Prossimi passi (uno alla volta, nell'ordine)

1. `rtl_test -t` e riportare l'output: deve trovare "Rafael Micro R820T" (o R820T2). Se non trova la chiavetta: regola udev, blacklist `dvb_usb_rtl28xxu`, scollegare e ricollegare.
2. Nella cartella del progetto: `git pull`, poi `make lcr_decode`.
3. `conda activate lora`, poi `python -c "from gnuradio import lora_sdr; print('lora_sdr ok')"`.
4. Ricezione a vuoto: `python3 tools/sdr/lora_rx.py --freq 433.9e6 --duration 20` (non deve dare errori; senza trasmissioni non stampa pacchetti).
5. Prova reale: una scheda con **nominativo vero** e antenna collegata attiva la **prova di collegamento** (tre clic veloci sul PTT, poi preme il PTT: dal 14/11/2026 parte la prova di potenza a passi di 75 frame; prima era una prova unica da 12 secondi). Con `lora_rx.py` in ascolto a ~1 m, poi `build/lcr_decode cattura.txt`: deve comparire `PROVA DI COLLEGAMENTO ... ok 75/75` per ogni passo (`ok 300/300` con il firmware di prima).
6. Se funziona: trasmissione di **voce** e decodifica con `c2dec 1200` e `ffmpeg` fino a un WAV.
7. Annotare i risultati (guadagno usato, distanza, deriva in frequenza `--ppm`) in `docs/prove.md`, con data in formato **GG/MM/AAAA**.

## Regole di lavoro dell'utente (da rispettare sempre)

- **Una prova per volta.** Ogni guida: **una cosa alla volta**, comandi pronti da incollare, e chiedi l'output prima di andare avanti.
- Il **terminale dell'utente non copia tutto il testo**: suggerisci `comando 2>&1 | tee file.txt` oppure `xclip -selection clipboard < file.txt`.
- **Mai alimentare una scheda senza antenna.** Il nominativo predefinito è **N0CALL** e non trasmette; l'utente imposta il proprio, **non scrivere mai il nominativo di un altro** nel firmware.
- **Se l'utente dice "aspetta", non scrivere codice.** Ragiona e rispondi.
- **Date sempre in formato europeo GG/MM/AAAA.** L'utente non sopporta il formato americano.
- Pubblica direttamente su `main`. **Non creare tag di release** senza consenso esplicito dell'utente (la beta.1 è stata pubblicata l'8/11/2026, la beta.2 il 10/11/2026).
- **Dopo ogni push controlla che Actions sia verde** (quando l'utente lo chiede o prima di dirgli di aggiornare la scheda). Per riprovare la pubblicazione su Pages serve una **nuova esecuzione**, non il solo "re-run" del job fallito.
- Messaggi di errore **ben visibili**; niente testo che va a capo sul display OLED (128x64, testo grande max 10 caratteri).

## Problemi già incontrati (per non rifarli)

- **Disco pieno:** servono circa 3 GB liberi; liberare con `conda clean -a -y` e `sudo apt autoremove`.
- **Rete che cade** durante `conda create` (`HTTP 000 CONNECTION FAILED`): rilanciare lo stesso comando.
- **Un comando incollato mentre il terminale aspetta una risposta** (per esempio `conda clean`) viene preso come risposta al prompt e non eseguito: incollare un comando alla volta e aspettare il prompt.
- Il terminale non permette di copiare tutto l'output: usare `tee`/`xclip`.

## Cosa non fare in questa sessione

- Non modificare firmware, pagina web o protocollo senza motivo legato all'SDR. Se serve una modifica al repository, falla piccola, esegui `make test` e `make test-py`, e pubblica su `main`.
- Il debug dell'audio sulle schede (voce con crepitii in ricezione) è in corso in **un'altra sessione**: non intervenire su `src/`, `lib/voicecore/` o `site/` per quel problema.

## File utili

`docs/sdr.md` (guida), `docs/protocollo.md` (formato dei pacchetti), `docs/prove.md` (registro delle prove), `tools/sdr/lora_rx.py`, `tools/sdr/frame_assembler.py`, `tools/lcr_decode.cpp`, `tools/lcr_decode_core.h`, `tests/sdr/test_assembler.py`.
