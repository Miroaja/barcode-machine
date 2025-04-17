#pragma once
#include <cstdint>
#include <ios>
#include <limits>

enum class side : uint8_t { left, right };
#include "macros.h"

constexpr uint16_t disconnect_flag = std::numeric_limits<uint16_t>::max();

inline uint64_t hash(uint64_t u) {
  uint64_t v = u * 3935559000370003845 + 2691343689449507681;

  v ^= v >> 21;
  v ^= v << 37;
  v ^= v >> 4;

  v *= 4768777513237032717;

  v ^= v << 20;
  v ^= v >> 41;
  v ^= v << 5;

  return v;
}

#if __BIG_ENDIAN__
#define htonll(x) (x)
#define ntohll(x) (x)
#else
#define htonll(x) (((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
#define ntohll(x) (((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))
#endif
