#include "../common/ipc_utils/ipc_utils.h"
#include "../common/network_models/packet_types.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 6767

static int recv_all(int fd, void *buffer, size_t len) {
  char *cursor = (char *)buffer;
  size_t received = 0;
  while (received < len) {
    ssize_t chunk = recv(fd, cursor + received, len - received, 0);
    if (chunk <= 0)
      return -1;
    received += (size_t)chunk;
  }
  return 0;
}

static int create_client(void) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in serv_addr;

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(SERVER_PORT);
  inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);
  connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  return sock;
}

static void auth_client(int sock, const char *username) {
  PacketHeader header = {PACKET_AUTH_REQ, PACKET_SIZE(AuthRequest), 0};
  AuthRequest request;
  memset(&request, 0, sizeof(request));
  strncpy(request.username, username, sizeof(request.username) - 1);
  strncpy(request.password_hash, "hash", sizeof(request.password_hash) - 1);
  send(sock, &header, sizeof(header), 0);
  send(sock, &request, sizeof(request), 0);

  char buffer[MAX_MSG_SIZE];
  PacketHeader *response = (PacketHeader *)buffer;
  recv_all(sock, response, sizeof(PacketHeader));
  recv_all(sock, buffer + sizeof(PacketHeader), response->length - sizeof(PacketHeader));
}

int main(void) {
  printf("--- Test Flux de Jeu Minimal ---\n");

  int c1 = create_client();
  int c2 = create_client();

  auth_client(c1, "Alice");
  auth_client(c2, "Bob");

  printf("[Client 1] Matchmaking...\n");
  send(c1, &(PacketHeader){PACKET_MATCHMAKING_REQ, PACKET_SIZE(MatchmakingRequest), 0},
       sizeof(PacketHeader), 0);
  MatchmakingRequest req1;
  memset(&req1, 0, sizeof(req1));
  strncpy(req1.username, "Alice", sizeof(req1.username) - 1);
  send(c1, &req1, sizeof(req1), 0);

  printf("[Client 2] Matchmaking...\n");
  send(c2, &(PacketHeader){PACKET_MATCHMAKING_REQ, PACKET_SIZE(MatchmakingRequest), 0},
       sizeof(PacketHeader), 0);
  MatchmakingRequest req2;
  memset(&req2, 0, sizeof(req2));
  strncpy(req2.username, "Bob", sizeof(req2.username) - 1);
  send(c2, &req2, sizeof(req2), 0);

  char buf[MAX_MSG_SIZE];
  PacketHeader *res_h = (PacketHeader *)buf;
  recv_all(c1, res_h, sizeof(PacketHeader));
  recv_all(c1, buf + sizeof(PacketHeader), res_h->length - sizeof(PacketHeader));

  if (res_h->type == PACKET_GAME_STARTED) {
    GameStarted *started = (GameStarted *)(buf + sizeof(PacketHeader));
    printf("[Client 1] Partie démarrée ! ID: %u, Couleur: %s\n", started->room_id,
           started->your_color == 0 ? "Blanc" : "Noir");

    PacketHeader move_h = {PACKET_PLAYER_MOVE, PACKET_SIZE(PlayerMove), 0};
    PlayerMove move = {started->room_id, "e2", "e4", '\0'};
    send(c1, &move_h, sizeof(move_h), 0);
    send(c1, &move, sizeof(move), 0);

    recv_all(c1, res_h, sizeof(PacketHeader));
    recv_all(c1, buf + sizeof(PacketHeader), res_h->length - sizeof(PacketHeader));

    if (res_h->type == PACKET_GAME_SNAPSHOT || res_h->type == PACKET_GAME_UPDATE_UDP) {
      printf("[Client 1] Réponse reçue après le coup: type=%u\n", res_h->type);
    }
  }

  close(c1);
  close(c2);
  return 0;
}
