#ifndef IPC_UTILS_H
#define IPC_UTILS_H

#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define MAX_MSG_SIZE 2048

/**
 * Structure générique pour les messages
 */
typedef struct {
    long mtype;       // Type du message (doit être > 0)
    char mtext[MAX_MSG_SIZE]; // Buffer de données
} ipc_msg_t;

/**
 * FILES DE MESSAGES (Message Queues)
 */

// Crée ou récupère une file de messages
int ipc_msg_get(key_t key);

// Envoie un message dans la file
int ipc_msg_send(int msqid, const void *msg, size_t size, long type);

// Reçoit un message de la file (retourne le nombre d'octets lus ou -1 en cas d'erreur)
int ipc_msg_receive(int msqid, void *msg, size_t size, long type);

// Supprime la file de messages
int ipc_msg_delete(int msqid);

/**
 * MÉMOIRE PARTAGÉE (Shared Memory)
 */

// Crée ou récupère un segment de mémoire partagée
int ipc_shm_get(key_t key, size_t size);

// Attache le segment de mémoire partagée à l'espace d'adressage du processus
void* ipc_shm_attach(int shmid);

// Détache le segment de mémoire partagée
int ipc_shm_detach(const void *shmaddr);

// Supprime le segment de mémoire partagée
int ipc_shm_delete(int shmid);

/**
 * UTILITAIRES
 */

// Génère une clé IPC à partir d'un chemin et d'un identifiant
key_t ipc_get_key(const char *pathname, int proj_id);

#endif // IPC_UTILS_H
