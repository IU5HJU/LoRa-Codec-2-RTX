// Impostazioni dell'operatore.
#pragma once

// Nominativo trasmesso a inizio e fine turno e ripetuto ogni ~5 s.
// Solo A-Z, 0-9 e '/', massimo 8 caratteri.
#define LCR_CALLSIGN "IU5HJU"

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
