// Costanti del protocollo VoLoRa (v2).
// Vedi docs/protocollo.md per la descrizione completa.
#pragma once

#include <cstddef>
#include <cstdint>

namespace lcr {

constexpr uint8_t kProtocolVersion = 2;  // v2 (09/11/2026): flag kFlagPosition (blocco posizione) e kFlagTotal (totale pacchetti) nel pacchetto START

// Modo Codec2 trasportato (nibble basso del primo byte dell'header).
constexpr uint8_t kModeCodec2_1200 = 1;
// Codec2 3200 bps (profilo Standard, dal 12/11/2026; prima "Qualita'"): 2 blocchi da 20 ms e 8 byte = un frame da 40 ms e 16 byte.
constexpr uint8_t kModeCodec2_3200 = 4;
// Prova di collegamento: i frame sono una sequenza nota (non voce) che il ricevitore confronta bit per bit.
constexpr uint8_t kModeTest = 2;
// Risposta del ricevitore alla prova (un solo pacchetto con il risultato).
constexpr uint8_t kModeTestReport = 3;
// Annuncio di "ripetitore occupato" (docs/studio-ripetitore.md): un solo pacchetto START|END con il nominativo del ripetitore e, come
// payload, 2 byte (big-endian) con i secondi di blocco previsti; 0 = di nuovo libero. E' solo un'informazione: le stazioni la
// mostrano sul display e non rifiutano il PTT. Il ripetitore lo manda con i parametri della voce prima di ritrasmettere un
// messaggio lungo (che le stazioni in Standard e Range non vedono con il CAD) e due volte, a distanza di kTextRepeatGapMs.
constexpr uint8_t kModeBusy = 6;
// Comando del sysop al ripetitore e risposta del ripetitore (sysop.h): un solo pacchetto START|END con il nominativo di chi manda,
// payload di 15 byte (comando) o 18 byte (risposta), firmato con HMAC-SHA256 troncato a 8 byte.
constexpr uint8_t kModeSysop = 5;
constexpr size_t kSysopCommandBytes = 15;
constexpr size_t kSysopReplyBytes = 18;
constexpr size_t kBusyPayloadBytes = 2;
// Messaggio di testo breve (stile SMS, fino a kMaxTextChars caratteri ASCII, senza '{' e '}'): un solo pacchetto START|END con il nominativo
// di chi scrive; il payload e' il testo. Nessuna parita'. Il mittente lo trasmette due volte (stessa sessione): i duplicati si
// scartano.
constexpr uint8_t kModeText = 7;
constexpr size_t kMaxTextChars = 150;
// Identificativo periodico del ripetitore: un pacchetto di testo (modo 7) con seq = kTextSeqBeacon. Le stazioni lo mostrano come "ultimo
// ricevuto" con il segnale, ma non lo mettono in casella e non suonano (docs/studio-ripetitore.md, "Identificativo del ponte").
constexpr uint8_t kTextSeqBeacon = 1;
// Pausa tra le due copie del messaggio di testo.
constexpr uint32_t kTextRepeatGapMs = 1000;

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

// Dimensione massima di un frame audio da 40 ms (Codec2 3200: 16 byte).
constexpr size_t kMaxFrameBytes = 16;

// Byte di un frame da 40 ms nel modo dato (i modi sconosciuti o di servizio si trattano come il 1200: 6 byte).
inline size_t frameBytesFor(uint8_t mode) { return mode == kModeCodec2_3200 ? kMaxFrameBytes : kFrameBytes; }

constexpr size_t kFrameSamples = 320;
constexpr uint32_t kFrameMs = 40;

// Un pacchetto dati trasporta 12 frame = 480 ms di voce = 72 byte.
constexpr size_t kFramesPerPacket = 12;
constexpr size_t kMaxDataPayload = kFramesPerPacket * kMaxFrameBytes;

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
// Limite di durata (TOT) di un messaggio nel profilo Messaggio: 15 s di voce (11/11/2026), come il massimo che il ripetitore
// ripete (lib/voicecore/message_store.h). A 1200 bps sono circa 139 s in aria a SF12/BW125. Il tetto di kMaxTurnMs (30 s)
// resta per lo Standard e il Range.
constexpr uint32_t kMessageMaxTurnMs = 15000;

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
  kFlagTotal = 64,    // dopo il blocco posizione c'e' 1 byte: numero totale di pacchetti della trasmissione (solo START, messaggi)
  kFlagPosition = 32, // dopo il nominativo c'e' il blocco posizione (position.h), solo nel pacchetto START
  kFlagRelayed = 128, // pacchetto ritrasmesso da un ripetitore (repeater.h): un ripetitore non lo ripete di nuovo
};
constexpr uint8_t kFlagsKnown = 255;  // tutti gli 8 bit sono in uso
// Flag protetti dalla parita' (gli altri sono derivabili o non utili).
constexpr uint8_t kFlagsParityMask = kFlagStart | kFlagEnd;

// Dimensione massima di un pacchetto sul canale.
// Buffer di ricezione: anche i frame di testo di altre reti (fino a 255 byte).
constexpr size_t kMaxRxBytes = 256;

constexpr size_t kMaxWireBytes = 5 + 1 + kMaxCallsign + 7 /* blocco posizione, position.h */ + 1 /* totale pacchetti */ + kMaxPayload;

}  // namespace lcr
