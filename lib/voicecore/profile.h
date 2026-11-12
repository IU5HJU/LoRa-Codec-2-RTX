// Profili di trasmissione: parametri radio e comportamento di una modalita'.
//
//   Standard   voce in tempo reale a Codec2 3200, SF7/BW250: piu' fedele, il modo di base (via ripetitore o nodo). Dal 12/11/2026;
//              fino ad allora si chiamava "Qualita'" e "Standard" era il Codec2 1200.
//   Range      voce in tempo reale a Codec2 1200, SF8/BW250 (il vecchio "Standard"): un po' piu' di portata, meno fedelta'.
//   Messaggio  lungo raggio: SF12/BW125. Si registra un messaggio, lo si trasmette piu' lentamente del parlato e chi
//              riceve lo memorizza e lo riproduce a ricezione conclusa.
//
// Il ricevitore LoRa ascolta un solo SF e una sola banda: due schede con profili diversi non si sentono. Il profilo si sceglie
// con CFG SET PROFILE (vale dal riavvio). Vedi docs/roadmap.md ("Tre profili") e docs/modo-messaggio.md.
#pragma once

#include <cstddef>
#include <cstdint>

#include "protocol.h"

namespace lcr {

enum ProfileId : uint8_t { kProfileRange = 0, kProfileMessage = 1, kProfileStandard = 2, kProfileCount = 3 };

struct Profile {
  uint8_t id;
  const char* code;  // nome usato da CFG: "std", "rng", "msg" ("qua" vale ancora, come "std")
  const char* name;  // nome per esteso
  // Radio
  uint8_t sf;
  float bwKhz;
  uint8_t cr;  // 5..8 = 4/5..4/8
  uint16_t preambleSymbols;
  // Trasmissione
  uint8_t codecMode;   // kModeCodec2_1200 o kModeCodec2_3200 (dice anche i byte per frame: frameBytesFor)
  uint32_t maxTurnMs;  // durata massima della voce di un turno o di un messaggio
  bool message;        // registra e poi invia (TX) / memorizza e poi riproduci (RX)
  // Ricezione (vedi RxStream)
  size_t rxPrebufferFrames;
  uint32_t rxPrebufferTimeoutMs;
  uint32_t rxSessionTimeoutMs;
  // Sync word LoRa (0 = quella di user_config.h) e profilo di solo testo (nessuna voce)
  uint8_t syncWord;
  bool textOnly;
};

// Parametri radio di una breve escursione sulle reti di testo radioamatoriali a pacchetto (MeshCom, 433,175 MHz), da un profilo
// voce qualsiasi: SF11/BW250/CR4/6, sync word 0x2B, preambolo 8. Non e' un profilo selezionabile: si entra dal menu per inviare un
// testo e ascoltare le risposte per un po', poi si torna al profilo di prima (docs/meshcom-testo.md).
const Profile& meshcomProfile();

// id sconosciuto: profilo Standard.
const Profile& profileFor(uint8_t id);
// "std" / "rng" / "msg" (maiuscole o minuscole; "qua", il vecchio nome del Codec2 3200, vale "std"). false se il nome non esiste.
bool profileFromCode(const char* code, uint8_t& id);

// Durata in aria di un pacchetto di 'bytes' byte (formula di Semtech per SX127x: intestazione LoRa esplicita, CRC attivo,
// ottimizzazione per basse velocita' attiva quando il simbolo dura 16 ms o piu'). Stima, non una misura.
double airtimeMs(size_t bytes, const Profile& p);

// Tempo in aria di un messaggio di 'voiceMs' millisecondi di voce: pacchetti dati, parita' (1 su 4), START e END con
// nominativo di 'callsignLen' caratteri, senza le pause tra pacchetti.
double messageAirtimeMs(uint32_t voiceMs, size_t callsignLen, const Profile& p);

}  // namespace lcr
