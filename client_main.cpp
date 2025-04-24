#include "common.h"
#include "macros.h"
#include <arpa/inet.h>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <ostream>
#include <signal.h>
#include <string>
#include <string_view>
#include <sys/select.h>
#include <sys/socket.h>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
using namespace std::chrono_literals;

namespace conn {
std::string address = "127.0.0.1";
uint16_t port = 6969;
int socket;
}; // namespace conn

namespace app {
std::string dev_path = "/dev/input/event16";
int dev_fd;
const std::unordered_map<int, char> keymap = {
    {KEY_A, 'a'},         {KEY_B, 'b'},           {KEY_C, 'c'},
    {KEY_D, 'd'},         {KEY_E, 'e'},           {KEY_F, 'f'},
    {KEY_G, 'g'},         {KEY_H, 'h'},           {KEY_I, 'i'},
    {KEY_J, 'j'},         {KEY_K, 'k'},           {KEY_L, 'l'},
    {KEY_M, 'm'},         {KEY_N, 'n'},           {KEY_O, 'o'},
    {KEY_P, 'p'},         {KEY_Q, 'q'},           {KEY_R, 'r'},
    {KEY_S, 's'},         {KEY_T, 't'},           {KEY_U, 'u'},
    {KEY_V, 'v'},         {KEY_W, 'w'},           {KEY_X, 'x'},
    {KEY_Y, 'y'},         {KEY_Z, 'z'},           {KEY_1, '1'},
    {KEY_2, '2'},         {KEY_3, '3'},           {KEY_4, '4'},
    {KEY_5, '5'},         {KEY_6, '6'},           {KEY_7, '7'},
    {KEY_8, '8'},         {KEY_9, '9'},           {KEY_0, '0'},
    {KEY_SPACE, ' '},     {KEY_MINUS, '-'},       {KEY_EQUAL, '='},
    {KEY_DOT, '.'},       {KEY_COMMA, ','},       {KEY_SLASH, '/'},
    {KEY_SEMICOLON, ';'}, {KEY_APOSTROPHE, '\''}, {KEY_LEFTBRACE, '['},
    {KEY_RIGHTBRACE, ']'}};
bool running = true;

}; // namespace app

std::string read_str() {
  struct input_event ev;
  std::string buffer = "";

  while (app::running) {
    ssize_t n = read(app::dev_fd, &ev, sizeof(ev));
    if (n != sizeof(ev)) {
      continue;
    }

    if (ev.type == EV_KEY && ev.value == 1) {
      if (ev.code == KEY_ENTER || ev.code == KEY_KPENTER) {
        break;
      }
      if (ev.code == KEY_BACKSPACE && !buffer.empty()) {
        buffer.pop_back();
        continue;
      }
      if (app::keymap.contains(ev.code)) {
        buffer.push_back(app::keymap.at(ev.code));
      }
    }
  }
  return buffer;
}

bool setup_dev() {
  std::cout << "Attempting to open device: " << app::dev_path << std::endl;
  app::dev_fd = open(app::dev_path.c_str(), O_RDONLY, 0);
  if (app::dev_fd < 0) {
    std::cerr << "Failed to open device" << std::endl;
    return false;
  }
  std::cout << "Success!" << std::endl;
  return true;
}

void read_conf() {
  std::ifstream f("client.conf");
  if (!f.is_open()) {
    std::cout << "Failed to open config file, using default conf.\n";
    return;
  }

  std::string var, value;
  while (f >> var >> value) {
    if (var == "address") {
      conn::address = value;
    } else if (var == "port") {
      try {
        conn::port = static_cast<uint16_t>(std::stoi(value));
      } catch (...) {
        std::cerr << "Invalid value for port\n";
      }
    } else if (var == "input_path") {
      app::dev_path = value;
    }
  }
}

bool handshake() {
  uint32_t initial_value = htonl(0xDEADBEEF);
  ssize_t sent_bytes =
      send(conn::socket, &initial_value, sizeof(initial_value), 0);
  if (sent_bytes < 0) {
    std::cerr << "Failed to send initial value" << std::endl;
    return false;
  }

  uint64_t x;
  ssize_t received_bytes = recv(conn::socket, &x, sizeof(x), 0);
  if (received_bytes < 0) {
    std::cerr << "Failed to receive value from server" << std::endl;
    return false;
  }
  x = ntohll(x);
  std::cout << "ACK!" << std::endl;

  uint64_t result = x;
  uint64_t n = x % 69;
  for (uint64_t i = 0; i < n; ++i) {
    result = hash(result);
  }

  result = htonll(result);
  ssize_t sent_hash_bytes = send(conn::socket, &result, sizeof(result), 0);
  if (sent_hash_bytes < 0) {
    std::cerr << "Failed to send hashed value back to server" << std::endl;
    return false;
  }

  std::cout << "Handshake completed successfully." << std::endl;
  return true;
}

void client_connect() {
  while (app::running) {
    std::cout << "Attempting connection to: " << conn::address << std::endl;
    conn::socket = socket(AF_INET, SOCK_STREAM, 0);
    if (conn::socket < 0) {
      std::cerr << "Failed to create socket" << std::endl;
      return;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(conn::port);
    if (inet_pton(AF_INET, conn::address.c_str(), &server_addr.sin_addr) <= 0) {
      std::cerr << "Invalid address" << std::endl;
      close(conn::socket);
    }

    if (connect(conn::socket, (struct sockaddr *)&server_addr,
                sizeof(server_addr)) == 0) {

      if (!handshake()) {
        std::cerr << "Connection failed, retrying..." << std::endl;
        close(conn::socket);
        continue;
      }

      std::cout << "Connected to the server!" << std::endl;
      break;
    } else {
      std::cerr << "Connection failed, retrying..." << std::endl;
      close(conn::socket);
    }
    std::this_thread::sleep_for(1s);
  }
}

void send_macro(const std::string_view &code) {
  macro hash_value;
  SHA256((const uint8_t *)code.data(), code.size(), (uint8_t *)&hash_value);
  std::cout << "Sending macro with code: '" << code << "'  (hash '"
            << hash_value << "') to server" << std::endl;
  send(conn::socket, &hash_value, sizeof(hash_value), 0);
}

void sigint(int) { app::running = false; }
int main(void) {
  struct sigaction sa;
  sa.sa_handler = sigint;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, nullptr);

  termios term_info, save;
  if (tcgetattr(STDIN_FILENO, &term_info) < 0) {
    std::cerr << "Tcgetattr failed." << std::endl;
    exit(EXIT_FAILURE);
  }
  save = term_info;

  term_info.c_lflag &= ~ECHO;
  if (tcsetattr(STDIN_FILENO, TCSANOW, &term_info) < 0) {
    std::cerr << "Tcsetattr failed." << std::endl;
    exit(EXIT_FAILURE);
  }

  read_conf();

  std::string input = "";
  if (!setup_dev()) {
    goto end;
  }
  client_connect();

  std::cout << "Awaiting input..." << std::endl;
  while (app::running) {
    input = read_str();
    send_macro(input);
  }

  close(conn::socket);

end:
  if (tcsetattr(STDIN_FILENO, TCSANOW, &save) < 0) {
    std::cerr << "Tcsetattr failed. Run ttysane to restore a reasonable state."
              << std::endl;
    exit(EXIT_FAILURE);
  }
}
