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

// Chemin unique pour la file de messages globale
#define GLOBAL_MSG_QUEUE_PATH "/tmp/global_mq"
#define GLOBAL_MSG_QUEUE_ID 100

// Types de messages (mtype) pour le routage au sein de la file unique
#define MSG_TYPE_AUTH 1
#define MSG_TYPE_MATCHMAKING 2
#define MSG_TYPE_GAMEWORKER 3
#define MSG_TYPE_CHAT 4
#define MSG_TYPE_GATEWAY 5
#define MSG_TYPE_TOURNAMENT 6
#define MSG_TYPE_STORAGE 7

#endif // IPC_KEYS_H
