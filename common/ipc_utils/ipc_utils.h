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

/**
 * @file ipc_utils.h
 * @brief Utilitaires pour la communication inter-processus (Message Queues et Shared Memory).
 */

#define MAX_MSG_SIZE 2048

/**
 * @struct ipc_msg_t
 * @brief Structure générique pour l'envoi de messages via msgsnd/msgrcv.
 */
typedef struct {
    long mtype;       // Type du message (doit être > 0, utilisé pour le filtrage)
    char mtext[MAX_MSG_SIZE]; // Données brutes du message
} ipc_msg_t;

/**
 * --- FILES DE MESSAGES (Message Queues) ---
 */

/**
 * @brief Crée ou récupère une file de messages existante.
 * @param key Clé IPC générée par ftok.
 * @return ID de la file de messages ou -1 en cas d'erreur.
 */
int ipc_msg_get(key_t key);

/**
 * @brief Envoie un bloc de données dans une file de messages.
 * @param msqid ID de la file de messages.
 * @param msg Pointeur vers les données à envoyer.
 * @param size Taille des données.
 * @param type Type de message (pour mtype).
 * @return 0 en cas de succès, -1 sinon.
 */
int ipc_msg_send(int msqid, const void *msg, size_t size, long type);

/**
 * @brief Récupère un message d'une file (bloquant par défaut).
 * @param msqid ID de la file de messages.
 * @param msg Buffer de destination.
 * @param size Taille max du buffer.
 * @param type Type de message à lire (0 pour le prochain disponible).
 * @return Nombre d'octets lus ou -1 en cas d'erreur.
 */
int ipc_msg_receive(int msqid, void *msg, size_t size, long type);

/**
 * @brief Récupère un message d'une file de manière non-bloquante.
 * @param msqid ID de la file de messages.
 * @param msg Buffer de destination.
 * @param size Taille max du buffer.
 * @param type Type de message à lire (0 pour le prochain disponible).
 * @return Nombre d'octets lus ou -1 en cas d'erreur (errno == ENOMSG si pas de message).
 */
int ipc_msg_receive_nowait(int msqid, void *msg, size_t size, long type);

/**
 * @brief Supprime définitivement une file de messages du système.
 */
int ipc_msg_delete(int msqid);

/**
 * --- MÉMOIRE PARTAGÉE (Shared Memory) ---
 */

/**
 * @brief Crée ou récupère un segment de mémoire partagée.
 */
int ipc_shm_get(key_t key, size_t size);

/**
 * @brief Attache le segment de mémoire à l'espace mémoire du processus actuel.
 * @return Pointeur vers la zone mémoire ou (void*)-1.
 */
void* ipc_shm_attach(int shmid);

/**
 * @brief Détache le segment de mémoire du processus actuel.
 */
int ipc_shm_detach(const void *shmaddr);

/**
 * @brief Supprime définitivement le segment de mémoire partagée du système.
 */
int ipc_shm_delete(int shmid);

/**
 * --- UTILITAIRES ---
 */

/**
 * @brief Génère une clé unique via ftok à partir d'un fichier et d'un ID.
 */
key_t ipc_get_key(const char *pathname, int proj_id);

#endif // IPC_UTILS_H
