// Codice Morse come elenco di toni e pause, per la segnalazione acustica di arrivo di un messaggio ("MSG", come lo
// squillo dei vecchi telefoni per gli SMS). Indipendente dalla scheda e testato sul PC (tests/test_morse.cpp).
//
// Tempi del Morse (unita' = durata di un punto): punto 1, linea 3, pausa tra gli elementi di una lettera 1, tra le lettere 3,
// tra le parole 7.
#pragma once

#include <cstddef>
#include <cstdint>

namespace lcr {

struct MorseElement {
  bool tone;    // true: suona, false: pausa
  uint16_t ms;  // durata
};

// Segnalazione di messaggio arrivato: lettere "MSG", 1000 Hz (dove l'altoparlante piccolo rende meglio), unita' di 60 ms.
constexpr const char* kMessageTone = "MSG";
constexpr float kMessageToneHz = 1000.0f;
constexpr uint16_t kMessageToneUnitMs = 60;
// Segnalazione di messaggio di testo arrivato: lettere "SMS", stesso tono e stessa unita' di "MSG".
constexpr const char* kTextTone = "SMS";

// Riempie 'out' con gli elementi di 'text' (lettere A-Z e cifre; le minuscole valgono come maiuscole, gli spazi sono pause
// tra parole, i caratteri sconosciuti sono saltati). Nessuna pausa finale. Ritorna il numero di elementi scritti; si ferma
// quando 'cap' e' pieno.
size_t morseElements(const char* text, uint16_t unitMs, MorseElement* out, size_t cap);

// Durata totale degli elementi, in millisecondi.
uint32_t morseDurationMs(const MorseElement* e, size_t n);

}  // namespace lcr
