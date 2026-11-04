// Seleziona la mappa dei pin in base alla scheda scelta a compilazione.
//   -DBOARD_TBEAM_V12   LILYGO T-Beam V1.2 (AXP2101)
//   -DBOARD_T3_V161     LILYGO LoRa32 T3 V1.6.1 (predefinita)
// Il resto del codice usa sempre l'alias `pins::`.
#pragma once

#if defined(BOARD_TBEAM_V12)
#include "board_pins_tbeam_v12.h"
namespace pins = pins_tbeam_v12;
#else
#include "board_pins_t3_v161.h"
namespace pins = pins_t3_v161;
#endif
