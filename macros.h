#pragma once
#include "common.h"
#include "controller.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <linux/input-event-codes.h>
#include <map>
#include <openssl/sha.h>
#include <optional>
#include <syncstream>
#include <thread>

using namespace std::chrono_literals;
namespace fs = std::filesystem;

struct macro {
  uint8_t data[SHA256_DIGEST_LENGTH];

  bool operator<(const macro &other) const {
    return std::memcmp(data, other.data, SHA256_DIGEST_LENGTH) < 0;
  }

  bool operator==(const macro &other) const {
    return std::memcmp(data, other.data, SHA256_DIGEST_LENGTH) == 0;
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

using macro_fn = std::function<void(controller, const void *)>;
using macro_sequence = std::vector<macro_fn>;

using context_t = const std::map<macro, macro_sequence>;

inline void play_sequence(controller c, const macro_sequence &seq,
                          context_t *context) {
  for (const auto &action : seq) {
    action(c, context);
  }
}

inline const macro_sequence undefined_macro_seq = {
    [](controller, const void *) {
      // do some bs here;
      std::cout << "huhh\n";
    }};

#define FAIL_HEADER                                                            \
  "Error while parsing [" << unit << "] @ line_n " << line_n << " : \n\""      \
                          << line << "\"\n"
#define CHECK_TRAILING_OR_FAIL(ss, context)                                    \
  do {                                                                         \
    std::string trailing;                                                      \
    if (ss >> trailing) {                                                      \
      std::cerr << FAIL_HEADER << "Unexpected trailing shit in '" << context   \
                << "': '" << trailing << "'" << std::endl;                     \
      return {};                                                               \
    }                                                                          \
  } while (0)
inline std::optional<macro_sequence> build_macro(const fs::path &unit) {
  macro_sequence sequence;
  std::ifstream file(unit);
  std::string line;
  int line_n = 0;

  if (!file.is_open()) {
    std::cerr << "Error opening file: " << unit << std::endl;
    return {};
  }

  auto press_key = [](controller c, uint16_t key, const std::string &key_name) {
    std::cout << "Pressing key: " << key_name << std::endl;
    press_button(c, key);
  };

  auto release_key = [](controller c, uint16_t key,
                        const std::string &key_name) {
    std::cout << "Releasing key: " << key_name << std::endl;
    release_button(c, key);
  };

  auto wait = [](controller c, int ms) {
    std::cout << "Waiting for " << ms << " ms" << std::endl;
    sync(c);
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
  };

  auto joy_l = [](controller c, float x, float y) {
    std::cout << "Joystick L: (" << x << ", " << y << ")" << std::endl;
    set_joystick<side::left>(c, {x, y});
  };

  auto joy_r = [](controller c, float x, float y) {
    std::cout << "Joystick R: (" << x << ", " << y << ")" << std::endl;
    set_joystick<side::right>(c, {x, y});
  };

  auto play_macro = [](controller c, const macro &macro, context_t *context) {
    std::cout << "Playing macro with hash: '" << macro << "'" << std::endl;
    play_sequence(c, context->at(macro), context);
  };

  while (std::getline(file, line)) {
    line_n++;
    if (line == "") {
      continue;
    }

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
      sequence.push_back([key, press_key](controller c, const void *) {
        press_key(c, keycode_map.at(key), key);
      });
    } else if (command == "release") {
      std::string key;
      if (!(ss >> key)) {
        std::cerr << FAIL_HEADER << "'release' command requires a key name."
                  << std::endl;
        return {};
      }
      CHECK_TRAILING_OR_FAIL(ss, "release");
      sequence.push_back([key, release_key](controller c, const void *) {
        release_key(c, keycode_map.at(key), key);
      });
    } else if (command == "wait") {
      int time_ms;
      if (!(ss >> time_ms)) {
        std::cerr << FAIL_HEADER
                  << "'wait' command requires a time in milliseconds."
                  << std::endl;
        CHECK_TRAILING_OR_FAIL(ss, "wait");
        return {};
      }
      sequence.push_back(
          [time_ms, wait](controller c, const void *) { wait(c, time_ms); });
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
        sequence.push_back(
            [x, y, joy_l](controller c, const void *) { joy_l(c, x, y); });
      } else {
        sequence.push_back(
            [x, y, joy_r](controller c, const void *) { joy_r(c, x, y); });
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
      sequence.push_back(
          [macros, play_macro](controller c, const void *context) {
            size_t index = rand() % macros.size();
            const macro &selected_macro = macros[index];
            play_macro(c, selected_macro, (context_t *)context);
          });
    } else {
      std::cerr << "Unknown command: " << command << std::endl;
    }
  }

  sequence.push_back([](controller c, const void *) { sync(c); });

  return sequence;
}
