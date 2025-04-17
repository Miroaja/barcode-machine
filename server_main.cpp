#include "common.h"
#include "controller.h"
#include <arpa/inet.h>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <queue>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

using namespace std::chrono_literals;

namespace conn {
constexpr uint16_t port = 6969;
constexpr size_t max_clients = 4;
int socket;
}; // namespace conn

constexpr int max_inputs_queued = 4;

struct client_info {
  int socket;
  std::mutex mut;
  std::queue<macro> input_queue;
  controller controller;
  side side;
  bool ready_to_die;
};

bool check_queue_empty(client_info *client) {
  std::lock_guard lck(client->mut);
  return client->input_queue.empty();
}

void client_connection(client_info *client) {
  while (true) {
    uint16_t buf;
    ssize_t received_bytes = recv(client->socket, &buf, sizeof(buf), 0);
    if (received_bytes != sizeof(buf)) {
      std::cerr << "Failed to receive value from client" << std::endl;
      continue;
    }
    buf = ntohs(buf);

    if (buf == disconnect_flag) {
      client->ready_to_die = true;
      return;
    }
    macro m = (macro)buf;

    {
      std::lock_guard lck(client->mut);
      if (client->input_queue.size() >= max_inputs_queued) {
        continue;
      }
      client->input_queue.push(m);
    }
  }
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

    switch (m) {
      DO(run_and_attack);
      DO(jump);
      DO(invalid);
    };

  skip:
    std::this_thread::sleep_for(10ms);
  }
  close(client->socket);
  destroy_controller(client->controller);
  delete client;
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

int main(void) {
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

  while (true) {
    int client_socket = accept(conn::socket, nullptr, nullptr);
    if (client_socket < 0) {
      std::cerr << "Failed to accept client connection" << std::endl;
      continue;
    }
    if (!handshake(client_socket)) {
      close(client_socket);
      continue;
    }

    side s;
    ssize_t received_bytes = recv(client_socket, &s, sizeof(s), 0);
    if (received_bytes != sizeof(s)) {
      std::cerr << "Failed to get client sideness" << std::endl;
      close(client_socket);
      continue;
    }

    client_info *client = new client_info{
        .socket = client_socket,
        .mut = std::mutex{},
        .input_queue = std::queue<macro>{},
        .controller = controller_init(),
        .side = s,
        .ready_to_die = false,

    };

    std::thread(client_connection, client).detach();
    std::thread(client_input, client).detach();
  }
}
