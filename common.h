#pragma once
#include <cstdint>
#include <limits>

enum class macro : uint16_t { run_and_attack, jump, invalid };

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
