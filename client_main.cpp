#include "common.h"
#include <arpa/inet.h>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
using namespace std::chrono_literals;

namespace conn {
constexpr std::string address = "";
constexpr uint16_t port = 6969;
int socket;
}; // namespace conn

namespace app {
bool running = true;
const std::unordered_map<std::string, macro> input_map{
    {"4251595201907", macro::run_and_attack}, {"6429810459824", macro::jump}};
}; // namespace app

bool handshake() {
  uint32_t initial_value = 0xDEADBEEF;
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
  std::cout << "ACK!" << std::endl;

  uint64_t result = x;
  uint64_t n = x % 69;
  for (uint64_t i = 0; i < n; ++i) {
    result = hash(result);
  }

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

void send_macro(macro macro_id) {
  ssize_t sent_hash_bytes = send(conn::socket, &macro_id, sizeof(macro_id), 0);
  if (sent_hash_bytes < 0) {
    std::cerr << "Failed to send macro" << std::endl;
  }
}

int main(void) {
  client_connect();

  std::string input;
  while (app::running) {
    std::cin >> input;
    if (app::input_map.contains(input)) {
      send_macro(app::input_map.at(input));
    } else {
      send_macro(macro::invalid);
    }
  }
  send(conn::socket, &disconnect_flag, sizeof(disconnect_flag), 0);
  close(conn::socket);
}
