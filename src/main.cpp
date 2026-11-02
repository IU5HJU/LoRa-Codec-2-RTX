// Firmware: da completare quando arrivano le schede (pin radio, I2S, OLED, PTT).
// La logica del protocollo e' gia' in lib/voicecore ed e' testata sul PC.
//
// Da fare:
//  1. adattatore IRadio con RadioLib (SX1278): begin(), scanChannel() per il CAD,
//     transmit(), startReceive(), lettura pacchetti;
//  2. adattatore ICodec con arduino-codec2 (modo 1200);
//  3. adattatori IAudioIn / IAudioOut su I2S (microfono e amplificatore);
//  4. pulsante PTT: pinMode(LCR_PTT_PIN, INPUT_PULLUP), PttButton::update() nel loop()
//     e pttPressed()/pttReleased() su VoiceNode; poi OLED e segnali acustici (INotifier);
//  5. VoiceNode::tick() nel loop().
#include <Arduino.h>

#include "user_config.h"

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("LoRa-Codec-2-RTX " LCR_CALLSIGN ": firmware da completare (vedi docs/pinout.md)");
}

void loop() { delay(1000); }
