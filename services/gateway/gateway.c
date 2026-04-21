#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "../../common/ipc_utils/ipc_keys.h"
#include "../../common/ipc_utils/ipc_utils.h"
#include "../../common/network_models/packet_types.h"
#include "gateway.h"

#define TCP_PORT 6767
#define UDP_PORT 6768
#define MAX_CLIENTS 100
#define BUFFER_SIZE MAX_MSG_SIZE

typedef struct {
    int in_use;
    int authenticated;
    int tcp_fd;
    uint32_t session_id;
    struct sockaddr_in udp_addr;
    int udp_registered;
    char username[MAX_USERNAME_LEN];
    uint32_t current_room_id;
    uint8_t role;
} ClientSession;

static int global_mq;
static int udp_server_fd;
static uint32_t next_session_id = 1;
static ClientSession sessions[MAX_CLIENTS];

static int send_all(int fd, const void *buffer, size_t len) {
    const char *cursor = (const char *)buffer;
    while (len > 0) {
        ssize_t written = send(fd, cursor, len, 0);
        if (written <= 0) {
            return -1;
        }
        cursor += written;
        len -= (size_t)written;
    }
    return 0;
}

static int recv_all(int fd, void *buffer, size_t len) {
    char *cursor = (char *)buffer;
    size_t received = 0;

    while (received < len) {
        ssize_t chunk = recv(fd, cursor + received, len - received, 0);
        if (chunk <= 0) {
            return -1;
        }
        received += (size_t)chunk;
    }

    return 0;
}

static void setup_ipc(void) {
    int fd = open(GLOBAL_MSG_QUEUE_PATH, O_CREAT | O_RDWR, 0666);
    if (fd != -1) {
        close(fd);
    }

    global_mq = ipc_msg_get(ipc_get_key(GLOBAL_MSG_QUEUE_PATH, GLOBAL_MSG_QUEUE_ID));
    if (global_mq == -1) {
        fprintf(stderr, "[Gateway] Impossible d'accéder à la file IPC.\n");
        exit(EXIT_FAILURE);
    }
}

static ClientSession *find_session_by_id(uint32_t session_id) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (sessions[i].in_use && sessions[i].session_id == session_id) {
            return &sessions[i];
        }
    }
    return NULL;
}

static ClientSession *create_session(int tcp_fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!sessions[i].in_use) {
            memset(&sessions[i], 0, sizeof(sessions[i]));
            sessions[i].in_use = 1;
            sessions[i].tcp_fd = tcp_fd;
            sessions[i].session_id = next_session_id++;
            return &sessions[i];
        }
    }
    return NULL;
}

static void send_packet_error(ClientSession *session, uint16_t code, const char *message) {
    if (!session || session->tcp_fd <= 0) {
        return;
    }

    char out_buf[MAX_MSG_SIZE];
    PacketHeader *header = (PacketHeader *)out_buf;
    PacketError *error = (PacketError *)(out_buf + sizeof(PacketHeader));

    memset(out_buf, 0, sizeof(out_buf));
    header->type = PACKET_ERROR;
    header->length = sizeof(PacketHeader) + sizeof(PacketError);
    header->session_id = session->session_id;
    error->code = code;
    strncpy(error->message, message, sizeof(error->message) - 1);

    send_all(session->tcp_fd, out_buf, header->length);
}

static void notify_disconnect(const ClientSession *session) {
    if (!session) {
        return;
    }

    char out_buf[MAX_MSG_SIZE];
    PacketHeader *header = (PacketHeader *)out_buf;
    ClientDisconnected *payload = (ClientDisconnected *)(out_buf + sizeof(PacketHeader));

    memset(out_buf, 0, sizeof(out_buf));
    header->type = PACKET_CLIENT_DISCONNECTED;
    header->length = sizeof(PacketHeader) + sizeof(ClientDisconnected);
    header->session_id = session->session_id;
    payload->room_id = session->current_room_id;
    payload->role = session->role;

    ipc_msg_send(global_mq, out_buf, header->length, MSG_TYPE_GAMEWORKER);
    ipc_msg_send(global_mq, out_buf, header->length, MSG_TYPE_CHAT);
}

static void destroy_session(ClientSession *session) {
    if (!session || !session->in_use) {
        return;
    }

    notify_disconnect(session);
    if (session->tcp_fd > 0) {
        close(session->tcp_fd);
    }
    memset(session, 0, sizeof(*session));
}

static void route_packet(ClientSession *session, PacketHeader *header, const char *payload) {
    char out_buf[MAX_MSG_SIZE];
    size_t payload_len = header->length - sizeof(PacketHeader);
    long target_type = -1;

    if (!session) {
        return;
    }

    if (!session->authenticated && header->type != PACKET_AUTH_REQ) {
        send_packet_error(session, 401, "Authentification requise.");
        return;
    }

    memset(out_buf, 0, sizeof(out_buf));
    header->session_id = session->session_id;

    switch (header->type) {
        case PACKET_AUTH_REQ:
            target_type = MSG_TYPE_AUTH;
            memcpy(out_buf, header, sizeof(PacketHeader));
            if (payload_len > 0) {
                memcpy(out_buf + sizeof(PacketHeader), payload, payload_len);
            }
            break;
        case PACKET_MATCHMAKING_REQ: {
            MatchmakingRequest req;
            memset(&req, 0, sizeof(req));
            req.player_id = session->session_id;
            strncpy(req.username, session->username, sizeof(req.username) - 1);
            header->length = sizeof(PacketHeader) + sizeof(MatchmakingRequest);
            memcpy(out_buf, header, sizeof(PacketHeader));
            memcpy(out_buf + sizeof(PacketHeader), &req, sizeof(req));
            target_type = MSG_TYPE_MATCHMAKING;
            break;
        }
        case PACKET_PLAYER_MOVE:
        case PACKET_LIST_ACTIVE_GAMES_REQ:
        case PACKET_SPECTATE_LEAVE_REQ:
            memcpy(out_buf, header, sizeof(PacketHeader));
            if (payload_len > 0) {
                memcpy(out_buf + sizeof(PacketHeader), payload, payload_len);
            }
            target_type = MSG_TYPE_GAMEWORKER;
            break;
        case PACKET_CHAT_MSG:
            memcpy(out_buf, header, sizeof(PacketHeader));
            if (payload_len > 0) {
                memcpy(out_buf + sizeof(PacketHeader), payload, payload_len);
            }
            target_type = MSG_TYPE_CHAT;
            break;
        case PACKET_SPECTATE_JOIN_REQ: {
            SpectateJoinRequest request;
            memset(&request, 0, sizeof(request));
            if (payload_len >= sizeof(request)) {
                memcpy(&request, payload, sizeof(request));
            } else if (payload_len >= sizeof(request.room_id)) {
                memcpy(&request, payload, sizeof(request.room_id));
            }
            strncpy(request.username, session->username, sizeof(request.username) - 1);
            header->length = sizeof(PacketHeader) + sizeof(SpectateJoinRequest);
            memcpy(out_buf, header, sizeof(PacketHeader));
            memcpy(out_buf + sizeof(PacketHeader), &request, sizeof(request));
            target_type = MSG_TYPE_GAMEWORKER;
            break;
        }
        case PACKET_TOURNAMENT_CREATE_REQ:
        case PACKET_TOURNAMENT_LIST_REQ:
        case PACKET_TOURNAMENT_JOIN_REQ:
            memcpy(out_buf, header, sizeof(PacketHeader));
            if (payload_len > 0) {
                memcpy(out_buf + sizeof(PacketHeader), payload, payload_len);
            }
            target_type = MSG_TYPE_TOURNAMENT;
            break;
        default:
            send_packet_error(session, 400, "Type de paquet inconnu.");
            return;
    }

    ipc_msg_send(global_mq, out_buf, header->length, target_type);
}

static void handle_udp_registration(void) {
    char buffer[MAX_MSG_SIZE];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    ssize_t received = recvfrom(udp_server_fd, buffer, sizeof(buffer), 0,
                                (struct sockaddr *)&client_addr, &client_len);

    if (received < (ssize_t)(sizeof(PacketHeader) + sizeof(UdpRegisterRequest))) {
        return;
    }

    PacketHeader *header = (PacketHeader *)buffer;
    if (header->type != PACKET_UDP_REGISTER_REQ) {
        return;
    }

    UdpRegisterRequest *request = (UdpRegisterRequest *)(buffer + sizeof(PacketHeader));
    ClientSession *session = find_session_by_id(request->session_id);
    if (!session) {
        return;
    }

    session->udp_addr = client_addr;
    session->udp_registered = 1;
}

static void handle_service_responses(void) {
    char msg_buffer[MAX_MSG_SIZE];

    while (ipc_msg_receive_nowait(global_mq, msg_buffer, sizeof(msg_buffer), MSG_TYPE_GATEWAY) != -1) {
        PacketHeader *header = (PacketHeader *)msg_buffer;
        ClientSession *session = find_session_by_id(header->session_id);

        if (header->type == PACKET_GAME_UPDATE_UDP) {
            if (!session) {
                continue;
            }
            if (session->udp_registered) {
                sendto(udp_server_fd, msg_buffer, header->length, 0,
                       (struct sockaddr *)&session->udp_addr, sizeof(session->udp_addr));
            } else {
                send_all(session->tcp_fd, msg_buffer, header->length);
            }
            continue;
        }

        if (!session) {
            continue;
        }

        if (header->type == PACKET_AUTH_OK) {
            AuthOk *ok = (AuthOk *)(msg_buffer + sizeof(PacketHeader));   
            session->authenticated = 1;
            strncpy(session->username, ok->username, sizeof(session->username) - 1);
            ipc_msg_send(global_mq, msg_buffer, header->length, MSG_TYPE_CHAT);
        } else if (header->type == PACKET_GAME_STARTED) {
            GameStarted *started = (GameStarted *)(msg_buffer + sizeof(PacketHeader));
            session->current_room_id = started->room_id;
            session->role = ROOM_ROLE_PLAYER;
            ipc_msg_send(global_mq, msg_buffer, header->length, MSG_TYPE_CHAT);
        } else if (header->type == PACKET_SPECTATE_JOIN_OK) {
            SpectateStatus *status = (SpectateStatus *)(msg_buffer + sizeof(PacketHeader));
            session->current_room_id = status->room_id;
            session->role = ROOM_ROLE_SPECTATOR;
            ipc_msg_send(global_mq, msg_buffer, header->length, MSG_TYPE_CHAT);
        } else if (header->type == PACKET_SPECTATE_LEAVE_OK) {
            ipc_msg_send(global_mq, msg_buffer, header->length, MSG_TYPE_CHAT);
            session->current_room_id = 0;
            session->role = ROOM_ROLE_NONE;
        }

        if (send_all(session->tcp_fd, msg_buffer, header->length) == -1) {
            destroy_session(session);
        }
    }
}

void start_gateway(void) {
    int server_fd;
    struct sockaddr_in tcp_addr;
    struct sockaddr_in udp_addr;

    setup_ipc();
    memset(sessions, 0, sizeof(sessions));

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    udp_server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_server_fd < 0) {
        perror("socket udp");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    setsockopt(udp_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&tcp_addr, 0, sizeof(tcp_addr));
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_addr.sin_port = htons(TCP_PORT);

    if (bind(server_fd, (struct sockaddr *)&tcp_addr, sizeof(tcp_addr)) < 0) {
        perror("bind tcp");
        exit(EXIT_FAILURE);
    }

    memset(&udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons(UDP_PORT);

    if (bind(udp_server_fd, (struct sockaddr *)&udp_addr, sizeof(udp_addr)) < 0) {
        perror("bind udp");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("[Gateway] TCP %d / UDP %d prêts.\n", TCP_PORT, UDP_PORT);

    while (1) {
        fd_set readfds;
        int max_fd = server_fd;

        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        FD_SET(udp_server_fd, &readfds);
        if (udp_server_fd > max_fd) {
            max_fd = udp_server_fd;
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (sessions[i].in_use && sessions[i].tcp_fd > 0) {
                FD_SET(sessions[i].tcp_fd, &readfds);
                if (sessions[i].tcp_fd > max_fd) {
                    max_fd = sessions[i].tcp_fd;
                }
            }
        }

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        int activity = select(max_fd + 1, &readfds, NULL, NULL, &tv);
        if (activity < 0 && errno != EINTR) {
            perror("select");
        }

        handle_service_responses();

        if (FD_ISSET(udp_server_fd, &readfds)) {
            handle_udp_registration();
        }

        if (FD_ISSET(server_fd, &readfds)) {
            int new_socket;
            struct sockaddr_in client_addr;
            socklen_t addrlen = sizeof(client_addr);

            new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
            if (new_socket >= 0) {
                ClientSession *session = create_session(new_socket);
                if (!session) {
                    close(new_socket);
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!sessions[i].in_use || sessions[i].tcp_fd <= 0) {
                continue;
            }
            if (!FD_ISSET(sessions[i].tcp_fd, &readfds)) {
                continue;
            }

            PacketHeader header;
            char payload[BUFFER_SIZE];
            memset(payload, 0, sizeof(payload));

            if (recv_all(sessions[i].tcp_fd, &header, sizeof(header)) == -1) {
                destroy_session(&sessions[i]);
                continue;
            }

            if (header.length < sizeof(PacketHeader) || header.length > BUFFER_SIZE) {
                destroy_session(&sessions[i]);
                continue;
            }

            size_t payload_len = header.length - sizeof(PacketHeader);
            if (payload_len > 0 && recv_all(sessions[i].tcp_fd, payload, payload_len) == -1) {
                destroy_session(&sessions[i]);
                continue;
            }

            route_packet(&sessions[i], &header, payload);
        }
    }
}
