// Costanti del protocollo LoRa-Codec-2-RTX (v2).
// Vedi docs/protocollo.md per la descrizione completa.
#pragma once

#include <cstddef>
#include <cstdint>

namespace lcr {

constexpr uint8_t kProtocolVersion = 2;  // v2 (09/11/2026): flag kFlagPosition e blocco posizione nel pacchetto START

// Modo Codec2 trasportato (nibble basso del primo byte dell'header).
constexpr uint8_t kModeCodec2_1200 = 1;
// Prova di collegamento: i frame sono una sequenza nota (non voce) che il ricevitore confronta bit per bit.
constexpr uint8_t kModeTest = 2;
// Risposta del ricevitore alla prova (un solo pacchetto con il risultato).
constexpr uint8_t kModeTestReport = 3;
// Pacchetto di controllo dell'hardware (CFG TEST AUDIO): la scheda trasmette per misurare se la radiofrequenza
// rientra nel microfono. I ricevitori lo ignorano come ogni modo sconosciuto.
constexpr uint8_t kModeRfCheck = 15;

// Codec2 1200 bps: 48 bit (6 byte) ogni 40 ms, 320 campioni a 8 kHz.
constexpr size_t kFrameBytes = 6;
// Pausa tra la fine di un pacchetto trasmesso e l'inizio del successivo. Il ricevitore, dopo aver letto un
// pacchetto, deve rileggere la radio, aggiornare il display e suonare l'audio: se il pacchetto seguente
// (tipicamente la parita' subito dopo il quarto dato) parte troppo presto, quello precedente viene
// sovrascritto nel FIFO della radio e va perso. Costa meno di un frame di voce (40 ms * 2).
constexpr uint32_t kInterPacketGapMs = 80;

constexpr size_t kFrameSamples = 320;
constexpr uint32_t kFrameMs = 40;

// Un pacchetto dati trasporta 12 frame = 480 ms di voce = 72 byte.
constexpr size_t kFramesPerPacket = 12;
constexpr size_t kMaxDataPayload = kFramesPerPacket * kFrameBytes;

// Parita' XOR: un pacchetto di parita' ogni 4 pacchetti dati.
// Layout in sequenza: D D D D P D D D D P ... (seq % 5 == 4 e' parita').
constexpr size_t kParityGroup = 4;
constexpr size_t kParityPeriod = kParityGroup + 1;
// Payload parita' = [count][xorFrames][xorFlags][xor dei payload dati].
constexpr size_t kParityHeader = 3;
constexpr size_t kMaxPayload = kMaxDataPayload + kParityHeader;

constexpr size_t kMaxCallsign = 8;

// Numero massimo di pacchetti per trasmissione (seq e' < kMaxPackets).
// Un turno da 30 s usa circa 80 pacchetti; il limite TX lascia margine.
constexpr size_t kMaxPackets = 128;
constexpr size_t kTxSeqLimit = 120;

// Durata della prova di collegamento: 300 frame = 12 s, circa 25 pacchetti dati piu' la parita'.
constexpr uint32_t kTestFrames = 300;

// Pulsante PTT: una pressione piu' breve di kPttClickMaxMs e' un "clic" e non trasmette; la trasmissione parte
// solo se il PTT resta premuto per questo tempo (il microfono e' comunque bufferizzato dalla pressione).
// Tre clic, ognuno entro kPttClickGapMs dal precedente, attivano o disattivano la prova di collegamento.
constexpr uint32_t kPttClickMaxMs = 300;
constexpr uint32_t kPttClickGapMs = 500;

// Attesa prima che il ricevitore risponda alla prova: chi ha trasmesso deve tornare in ascolto.
constexpr uint32_t kTestReplyDelayMs = 700;

// Modo "registra e poi trasmetti" (prova): dopo il rilascio del PTT la trasmissione parte con questo ritardo.
constexpr uint32_t kRecSendDelayMs = 1000;

// Durata massima di un turno di trasmissione.
constexpr uint32_t kMaxTurnMs = 30000;
constexpr uint32_t kMaxTurnFrames = kMaxTurnMs / kFrameMs;

// Il nominativo e' ripetuto ogni N pacchetti (circa 5 s) per chi si
// sintonizza a trasmissione gia' iniziata.
constexpr uint32_t kCallsignRepeatPackets = 10;

// Ricezione: frame da accumulare prima di iniziare la riproduzione (1 s),
// attesa massima per il pre-buffer e silenzio dopo cui la sessione e' chiusa.
constexpr size_t kPrebufferFrames = 25;
constexpr uint32_t kPrebufferTimeoutMs = 2000;
constexpr uint32_t kSessionTimeoutMs = 3000;

enum Flags : uint8_t {
  kFlagStart = 1,     // primo pacchetto della trasmissione
  kFlagEnd = 2,       // ultimo pacchetto dati (OVER)
  kFlagParity = 4,    // pacchetto di parita' XOR
  kFlagCallsign = 8,  // l'header contiene il nominativo
  kFlagNoFec = 16,    // la trasmissione non usa parita'
  kFlagPosition = 32, // dopo il nominativo c'e' il blocco posizione (position.h), solo nel pacchetto START
};
constexpr uint8_t kFlagsKnown = 63;
// Flag protetti dalla parita' (gli altri sono derivabili o non utili).
constexpr uint8_t kFlagsParityMask = kFlagStart | kFlagEnd;

// Dimensione massima di un pacchetto sul canale.
constexpr size_t kMaxWireBytes = 5 + 1 + kMaxCallsign + 7 /* blocco posizione, position.h */ + kMaxPayload;

}  // namespace lcr
