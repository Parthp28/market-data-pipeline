#pragma once

// Why: a single bytes.hpp keeps endian helpers inline and header-only so the
// hot path never links a separate endian object file or pays a call overhead
// that a libc wrapper would introduce on every field decode.

#include <cstdint>
#include <cstring>

namespace mdp {

inline uint16_t load_be16(const void* p) {
  uint16_t v = 0;
  std::memcpy(&v, p, sizeof(v));
  return static_cast<uint16_t>((v >> 8) | (v << 8));
}

inline uint32_t load_be32(const void* p) {
  uint32_t v = 0;
  std::memcpy(&v, p, sizeof(v));
  return ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) |
         ((v & 0x00FF0000u) >> 8) | ((v & 0xFF000000u) >> 24);
}

inline uint64_t load_be64(const void* p) {
  uint64_t v = 0;
  std::memcpy(&v, p, sizeof(v));
  return ((v & 0x00000000000000FFull) << 56) | ((v & 0x000000000000FF00ull) << 40) |
         ((v & 0x0000000000FF0000ull) << 24) | ((v & 0x00000000FF000000ull) << 8) |
         ((v & 0x000000FF00000000ull) >> 8) | ((v & 0x0000FF0000000000ull) >> 24) |
         ((v & 0x00FF000000000000ull) >> 40) | ((v & 0xFF00000000000000ull) >> 56);
}

inline void store_be16(void* p, uint16_t v) {
  const uint16_t be = static_cast<uint16_t>((v >> 8) | (v << 8));
  std::memcpy(p, &be, sizeof(be));
}

inline void store_be32(void* p, uint32_t v) {
  const uint32_t be = ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) |
                      ((v & 0x00FF0000u) >> 8) | ((v & 0xFF000000u) >> 24);
  std::memcpy(p, &be, sizeof(be));
}

inline void store_be64(void* p, uint64_t v) {
  const uint64_t be =
      ((v & 0x00000000000000FFull) << 56) | ((v & 0x000000000000FF00ull) << 40) |
      ((v & 0x0000000000FF0000ull) << 24) | ((v & 0x00000000FF000000ull) << 8) |
      ((v & 0x000000FF00000000ull) >> 8) | ((v & 0x0000FF0000000000ull) >> 24) |
      ((v & 0x00FF000000000000ull) >> 40) | ((v & 0xFF00000000000000ull) >> 56);
  std::memcpy(p, &be, sizeof(be));
}

}  // namespace mdp
