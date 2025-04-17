#pragma once
#include "common.h"
#include "controller.h"
#include <cstdint>

enum class macro : uint16_t { run_and_attack, jump, invalid };

inline float forward(side s) { return s == side::left ? 1.0 : -1.0; }
inline float back(side s) { return -forward(s); }

namespace macros {
inline void run_and_attack(controller c, side s) {
  (void)c;
  (void)s;
}
inline void jump(controller c, side) { (void)c; }
inline void invalid(controller c, side) { (void)c; }
}; // namespace macros

#define DO(m)                                                                  \
  case macro::m: {                                                             \
    macros::m(client->controller, client->side);                               \
  } break
