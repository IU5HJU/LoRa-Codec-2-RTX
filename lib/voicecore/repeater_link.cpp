#include "repeater_link.h"

#include <cstring>

namespace lcr {

uint16_t linkCrc16(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (int b = 0; b < 8; b++) crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
  }
  return crc;
}

size_t encodeLinkFrame(uint8_t type, const uint8_t* data, size_t len, uint8_t* out, size_t cap) {
  if (len > kLinkMaxData || len > 255 || cap < 3 + len + 2) return 0;
  out[0] = kLinkStart;
  out[1] = type;
  out[2] = static_cast<uint8_t>(len);
  if (len) memcpy(out + 3, data, len);
  const uint16_t crc = linkCrc16(out + 1, 2 + len);
  out[3 + len] = static_cast<uint8_t>(crc >> 8);
  out[4 + len] = static_cast<uint8_t>(crc & 0xFF);
  return 5 + len;
}

void LinkParser::drop(size_t k) {
  if (k >= n_) {
    n_ = 0;
    return;
  }
  memmove(buf_, buf_ + k, n_ - k);
  n_ -= k;
}

bool LinkParser::feed(uint8_t b) {
  if (n_ >= sizeof(buf_)) drop(1);
  buf_[n_++] = b;
  for (;;) {
    // Si parte sempre da un 0x7E.
    size_t s = 0;
    while (s < n_ && buf_[s] != kLinkStart) s++;
    if (s) drop(s);
    if (n_ < 3) return false;
    const size_t need = 3 + static_cast<size_t>(buf_[2]) + 2;
    if (buf_[2] > kLinkMaxData) {  // lunghezza impossibile: falso inizio
      drop(1);
      continue;
    }
    if (n_ < need) return false;
    const uint16_t crc = linkCrc16(buf_ + 1, 2 + buf_[2]);
    if (buf_[need - 2] == static_cast<uint8_t>(crc >> 8) && buf_[need - 1] == static_cast<uint8_t>(crc & 0xFF)) {
      frame_.type = buf_[1];
      frame_.len = buf_[2];
      if (frame_.len) memcpy(frame_.data, buf_ + 3, frame_.len);
      drop(need);
      return true;
    }
    crcErrors_++;
    drop(1);  // CRC errato: il 0x7E era un falso inizio, si cerca il prossimo
  }
}

bool TxQueue::push(const uint8_t* data, size_t len) {
  if (!data || len == 0 || len > kLinkMaxData) return false;
  if (count_ >= kSlots) {
    dropped_++;
    return false;
  }
  const size_t slot = (head_ + count_) % kSlots;
  memcpy(data_[slot], data, len);
  len_[slot] = len;
  count_++;
  return true;
}

bool TxQueue::next(uint32_t nowMs, uint8_t* out, size_t cap, size_t& len) {
  if (count_ == 0) return false;
  if (haveSent_ && nowMs - lastSentMs_ < kInterPacketGapMs) return false;
  if (len_[head_] > cap) return false;
  len = len_[head_];
  memcpy(out, data_[head_], len);
  head_ = (head_ + 1) % kSlots;
  count_--;
  return true;
}

}  // namespace lcr
