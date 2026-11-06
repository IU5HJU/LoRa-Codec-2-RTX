// Impostazioni dell'operatore.
#pragma once

// Nominativo trasmesso a inizio e fine turno e ripetuto ogni ~5 s.
// Solo A-Z, 0-9 e '/', massimo 8 caratteri.
// Il valore predefinito N0CALL e' il segnaposto "nessun nominativo": con N0CALL
// la scheda riceve ma NON trasmette. Per trasmettere inserire il proprio nominativo.
#ifndef LCR_CALLSIGN
#define LCR_CALLSIGN "N0CALL"
#endif

// Pulsante PTT. Il pin dipende dalla scheda (src/board_pins_*.h):
//  - T3 V1.6.1: ha solo il tasto RST, quindi serve un pulsante ESTERNO tra GPIO0 e massa
//    (pull-up interno). GPIO0 e' un pin di boot: se e' premuto durante accensione o reset
//    la scheda entra in modalita' flash.
//  - T-Beam V1.2: tasto di bordo IO38 (pull-up esterno della scheda).
// Il PTT vale solo dopo essere stato rilasciato almeno una volta dopo l'accensione.
#define LCR_PTT_ACTIVE_LOW 1
#define LCR_PTT_DEBOUNCE_MS 30

// Parita' XOR (1 pacchetto ogni 4): consigliata.
#define LCR_FEC 1

// Parametri radio (SX1278, 433 MHz).
// ATTENZIONE: la frequenza e' un SEGNAPOSTO, da scegliere con il band plan
// IARU Regione 1 / ARI per la sottobanda digitale dei 70 cm prima di trasmettere.
#define LCR_FREQUENCY_MHZ 433.900f
#define LCR_BANDWIDTH_KHZ 250.0f
#define LCR_SPREADING_FACTOR 8
#define LCR_CODING_RATE 5  // 4/5
#define LCR_TX_POWER_DBM 10
#define LCR_PREAMBLE_SYMBOLS 12
// Sync word LoRa dedicato (diverso da LoRaWAN 0x34 e Meshtastic 0x2B).
#define LCR_SYNC_WORD 0x4C

// Microfono I2S: slot dello stereo da cui leggere (0 = sinistro, con L/R a GND).
// Se si sente solo silenzio provare 1.
#define LCR_MIC_SLOT 0

// OLED SSD1306 (128x64, indirizzo 0x3C) sulla T3 V1.6.1.
#define LCR_OLED_ADDRESS 0x3C

// Microfono (AGC). Il guadagno sale solo sopra il livello del fondo + 6 dB; in silenzio scende verso 12 dB, cosi' il rumore
// non viene amplificato come voce. Il fondo e' il livello "AGC in" misurato in silenzio: standard -60 dBFS (stanza
// silenziosa con l'INMP441). Ognuno lo misura con "Misura il fondo" nella pagina di configurazione e lo corregge a mano
// (CFG SET MICFLOOR, da -80 a -36 dBFS); il valore salvato nella scheda sostituisce questo.
#define LCR_MIC_FLOOR_DBFS -60
#define LCR_MIC_AGC_MAX_GAIN_DB 30.0f  // guadagno massimo

// Volume iniziale dell'altoparlante (0 = muto, 1-10 in passi da 3 dB, 10 = massimo). Si cambia anche
// dalla configurazione (CFG SET VOLUME) e, a scheda accesa, con il pulsante volume su GPIO15.
#define LCR_VOLUME_STEP 7

// Versione del firmware: la imposta tools/git_version.py in compilazione (git describe); senza git vale "dev".
#ifndef LCR_VERSION
#define LCR_VERSION "dev"
#endif
