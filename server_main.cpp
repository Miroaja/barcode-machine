#include "common.h"
#include "controller.h"
#include "macros.h"
#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <map>
#include <mutex>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <ostream>
#include <queue>
#include <sys/socket.h>
#include <termios.h>
#include <thread>
#include <unistd.h>

using namespace std::chrono_literals;
namespace chrono = std::chrono;
namespace fs = std::filesystem;

namespace conn {
constexpr uint16_t port = 6969;
constexpr size_t max_clients = 4;
int socket;
}; // namespace conn

using duration_t = chrono::duration<float>;

namespace app {
std::mutex macro_mut;
std::map<macro, macro_sequence> macros;
std::atomic_bool running = true;
std::atomic_bool paused = false;
duration_t cooldown_increment = 1s;
duration_t cooldown_base = 500ms;
duration_t cooldown_max = 3s;
}; // namespace app

struct client_info {
  int socket;
  std::mutex mut;
  std::queue<macro> input_queue;
  controller controller;
  std::map<macro, std::pair<chrono::time_point<chrono::high_resolution_clock>,
                            duration_t>>
      cooldowns;
  bool ready_to_die;
};

void load_macros() {
  const fs::path macro_dir = "macros";

  if (!fs::exists(macro_dir) || !fs::is_directory(macro_dir)) {
    std::cerr << "Error: Macro directory '" << macro_dir
              << "' does not exist or is not a directory.\n";
    return;
  }

  for (const auto &entry : fs::directory_iterator(macro_dir)) {
    if (!entry.is_regular_file()) {
      continue;
    }

    const auto &path = entry.path();
    const std::string filename = path.filename().string();

    macro macro_id;
    SHA256((const uint8_t *)filename.data(), filename.size(),
           (uint8_t *)&macro_id);

    auto sequence_opt = build_macro(path);
    if (!sequence_opt) {
      std::cerr << "Failed to load macro from file: " << filename << std::endl;
      exit(EXIT_FAILURE);
    }

    std::lock_guard lck(app::macro_mut);
    app::macros[macro_id] = std::move(*sequence_opt);
    std::cout << "Loaded macro: " << filename << " with hash '" << macro_id
              << "'" << std::endl;
  }
}

bool check_queue_empty(client_info *client) {
  std::lock_guard lck(client->mut);
  return client->input_queue.empty();
}

void client_connection(client_info *client) {
  while (app::running) {

    macro buf;
    ssize_t received_bytes = recv(client->socket, &buf, sizeof(buf), 0);
    if (received_bytes == 0) {
      client->ready_to_die = true;
      return;
    }

    if (received_bytes != sizeof(buf)) {
      std::cerr << "Failed to receive value from client" << std::endl;
      continue;
    }

    if (app::paused) {
      continue;
    }
    {
      std::lock_guard lck(client->mut);
      client->input_queue.push(buf);
    }
  }
  client->ready_to_die = true;
}

void client_input(client_info *client) {
  while (!client->ready_to_die) {
    macro m;
    if (!check_queue_empty(client)) {
      std::lock_guard lck(client->mut);
      m = client->input_queue.front();
      client->input_queue.pop();
    } else {
      goto skip;
    }

    std::thread([m, client] {
      auto now = chrono::high_resolution_clock::now();

      if (!client->cooldowns.contains(m)) {
        client->cooldowns[m] = std::pair{now, app::cooldown_base};
        std::cout << "Initializing cooldown for macro: '" << m << "'"
                  << std::endl;
      } else {
        auto &[start, duration] = client->cooldowns[m];

        auto elapsed = chrono::duration_cast<duration_t>(now - start);

        if (elapsed < duration) {
          duration =
              std::min(duration + app::cooldown_increment, app::cooldown_max);
          // TODO: discuss wether to reset the start time here. i.e. if the
          // cooldown should reset fully or just extend
          return;
        }

        duration = app::cooldown_base;
        start = now;
      }

      macro_sequence seq;
      {
        std::lock_guard lck(app::macro_mut);
        if (app::macros.contains(m)) {
          seq = app::macros.at(m);
        } else {
          seq = undefined_macro_seq;
        }
      }

      std::cout << "Playing macro with hash: '" << m << "'" << std::endl;
      play_sequence(client->controller, seq, &app::macros);
    }).detach();

  skip:
    std::this_thread::sleep_for(10ms);
  }
  close(client->socket);
  destroy_controller(client->controller);
  delete client;
  std::cout << "Destroyed client." << std::endl;
}

void pause_monitor() {
  std::string input;
  while (app::running) {
    std::cin >> input;
    if (input != "pause") {
      continue;
    }

    app::paused = !app::paused;
    std::cout << (app::paused ? "Paused!" : "Resumed!") << std::endl;
  }
}

uint64_t get_random_64bit() {
  uint64_t random_value;
  int fd = open("/dev/urandom", O_RDONLY);
  if (fd < 0) {
    return 0;
  }
  read(fd, &random_value, sizeof(random_value));
  close(fd);
  return random_value;
}

bool handshake(int socket) {
  uint32_t initial_value;
  ssize_t received_bytes =
      recv(socket, &initial_value, sizeof(initial_value), 0);
  if (received_bytes < 0) {
    std::cerr << "Failed to receive initial value" << std::endl;
    return false;
  }
  if (ntohl(initial_value) != 0xDEADBEEF) {
    std::cerr << "ACK failed." << std::endl;
    return false;
  }
  std::cout << "ACK!" << std::endl;
  uint64_t x = htonll(get_random_64bit());
  ssize_t sent_bytes = send(socket, &x, sizeof(x), 0);
  if (sent_bytes < 0) {
    std::cerr << "Failed to send value to client" << std::endl;
    return false;
  }

  x = ntohll(x);
  uint64_t result = x;
  uint64_t n = x % 69;
  for (uint64_t i = 0; i < n; ++i) {
    result = hash(result);
  }

  uint64_t client_result;
  ssize_t received_hash_bytes =
      recv(socket, &client_result, sizeof(client_result), 0);
  if (received_hash_bytes < 0) {
    std::cerr << "Failed to receive hashed value from client" << std::endl;
    return false;
  }
  if (ntohll(client_result) != result) {
    std::cerr << "Hash comparison failed." << std::endl;
    return false;
  }

  std::cout << "Handshake completed successfully" << std::endl;
  return true;
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

  load_macros();

  conn::socket = socket(AF_INET, SOCK_STREAM, 0);
  if (conn::socket < 0) {
    std::cerr << "Failed to create socket" << std::endl;
    return 1;
  }
  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(conn::port);

  if (bind(conn::socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    std::cerr << "Failed to bind socket" << std::endl;
    close(conn::socket);
    return 1;
  }

  if (listen(conn::socket, conn::max_clients) < 0) {
    std::cerr << "Failed to listen on socket" << std::endl;
    close(conn::socket);
    return 1;
  }

  std::thread(pause_monitor).detach();
  while (app::running) {
    int client_socket = accept(conn::socket, nullptr, nullptr);
    if (client_socket < 0) {
      std::cerr << "Failed to accept client connection" << std::endl;
      continue;
    }
    if (!handshake(client_socket)) {
      close(client_socket);
      continue;
    }

    client_info *client = new client_info{
        .socket = client_socket,
        .mut = std::mutex{},
        .input_queue = std::queue<macro>{},
        .controller = controller_init(),
        .cooldowns = decltype(client_info::cooldowns){},
        .ready_to_die = false,
    };

    std::thread(client_connection, client).detach();
    std::thread(client_input, client).detach();
  }

  close(conn::socket);

  if (tcsetattr(STDIN_FILENO, TCSANOW, &save) < 0) {
    std::cerr << "Tcsetattr failed. Run ttysane to restore a reasonable state."
              << std::endl;
    exit(EXIT_FAILURE);
  }
}
