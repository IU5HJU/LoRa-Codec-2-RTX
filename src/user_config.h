// Impostazioni dell'operatore.
#pragma once

// Nominativo trasmesso a inizio e fine turno e ripetuto ogni ~5 s.
// Solo A-Z, 0-9 e '/', massimo 8 caratteri.
#define LCR_CALLSIGN "IU5HJU"

// Pulsante PTT. Di default il pulsante a bordo (GPIO0, BOOT/PRG), se la tua
// revisione lo ha: da verificare sulla scheda. GPIO0 e' un pin di boot, quindi
// non tenerlo premuto durante accensione o reset.
// Per un pulsante esterno cambia solo LCR_PTT_PIN (verso massa, con pull-up interno).
#define LCR_PTT_PIN 0
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
// Sync word LoRa dedicato (diverso da LoRaWAN 0x34 e Meshtastic 0x2B).
#define LCR_SYNC_WORD 0x4C
