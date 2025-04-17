#pragma once
#include "common.h"
#include "controller.h"
#include <cstdint>
#include <thread>

using namespace std::chrono_literals;

enum class macro : uint16_t { run_and_attack, jump, invalid };

#define FORWARD (s == side::left ? 1.0 : -1.0)
#define BACK (-FORWARD)
#define UP (-1.0f)
#define DOWN (1.0f)

#define NEUTRAL {0.0f, 0.0f}

// If you want to use the FORWARD and BACK macros for inputs, you must name the
// second argument (of type side) to 's'

namespace macros {
inline void run_and_attack(controller c, side s) {
  set_joystick<side::left>(c, {FORWARD, 0.0f});
  set_joystick<side::right>(c, {0.0f, UP});
  sync(c);
  std::this_thread::sleep_for(500ms);

  set_joystick<side::left>(c, NEUTRAL);
  set_joystick<side::right>(c, NEUTRAL);
  sync(c);
  std::this_thread::sleep_for(100ms);

  press_button(c, BTN_X);
  sync(c);
  std::this_thread::sleep_for(100ms);
  release_button(c, BTN_X);
  sync(c);
}
inline void jump(controller c, side) { (void)c; }
inline void invalid(controller c, side) { (void)c; }
}; // namespace macros

#define DO(m)                                                                  \
  case macro::m: {                                                             \
    macros::m(client->controller, client->side);                               \
  } break
