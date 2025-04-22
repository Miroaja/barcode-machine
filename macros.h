#pragma once
#include "common.h"
#include "controller.h"
#include <cstdint>
#include <linux/input-event-codes.h>
#include <thread>

using namespace std::chrono_literals;

enum class macro : uint16_t { run, run_back, jump, spin_jump, invalid };

#define LEFT (std::pair{1.0f, 0.0f})
#define RIGHT (std::pair{-1.0f, 0.0f})
#define UP (std::pair{-1.0f, 0.0f})
#define DOWN (std::pair{1.0f, 0.0f})

#define NEUTRAL {0.0f, 0.0f}

#define FORWARD (s == side::left ? RIGHT : LEFT)
#define BACK (s == side::left ? LEFT : RIGHT)

// If you want to use the FORWARD and BACK macros for inputs, you must name the
// second argument (of type side) to 's'

namespace macros {
inline void run(controller c, side s) {
  set_joystick<side::left>(c, FORWARD);
  sync(c);
  std::this_thread::sleep_for(500ms);

  set_joystick<side::left>(c, NEUTRAL);
  sync(c);
}
inline void run_back(controller c, side s) {
  set_joystick<side::left>(c, BACK);
  sync(c);
  std::this_thread::sleep_for(500ms);

  set_joystick<side::left>(c, NEUTRAL);
  sync(c);
}
inline void jump(controller c, side s) {
  press_button(c, BTN_A);
  set_joystick<side::left>(c, FORWARD);
  sync(c);
  std::this_thread::sleep_for(100ms);
  set_joystick<side::left>(c, NEUTRAL);
  release_button(c, BTN_A);
  sync(c);
}
inline void spin_jump(controller c, side s) {
  press_button(c, BTN_X);
  set_joystick<side::left>(c, FORWARD);
  sync(c);
  std::this_thread::sleep_for(100ms);
  set_joystick<side::left>(c, NEUTRAL);
  release_button(c, BTN_X);
  sync(c);
}

inline void invalid(controller c, side) { (void)c; }
}; // namespace macros

#define DO(m)                                                                  \
  case macro::m: {                                                             \
    macros::m(client->controller, client->side);                               \
  } break
