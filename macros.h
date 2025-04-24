#pragma once
#include "common.h"
#include "controller.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <linux/input-event-codes.h>
#include <openssl/sha.h>
#include <optional>
#include <thread>

using namespace std::chrono_literals;
namespace fs = std::filesystem;

struct macro {
  uint8_t data[256];

  bool operator<(const macro &other) const {
    return std::memcmp(data, other.data, 256) < 0;
  }
};

const std::string base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
inline std::ostream &operator<<(std::ostream &os, const macro &m) {
  std::string encoded;
  int val = 0, valb = -6;
  for (size_t i = 0; i < sizeof(macro); i++) {
    val = (val << 8) + m.data[i];
    valb += 8;
    while (valb >= 0) {
      encoded.push_back(base64_chars[(val >> valb) & 0x3F]);
      valb -= 6;
    }
  }
  if (valb > -6) {
    encoded.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
  }
  while (encoded.size() % 4) {
    encoded.push_back('=');
  }

  return os << encoded;
}

inline std::string base64_encode(const uint8_t *data, size_t length) {
  std::string encoded;
  int val = 0, valb = -6;
  for (size_t i = 0; i < length; i++) {
    val = (val << 8) + data[i];
    valb += 8;
    while (valb >= 0) {
      encoded.push_back(base64_chars[(val >> valb) & 0x3F]);
      valb -= 6;
    }
  }
  if (valb > -6)
    encoded.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
  while (encoded.size() % 4)
    encoded.push_back('=');
  return encoded;
}

using macro_fn = std::function<void()>;
using macro_sequence = std::vector<macro_fn>;

#define FAIL_HEADER "Error while parsing [" << unit << "] :"
#define CHECK_TRAILING_OR_FAIL(ss, context)                                    \
  do {                                                                         \
    std::string trailing;                                                      \
    if (ss >> trailing) {                                                      \
      std::cerr << FAIL_HEADER << "Unexpected trailing input after '"          \
                << context << "': '" << trailing << "'" << std::endl;          \
      return {};                                                               \
    }                                                                          \
  } while (0)

inline std::optional<macro_sequence> build_macro(const fs::path &unit) {
  macro_sequence sequence;
  std::ifstream file(unit);
  std::string line;

  if (!file.is_open()) {
    std::cerr << "Error opening file: " << unit << std::endl;
    return {};
  }

  auto press_key = [](const std::string &key) {
    std::cout << "Pressing key: " << key << std::endl;
    // Here we would simulate pressing a key via uinput or other means
  };

  auto release_key = [](const std::string &key) {
    std::cout << "Releasing key: " << key << std::endl;
    // Here we would simulate releasing a key via uinput or other means
  };

  auto wait = [](int ms) {
    std::cout << "Waiting for " << ms << " ms" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
  };

  auto joy_l = [](float x, float y) {
    std::cout << "Joystick L: (" << x << ", " << y << ")" << std::endl;
    // Here we would send joystick values to the system (using uinput or
    // similar)
  };

  auto joy_r = [](float x, float y) {
    std::cout << "Joystick L: (" << x << ", " << y << ")" << std::endl;
    // Here we would send joystick values to the system (using uinput or
    // similar)
  };

  auto play_macro = [](const macro &macro) {
    std::cout << "Playing macro with hash: " << macro << std::endl;
    // We would look up the macro in `macro_map` and execute its sequence.
  };

  while (std::getline(file, line)) {
    std::stringstream ss(line);
    std::string command;
    ss >> command;

    if (command == "press") {
      std::string key;
      if (!(ss >> key)) {
        std::cerr << FAIL_HEADER << "'press' command requires a key name."
                  << std::endl;
        return {};
      }
      CHECK_TRAILING_OR_FAIL(ss, "press");
      sequence.push_back([key, press_key]() { press_key(key); });
    } else if (command == "release") {
      std::string key;
      if (!(ss >> key)) {
        std::cerr << FAIL_HEADER << "'release' command requires a key name."
                  << std::endl;
        return {};
      }
      CHECK_TRAILING_OR_FAIL(ss, "release");
      sequence.push_back([key, release_key]() { release_key(key); });
    } else if (command == "wait") {
      int time_ms;
      ss >> time_ms;
      if (!(ss >> time_ms)) {
        std::cerr << FAIL_HEADER
                  << "'wait' command requires a time in milliseconds."
                  << std::endl;
        CHECK_TRAILING_OR_FAIL(ss, "wait");
        return {};
      }
      sequence.push_back([time_ms, wait]() { wait(time_ms); });
    } else if (command == "joy_l" || command == "joy_r") {
      char bracket_open, bracket_close;
      float x, y;

      if (!(ss >> bracket_open) || bracket_open != '[') {
        std::cerr << FAIL_HEADER
                  << "'joy_l'/'joy_r' command expects an opening bracket '['."
                  << std::endl;
        return {};
      }
      if (!(ss >> x) || !(ss.ignore(1, ',')) || !(ss >> y)) {
        std::cerr << FAIL_HEADER
                  << "'joy_l'/'joy_r' command requires two float values "
                     "separated by a comma."
                  << std::endl;
        return {};
      }
      if (!(ss >> bracket_close) || bracket_close != ']') {
        std::cerr << FAIL_HEADER
                  << "'joy_l'/'joy_r' command expects a closing bracket "
                     "']'.\n";
        return {};
      }

      CHECK_TRAILING_OR_FAIL(ss, "joy_l/joy_r");
      if (command == "joy_l") {
        sequence.push_back([x, y, joy_l]() { joy_l(x, y); });
      } else {
        sequence.push_back([x, y, joy_r]() { joy_r(x, y); });
      }
    } else if (command == "play") {
      std::string macro_id;
      std::vector<macro> macros;
      char bracket_open, bracket_close;

      if (!(ss >> bracket_open) || bracket_open != '[') {
        std::cerr << FAIL_HEADER
                  << "'play' command expects an opening bracket '['."
                  << std::endl;
        return {};
      }

      while (ss >> macro_id) {
        macro m;
        SHA256((const uint8_t *)macro_id.data(), macro_id.size(),
               (uint8_t *)&m);
        macros.push_back(m);

        if (ss.peek() == ',') {
          ss.ignore();
        }
      }

      if (!(ss >> bracket_close) || bracket_close != ']') {
        std::cerr << FAIL_HEADER
                  << "'play' command expects a closing bracket ']'.\n";
        return {};
      }

      CHECK_TRAILING_OR_FAIL(ss, "play");
      sequence.push_back([macros, play_macro]() {
        size_t index = rand() % macros.size();
        const macro &selected_macro = macros[index];
        play_macro(selected_macro);
      });
    } else {
      std::cerr << "Unknown command: " << command << std::endl;
    }
  }

  return sequence;
}

#define LEFT (std::pair{1.0f, 0.0f})
#define RIGHT (std::pair{-1.0f, 0.0f})
#define UP (std::pair{-1.0f, 0.0f})
#define DOWN (std::pair{1.0f, 0.0f})

#define NEUTRAL {0.0f, 0.0f}

#define FORWARD (s == side::left ? RIGHT : LEFT)
#define BACK (s == side::left ? LEFT : RIGHT)

// If you want to use the FORWARD and BACK macros for inputs, you must name
// the second argument (of type side) to 's'

namespace macros {
inline void run(controller c, side s) {
  set_joystick<side::left>(c, FORWARD);
  sync(c);
  std::this_thread::sleep_for(500ms);

  set_joystick<side::left>(c, NEUTRAL);
  sync(c);
}
inline void run_back(controller c, side) {
  press_button(c, BTN_DPAD_DOWN);
  sync(c);
  std::this_thread::sleep_for(2ms);
  release_button(c, BTN_DPAD_DOWN);
  sync(c);
}
inline void jump(controller c, side) {
  press_button(c, BTN_A);
  press_button(c, BTN_X);
  sync(c);
  std::this_thread::sleep_for(20ms);
  release_button(c, BTN_A);
  release_button(c, BTN_X);
  sync(c);
}
inline void spin_jump(controller c, side) {
  press_button(c, BTN_B);
  sync(c);
  std::this_thread::sleep_for(2ms);
  release_button(c, BTN_B);
  sync(c);
}
inline void balls(controller c, side) {
  press_button(c, BTN_X);
  sync(c);
  std::this_thread::sleep_for(2ms);
  release_button(c, BTN_X);
  sync(c);
}
inline void balls2(controller c, side) {
  press_button(c, BTN_Y);
  sync(c);
  std::this_thread::sleep_for(2ms);
  release_button(c, BTN_Y);
  sync(c);
}
inline void balls3(controller c, side) {
  press_button(c, BTN_DPAD_LEFT);
  sync(c);
  std::this_thread::sleep_for(2ms);
  release_button(c, BTN_DPAD_LEFT);
  sync(c);
}
inline void balls4(controller c, side) {
  press_button(c, BTN_DPAD_UP);
  sync(c);
  std::this_thread::sleep_for(2ms);
  release_button(c, BTN_DPAD_UP);
  sync(c);
}

inline void invalid(controller c, side) {
  press_button(c, BTN_DPAD_RIGHT);
  sync(c);
  std::this_thread::sleep_for(2ms);
  release_button(c, BTN_DPAD_RIGHT);
  sync(c);
}
}; // namespace macros

#define DO(m)                                                                  \
  case macro::m: {                                                             \
    macros::m(client->controller, client->side);                               \
  } break
