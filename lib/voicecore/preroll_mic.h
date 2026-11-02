// Microfono sempre acceso con pre-roll.
//
// Il task di cattura chiama feed() per ogni frame da 40 ms, anche a PTT
// rilasciato; il ring conserva solo gli ultimi N frame. Alla pressione del PTT
// (start) l'audio gia' presente (pre-roll) viene trasmesso per primo, cosi' la
// prima sillaba non si perde e non servono transitori di avvio del microfono.
//
// ATTENZIONE: feed() e le altre funzioni girano in contesti diversi sul
// firmware (task di cattura e loop principale). La classe non ha lock: serve un
// mutex nell'adattatore, oppure un'unica chiamata dal medesimo task.
#pragma once

#include <cstddef>
#include <cstdint>

#include "interfaces.h"
#include "protocol.h"

namespace lcr {

class PrerollMic : public IAudioIn {
 public:
  static constexpr size_t kCapacity = 32;  // frame nel ring (1,28 s)

  explicit PrerollMic(size_t prerollFrames = 5);  // 5 frame = 200 ms

  // Dal task di cattura: un frame da kFrameSamples campioni.
  void feed(const int16_t* pcm);

  void start() override;
  void stop() override;
  bool readFrame(int16_t* pcm) override;

  size_t buffered() const { return count_; }
  uint32_t overruns() const { return overruns_; }

 private:
  void trimTo(size_t n);

  size_t preroll_;
  bool recording_ = false;
  size_t head_ = 0;
  size_t count_ = 0;
  uint32_t overruns_ = 0;
  int16_t frames_[kCapacity][kFrameSamples];
};

}  // namespace lcr
