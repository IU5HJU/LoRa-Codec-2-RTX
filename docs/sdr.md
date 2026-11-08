# Ricevere e decodificare con un SDR

Con una chiavetta SDR (NESDR, RTL-SDR) e GNU Radio si può **ascoltare e decodificare le trasmissioni** di LoRa-Codec-2-RTX senza una scheda ricevente: il segnale è un normale LoRa e i pacchetti non sono cifrati
([protocollo.md](protocollo.md)). Serve a controllare in modo indipendente cosa trasmette una scheda (frequenza, struttura dei pacchetti, prova di collegamento) e a **riascoltare la voce**.

```
NESDR -> GNU Radio + gr-lora_sdr -> pacchetti in esadecimale -> lcr_decode -> frame Codec2 -> c2dec -> WAV
          tools/sdr/lora_rx.py        cattura.txt               build/lcr_decode    .c2         .raw / .wav
```

> **Stato:** scritto e provato **sul PC con pacchetti simulati** (ricostruzione, parità, frame persi, prove di collegamento: `make test`).
> Il ricevitore `tools/sdr/lora_rx.py` e la demodulazione LoRa con una chiavetta vera **non sono ancora stati provati**: annota i risultati in [prove.md](prove.md).

## 1. Installazione (Ubuntu, anche la 20.04)

GNU Radio 3.10 e `gr-lora_sdr` si installano con **conda**, che funziona su qualunque versione di Ubuntu (su Ubuntu 20.04 `apt` ha solo GNU Radio 3.8, che non basta). Servono circa 3 GB liberi.

```bash
# driver e prova della chiavetta (sistema)
sudo apt install -y rtl-sdr
echo 'blacklist dvb_usb_rtl28xxu' | sudo tee /etc/modprobe.d/blacklist-rtl.conf
# scollega e ricollega la chiavetta, poi:
rtl_test -t        # deve trovare "Rafael Micro R820T" (o R820T2)

# conda (Miniconda), poi un ambiente con GNU Radio 3.10, il supporto RTL-SDR e il modulo LoRa
cd ~ && wget https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh && bash Miniconda3-latest-Linux-x86_64.sh
# nuovo terminale, poi:
conda create -y -n lora -c tapparelj -c conda-forge python=3.12 gnuradio gnuradio-osmosdr gnuradio-lora_sdr
conda activate lora
python -c "from gnuradio import lora_sdr; print('lora_sdr ok')"
conda clean -a -y

# decodificatore e codec (nella cartella del progetto)
make lcr_decode
sudo apt install -y codec2 ffmpeg     # c2dec per decodificare l'audio
```

Note: python 3.12 va fissato (con 3.14 il pacchetto `gnuradio-lora_sdr` non c'e'); se il disco e' quasi pieno liberalo prima (`conda clean -a`, `sudo apt autoremove`); se la rete cade (`HTTP 000 CONNECTION FAILED`) rilancia lo stesso comando.

## 2. Ricezione

Antenna del NESDR a circa 17 cm (quarto d'onda a 434 MHz) e **almeno un metro dalla scheda** che trasmette (a distanza ravvicinata la chiavetta si satura: abbassa `--gain`).

```bash
conda activate lora
python3 tools/sdr/lora_rx.py --freq 433.9e6 --out cattura.txt
```

**Se la chiavetta non viene vista** solo dentro conda (`No supported devices found`, mentre `/usr/bin/rtl_test -t` la trova): su Ubuntu 20.04 la `libudev` di conda non elenca i dispositivi USB. Avvia `lora_rx.py` con `LD_PRELOAD="/lib/x86_64-linux-gnu/libudev.so.1 /usr/lib/x86_64-linux-gnu/libusb-1.0.so.0 /usr/lib/x86_64-linux-gnu/librtlsdr.so.0"` davanti al comando.

I parametri predefiniti sono quelli del firmware: **SF8, BW 250 kHz, CR 4/5, sync word 0x4C, preambolo 12, intestazione esplicita, CRC attivo**. Se hai cambiato la frequenza con `CFG SET FREQ`, passa la stessa a `--freq`.
Per ogni pacchetto stampa una riga come `[3] CRC ok  voce Codec2 1200, sessione 1234, seq 1, flag -, payload 72 byte` e accoda a `cattura.txt` una riga
`t=<secondi> crc=<0|1> len=<n> hex=<byte>`. `--help` elenca tutte le opzioni (`--gain`, `--ppm`, `--duration`, `--device`).

## 3. Decodifica

```bash
build/lcr_decode cattura.txt --out sessioni
```

Riassembla i pacchetti come farebbe la scheda ricevente (stessa logica del firmware, **parità XOR compresa**) e stampa, per ogni trasmissione:

- **voce:** nominativo, pacchetti, frame, secondi, frame persi e pacchetti recuperati; scrive `sessione_<id>.c2`. Poi:
  ```bash
  cp sessioni/sessione_1234.c2 sessioni/sessione_1234.bin   # c2dec 0.9.2 non decodifica tutto un .c2 senza intestazione
  c2dec 1200 sessioni/sessione_1234.bin sessioni/sessione_1234.raw
  ffmpeg -f s16le -ar 8000 -ac 1 -i sessioni/sessione_1234.raw sessioni/sessione_1234.wav
  ```
- **prova di collegamento:** l'esito bit per bit (frame ok, persi, danneggiati), come lo mostra la scheda ricevente, e le eventuali risposte;
- i pacchetti con CRC errato si scartano (`--accept-bad-crc` li include).

Il decodificatore accetta anche le righe che stampa `gr-lora_sdr` con `print_rx_msg` in esadecimale, o un file con un solo pacchetto per riga.

## 4. Controllo di una scheda

1. Sulla scheda attiva la prova di collegamento (tre clic sul PTT) e tieni premuto il PTT per i 12 secondi.
2. Con `lora_rx.py` in ascolto, a fine prova ferma con `Ctrl+C` e lancia `build/lcr_decode cattura.txt`: deve comparire `PROVA DI COLLEGAMENTO ... ok 300/300 (100%)`.
3. Se la prova e' perfetta per la scheda ricevente ma l'SDR perde pacchetti, il problema e' dell'SDR (sensibilita', guadagno, deriva in frequenza), non del collegamento.

## Se qualcosa non va

| Sintomo | Cosa provare |
|---|---|
| nessun pacchetto | frequenza identica a quella della scheda; antenna collegata; sync word 0x4C; avvicina la scheda (1-2 m) e prova `--gain 20`/`40`; `--ppm` con il valore misurato da `rtl_test -p` |
| solo CRC errati | segnale troppo forte (abbassa `--gain` o allontana la scheda) o troppo debole; deriva di frequenza (`--ppm`) |
| `ModuleNotFoundError` / `ImportError` | non e' attivo l'ambiente: `conda activate lora` |
| `No supported devices found` o `Wrong rtlsdr device index` solo dentro conda | usa il `LD_PRELOAD` indicato nella sezione 2 |
| audio troppo corto o tagliato | `c2dec` su un file `.c2`: copialo con estensione `.bin` (sezione 3) |
| `usb_claim_interface error` | il modulo del kernel della TV digitale occupa la chiavetta: controlla la blacklist e scollega/ricollega |
| messaggi `O` (overflow) | porta `--samp-rate` a 500000 (multiplo di 250000) o chiudi altri programmi pesanti |
| nessun audio riconoscibile | controlla `frame persi` in `lcr_decode`: sotto il 5% la voce e' comprensibile; il Codec2 a 1200 bps ha comunque un suono "robotico" |

Il ricevitore usa il blocco `frame_sync` ... `crc_verif` di `gr-lora_sdr` come nell'esempio `examples/lora_RX.py` del progetto; se una versione futura cambia la firma dei blocchi, confronta `tools/sdr/lora_rx.py` con quell'esempio.
