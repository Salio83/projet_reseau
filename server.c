#include "api_contract.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main() {
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  if (server_fd == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }
  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(8080);
  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
    perror("bind");
    exit(EXIT_FAILURE);
  }
  if (listen(server_fd, 10) == -1) {
    perror("listen");
    exit(EXIT_FAILURE);
  }
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);
  int client_socket =
      accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

  if (client_socket == -1) {
    perror("accept");
    exit(EXIT_FAILURE);
  }
  return 0;
}