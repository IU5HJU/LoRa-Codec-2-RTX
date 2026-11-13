// Prove radio dell'identificativo in Morse (docs/studio-identificativo-fm.md): CW puro (portante accesa e spenta) e Morse a tono in
// FM stretta, con i pacchetti FSK del SX1278. Qui c'e' solo la parte senza radio: lettura del comando CFG RADIOTEST e piano dei
// pacchetti, provati sul PC (tests/test_radio_test.cpp). La radio e' pilotata da src/radio_lora.* (fskBegin, fskSend, fskEnd) e
// da src/main.cpp (runRadioTest).
//
// Come funziona: nel modo FSK un pacchetto a lunghezza fissa senza preambolo, senza parola di sincronismo e senza CRC e' solo la sua
// sequenza di bit. Tutti i bit a 1 (0xFF) sono una portante senza modulazione (CW); i bit 1010... (0xAA) alternano la frequenza di
// +-deviazione a meta' del bit rate: un tono quadro di (bit rate / 2) Hz che un ricevitore FM riproduce come un fischio. Un punto
// o una linea e' un pacchetto della durata giusta; le pause sono silenzio.
//
// Comando (resto della riga dopo "CFG RADIOTEST"):
//   MORSE FM|CW TESTO [wpm=15] [hz=700] [dev=2.5] [ramp=0..15] [shape=0|0.3|0.5|1.0]   un identificativo
//   BEACON FM|CW TESTO PERIODO_S [stessi parametri]                                    lo ripete ogni PERIODO_S secondi (fino a un tasto in seriale)
//   CARRIER [SECONDI] [ramp=..]                                                        portante senza modulazione (misura della frequenza)
//   TONE [SECONDI] [hz=700] [dev=2.5] [shape=..] [ramp=..]                             tono continuo in FM (misura di deviazione e tono)
// TESTO: lettere e cifre (il segno _ e' uno spazio), al massimo 16 caratteri.
#pragma once

#include <cstddef>
#include <cstdint>

namespace lcr {

enum class RadioTestKind : uint8_t { Morse, Beacon, Carrier, Tone };

struct RadioTestRequest {
  RadioTestKind kind = RadioTestKind::Morse;
  bool fm = true;       // Morse a tono in FM (true) o CW puro (false); Carrier e' sempre CW, Tone sempre FM
  char text[17] = {0};
  int wpm = 15;         // parole al minuto (PARIS): un punto dura 1200 / wpm ms
  float toneHz = 700;   // tono in FM
  float devKhz = 2.5f;  // deviazione di picco in FM
  int ramp = -1;        // salita della potenza (4 bit del registro PaRamp), -1 = valore del chip
  int shape = 0;        // filtro gaussiano in FM: 0 nessuno, 3 = BT 0,3, 5 = BT 0,5, 10 = BT 1,0
  int seconds = 5;      // durata di Carrier e Tone
  int periodS = 60;     // periodo del Beacon
};

constexpr float kCwBitRateKbps = 2.4f;  // CW: pacchetti fino a 850 ms, passo di 3,3 ms
constexpr float kCwDevKhz = 0.6f;       // deviazione minima: la portante e' spostata di 0,6 kHz e si compensa sulla frequenza

// Legge i parametri. false con un messaggio in 'err' se qualcosa non va.
bool parseRadioTest(const char* args, RadioTestRequest& out, char* err, size_t cap);

// Bit rate (kbit/s) dei pacchetti: CW 2,4; FM il doppio del tono.
float radioTestBitRateKbps(const RadioTestRequest& r);
// Byte del pacchetto: 0xFF (portante) o 0xAA (tono).
inline uint8_t radioTestPattern(const RadioTestRequest& r) {
  return (r.kind == RadioTestKind::Tone || ((r.kind == RadioTestKind::Morse || r.kind == RadioTestKind::Beacon) && r.fm)) ? 0xAA : 0xFF;
}
// Durata in aria di un pacchetto di 'bytes' byte a 'kbps', in millisecondi.
float burstAirMs(uint16_t bytes, float kbps);

// Morse in FM: la portante resta accesa per tutto l'identificativo. Prima del primo tono una portante pura apre lo squelch dei
// ricevitori (kMorseLeadMs), nelle pause fra i toni la portante continua (non si spegne la radio), alla fine una coda (kMorseTailMs).
constexpr uint32_t kMorseLeadMs = 500;
constexpr uint32_t kMorseTailMs = 250;

// Trasmette 'ms' millisecondi di portante pura (pacchetti 0xFF da al massimo 255 byte, uno dopo l'altro). 'onSent(ms)' dopo ogni pacchetto.
// false se la radio rifiuta un pacchetto.
template <class Radio, class OnSent>
bool sendCarrierMs(Radio& radio, uint32_t ms, float kbps, OnSent onSent) {
  long left = static_cast<long>(static_cast<double>(ms) * kbps / 8.0 + 0.5);
  while (left > 0) {
    const uint16_t n = static_cast<uint16_t>(left > 255 ? 255 : left);
    if (!radio.fskSend(0xFF, n)) return false;
    onSent(static_cast<uint32_t>(static_cast<float>(n) * 8.0f / kbps + 0.5f));
    left -= n;
  }
  return true;
}

struct MorseBurst {
  bool tone;       // true: pacchetto da trasmettere, false: pausa
  uint32_t ms;     // durata voluta
  uint16_t bytes;  // byte del pacchetto (solo per i toni)
};

// Elementi del Morse di r.text con le durate di r.wpm, ognuno tradotto in byte. Un tono troppo lungo per un pacchetto (255 byte)
// viene accorciato e *clamped diventa true. Ritorna il numero di elementi.
size_t planMorseBursts(const RadioTestRequest& r, MorseBurst* out, size_t cap, bool* clamped);

}  // namespace lcr
