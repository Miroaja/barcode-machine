#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <limits>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <ostream>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
#include <unordered_map>
#include <utility>

using controller = int;
#include "common.h"

constexpr auto max_abs = std::numeric_limits<int16_t>::max();
constexpr auto min_abs = std::numeric_limits<int16_t>::min();

inline void setup_abs(int fd, uint16_t chan) {
  if (ioctl(fd, UI_SET_ABSBIT, chan)) {
    std::cerr << "Failed to set abs bit" << std::endl;
    return;
  }
  uinput_abs_setup s{};
  s.code = chan;
  s.absinfo.minimum = min_abs;
  s.absinfo.maximum = max_abs;

  if (ioctl(fd, UI_ABS_SETUP, &s)) {
    std::cerr << "Failed to do uinput abs setup" << std::endl;
  }
}

inline controller controller_init() {
  int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

  if (fd < 0) {
    std::cerr << "Failed to open /dev/uinput" << std::endl;
    return -1;
  }

  ioctl(fd, UI_SET_EVBIT, EV_KEY);

  ioctl(fd, UI_SET_KEYBIT, BTN_0);
  ioctl(fd, UI_SET_KEYBIT, BTN_1);
  ioctl(fd, UI_SET_KEYBIT, BTN_2);
  ioctl(fd, UI_SET_KEYBIT, BTN_3);
  ioctl(fd, UI_SET_KEYBIT, BTN_4);
  ioctl(fd, UI_SET_KEYBIT, BTN_5);
  ioctl(fd, UI_SET_KEYBIT, BTN_6);
  ioctl(fd, UI_SET_KEYBIT, BTN_7);
  ioctl(fd, UI_SET_KEYBIT, BTN_8);
  ioctl(fd, UI_SET_KEYBIT, BTN_9);

  ioctl(fd, UI_SET_KEYBIT, BTN_A);
  ioctl(fd, UI_SET_KEYBIT, BTN_B);
  ioctl(fd, UI_SET_KEYBIT, BTN_X);
  ioctl(fd, UI_SET_KEYBIT, BTN_Y);

  ioctl(fd, UI_SET_KEYBIT, BTN_TL);
  ioctl(fd, UI_SET_KEYBIT, BTN_TR);
  ioctl(fd, UI_SET_KEYBIT, BTN_TL2);
  ioctl(fd, UI_SET_KEYBIT, BTN_TR2);

  ioctl(fd, UI_SET_KEYBIT, BTN_START);
  ioctl(fd, UI_SET_KEYBIT, BTN_SELECT);

  ioctl(fd, UI_SET_KEYBIT, BTN_THUMBL);
  ioctl(fd, UI_SET_KEYBIT, BTN_THUMBR);

  ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_UP);
  ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_DOWN);
  ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_LEFT);
  ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_RIGHT);

  ioctl(fd, UI_SET_EVBIT, EV_ABS);

  setup_abs(fd, ABS_X);
  setup_abs(fd, ABS_Y);

  setup_abs(fd, ABS_RX);
  setup_abs(fd, ABS_RY);

  uinput_setup setup{};
  strcpy(setup.name, "Barcode controller");
  setup.id = {
      .bustype = BUS_USB,
      .vendor = 0x3,
      .product = 0x3,
      .version = 2,
  };

  if (ioctl(fd, UI_DEV_SETUP, &setup)) {
    std::cerr << "Failed to setup device" << std::endl;
    return -1;
  }

  if (ioctl(fd, UI_DEV_CREATE)) {
    std::cerr << "Failed to create device" << std::endl;
    return -1;
  }

  return fd;
}

inline void destroy_controller(controller c) {
  if (ioctl(c, UI_DEV_DESTROY)) {

    std::cerr << "Failed to destroy device" << std::endl;
  }

  close(c);
}

inline void send_event(controller c, uint16_t type, uint16_t code,
                       int32_t value) {
  input_event ev{};
  ev.type = type;
  ev.code = code;
  ev.value = value;

  if (write(c, &ev, sizeof(ev)) != sizeof(ev)) {
    std::cerr << "Failed to send event to controller" << std::endl;
  }
}

inline int16_t map_controller_range(float v) {
  float slope = (max_abs - min_abs) / (2.0f);
  float output = min_abs + slope * (v + 1);
  return (int16_t)std::clamp<int32_t>(output, min_abs, max_abs);
}

inline void sync(controller c) { send_event(c, EV_SYN, SYN_REPORT, 0); }

template <side S>
inline void set_joystick(controller c, std::pair<float, float> coords);
template <>
inline void set_joystick<side::left>(controller c,
                                     std::pair<float, float> coords) {
  send_event(c, EV_ABS, ABS_X, map_controller_range(coords.first));
  send_event(c, EV_ABS, ABS_Y, map_controller_range(coords.second));
}
template <>
inline void set_joystick<side::right>(controller c,
                                      std::pair<float, float> coords) {
  send_event(c, EV_ABS, ABS_RX, map_controller_range(coords.first));
  send_event(c, EV_ABS, ABS_RY, map_controller_range(coords.second));
}

inline void press_button(controller c, uint16_t button) {
  send_event(c, EV_KEY, button, 1);
}

inline void release_button(controller c, uint16_t button) {
  send_event(c, EV_KEY, button, 0);
}

const std::unordered_map<std::string, uint16_t> keycode_map = {
    {"0", BTN_0},
    {"1", BTN_1},
    {"2", BTN_2},
    {"3", BTN_3},
    {"4", BTN_4},
    {"5", BTN_5},
    {"6", BTN_6},
    {"7", BTN_7},
    {"8", BTN_8},
    {"9", BTN_9},

    {"A", BTN_A},
    {"B", BTN_B},
    {"X", BTN_X},
    {"Y", BTN_Y},

    {"TL", BTN_TL},
    {"TR", BTN_TR},
    {"TL2", BTN_TL2},
    {"TR2", BTN_TR2},

    {"START", BTN_START},
    {"SELECT", BTN_SELECT},

    {"THUMBL", BTN_THUMBL},
    {"THUMBR", BTN_THUMBR},

    {"DPAD_UP", BTN_DPAD_UP},
    {"DPAD_DOWN", BTN_DPAD_DOWN},
    {"DPAD_LEFT", BTN_DPAD_LEFT},
    {"DPAD_RIGHT", BTN_DPAD_RIGHT}};
