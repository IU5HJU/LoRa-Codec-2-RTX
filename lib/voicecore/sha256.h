// SHA-256 e HMAC-SHA256 (FIPS 180-4, RFC 2104), senza dipendenze: servono a firmare i comandi del sysop (sysop.h).
#pragma once

#include <cstddef>
#include <cstdint>

namespace lcr {

constexpr size_t kSha256Bytes = 32;

void sha256(const uint8_t* data, size_t len, uint8_t out[kSha256Bytes]);
void hmacSha256(const uint8_t* key, size_t keyLen, const uint8_t* msg, size_t msgLen, uint8_t out[kSha256Bytes]);

}  // namespace lcr
