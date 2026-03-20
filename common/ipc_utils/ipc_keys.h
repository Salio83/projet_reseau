#ifndef IPC_KEYS_H
#define IPC_KEYS_H

/**
 * Clés ftok pour les files de messages IPC
 */

#define AUTH_MSG_QUEUE_PATH "/tmp/auth_mq"
#define MATCHMAKING_MSG_QUEUE_PATH "/tmp/matchmaker_mq"
#define GAMEWORKER_MSG_QUEUE_PATH "/tmp/gameworker_mq"
#define CHAT_MSG_QUEUE_PATH "/tmp/chat_mq"
#define GATEWAY_MSG_QUEUE_PATH "/tmp/gateway_mq" // Pour que les services répondent au gateway

#define AUTH_MSG_QUEUE_ID 1
#define MATCHMAKING_MSG_QUEUE_ID 2
#define GAMEWORKER_MSG_QUEUE_ID 3
#define CHAT_MSG_QUEUE_ID 4
#define GATEWAY_MSG_QUEUE_ID 5

#endif // IPC_KEYS_H
