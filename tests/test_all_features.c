#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>

#include "../common/ipc_utils/ipc_utils.h"
#include "../common/ipc_utils/ipc_keys.h"
#include "../common/network_models/packet_types.h"

#define TEST_KEY_PATH "/tmp/test_ipc_key"
#define TEST_PROJ_ID 42
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

// Colors for output
#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define RESET "\033[0m"

void test_ipc_shm() {
    printf("--- Test IPC Shared Memory ---\n");
    
    // Create a dummy file for ftok
    FILE *fp = fopen(TEST_KEY_PATH, "w");
    if (fp) fclose(fp);

    key_t key = ipc_get_key(TEST_KEY_PATH, TEST_PROJ_ID);
    if (key == -1) {
        printf(RED "[FAILED] ipc_get_key\n" RESET);
        return;
    }

    size_t shm_size = 1024;
    int shmid = ipc_shm_get(key, shm_size);
    if (shmid == -1) {
        printf(RED "[FAILED] ipc_shm_get\n" RESET);
        return;
    }
    printf(GREEN "[PASSED] ipc_shm_get\n" RESET);

    char *shm_addr = (char *)ipc_shm_attach(shmid);
    if (shm_addr == NULL) {
        printf(RED "[FAILED] ipc_shm_attach\n" RESET);
        return;
    }
    printf(GREEN "[PASSED] ipc_shm_attach\n" RESET);

    const char *test_msg = "Hello from Shared Memory!";
    strcpy(shm_addr, test_msg);
    if (strcmp(shm_addr, test_msg) == 0) {
        printf(GREEN "[PASSED] Write/Read from SHM\n" RESET);
    } else {
        printf(RED "[FAILED] Write/Read from SHM\n" RESET);
    }

    if (ipc_shm_detach(shm_addr) == -1) {
        printf(RED "[FAILED] ipc_shm_detach\n" RESET);
    } else {
        printf(GREEN "[PASSED] ipc_shm_detach\n" RESET);
    }

    if (ipc_shm_delete(shmid) == -1) {
        printf(RED "[FAILED] ipc_shm_delete\n" RESET);
    } else {
        printf(GREEN "[PASSED] ipc_shm_delete\n" RESET);
    }
}

void test_ipc_msg() {
    printf("\n--- Test IPC Message Queues ---\n");

    key_t key = ipc_get_key(TEST_KEY_PATH, TEST_PROJ_ID + 1);
    if (key == -1) {
        printf(RED "[FAILED] ipc_get_key\n" RESET);
        return;
    }

    int msqid = ipc_msg_get(key);
    if (msqid == -1) {
        printf(RED "[FAILED] ipc_msg_get\n" RESET);
        return;
    }
    printf(GREEN "[PASSED] ipc_msg_get\n" RESET);

    const char *test_msg = "Message Queue Test Packet";
    if (ipc_msg_send(msqid, test_msg, strlen(test_msg) + 1, 1) == -1) {
        printf(RED "[FAILED] ipc_msg_send\n" RESET);
    } else {
        printf(GREEN "[PASSED] ipc_msg_send\n" RESET);
    }

    char buffer[MAX_MSG_SIZE];
    int nbytes = ipc_msg_receive(msqid, buffer, MAX_MSG_SIZE, 1);
    if (nbytes == -1) {
        printf(RED "[FAILED] ipc_msg_receive\n" RESET);
    } else {
        if (strcmp(buffer, test_msg) == 0) {
            printf(GREEN "[PASSED] ipc_msg_receive (content correct)\n" RESET);
        } else {
            printf(RED "[FAILED] ipc_msg_receive (content mismatch: %s)\n" RESET, buffer);
        }
    }

    if (ipc_msg_delete(msqid) == -1) {
        printf(RED "[FAILED] ipc_msg_delete\n" RESET);
    } else {
        printf(GREEN "[PASSED] ipc_msg_delete\n" RESET);
    }
}

void test_network_auth() {
    printf("\n--- Test Network (Auth Workflow) ---\n");
    printf("(Note: requires server_app and auth_app to be running)\n");

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf(RED "[FAILED] Socket creation\n" RESET);
        return;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        printf(RED "[FAILED] Address conversion\n" RESET);
        close(sock);
        return;
    }

    struct timeval tv;
    tv.tv_sec = 2; // 2 seconds timeout
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf(RED "[SKIPPED] Network test: Could not connect to gateway on %s:%d\n" RESET, SERVER_IP, SERVER_PORT);
        close(sock);
        return;
    }
    printf(GREEN "[PASSED] Connected to Gateway\n" RESET);

    PacketHeader header;
    header.type = PACKET_AUTH_REQ;
    header.length = sizeof(PacketHeader) + sizeof(AuthRequest);
    header.client_id = 0;

    AuthRequest auth;
    strncpy(auth.username, "TestRunner", 32);
    strncpy(auth.password_hash, "test_hash", 64);

    if (send(sock, &header, sizeof(PacketHeader), 0) < 0 ||
        send(sock, &auth, sizeof(AuthRequest), 0) < 0) {
        printf(RED "[FAILED] Sending AuthRequest\n" RESET);
        close(sock);
        return;
    }
    printf(GREEN "[PASSED] AuthRequest sent\n" RESET);

    char buffer[2048];
    int valread = read(sock, buffer, 2048);
    if (valread > 0) {
        PacketHeader* res_header = (PacketHeader*)buffer;
        printf(GREEN "[PASSED] Response received! Type: %d\n" RESET, res_header->type);
        if (res_header->type == PACKET_AUTH_REQ) {
            AuthRequest* res_auth = (AuthRequest*)(buffer + sizeof(PacketHeader));
            if (strcmp(res_auth->username, "TestRunner") == 0) {
                printf(GREEN "[PASSED] Auth confirmation for %s\n" RESET, res_auth->username);
            } else {
                printf(RED "[FAILED] Auth response content mismatch\n" RESET);
            }
        }
    } else {
        printf(RED "[FAILED] No response from gateway (timeout)\n" RESET);
    }

    close(sock);
}

int main(int argc, char *argv[]) {
    printf("=== WORLD POLYTECH CHESS TEST SUITE ===\n\n");
    
    test_ipc_shm();
    test_ipc_msg();
    test_network_auth();

    printf("\nTests finished.\n");
    return 0;
}
