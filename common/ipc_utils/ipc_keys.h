#ifndef IPC_KEYS_H
#define IPC_KEYS_H

/**
 * @file ipc_keys.h
 * @brief Définition des clés et chemins pour la communication inter-processus (IPC).
 * 
 * Ce fichier centralise les chemins de fichiers et les identifiants de projet
 * utilisés par la fonction ftok() pour générer des clés IPC uniques.
 * Ces clés permettent aux différents services (Auth, Matchmaker, GameWorker, etc.)
 * de s'identifier et de communiquer via des files de messages Linux.
 */

// Chemins utilisés pour générer les clés ftok (doivent pointer vers des fichiers existants)
#define AUTH_MSG_QUEUE_PATH "/tmp/auth_mq"
#define MATCHMAKING_MSG_QUEUE_PATH "/tmp/matchmaker_mq"
#define GAMEWORKER_MSG_QUEUE_PATH "/tmp/gameworker_mq"
#define CHAT_MSG_QUEUE_PATH "/tmp/chat_mq"
#define GATEWAY_MSG_QUEUE_PATH "/tmp/gateway_mq" // File de retour pour les réponses vers le Gateway

// Identifiants de projet (proj_id) pour différencier les files sur un même chemin
#define AUTH_MSG_QUEUE_ID 1
#define MATCHMAKING_MSG_QUEUE_ID 2
#define GAMEWORKER_MSG_QUEUE_ID 3
#define CHAT_MSG_QUEUE_ID 4
#define GATEWAY_MSG_QUEUE_ID 5

#endif // IPC_KEYS_H
