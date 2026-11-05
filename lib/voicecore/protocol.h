// Costanti del protocollo LoRa-Codec-2-RTX (v1).
// Vedi docs/protocollo.md per la descrizione completa.
#pragma once

#include <cstddef>
#include <cstdint>

namespace lcr {

constexpr uint8_t kProtocolVersion = 1;

// Modo Codec2 trasportato (nibble basso del primo byte dell'header).
constexpr uint8_t kModeCodec2_1200 = 1;

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
};
constexpr uint8_t kFlagsKnown = 31;
// Flag protetti dalla parita' (gli altri sono derivabili o non utili).
constexpr uint8_t kFlagsParityMask = kFlagStart | kFlagEnd;

// Dimensione massima di un pacchetto sul canale.
constexpr size_t kMaxWireBytes = 5 + 1 + kMaxCallsign + kMaxPayload;

}  // namespace lcr
