#include "ipc_utils.h"
#include <string.h>

/**
 * --- FILES DE MESSAGES (Message Queues) ---
 */

// Initialise ou accède à une file de messages
int ipc_msg_get(key_t key) {
    // msgget crée la file avec les permissions 0666 (lecture/écriture pour tous)
    // IPC_CREAT indique de créer la file si elle n'existe pas encore
    int msqid = msgget(key, 0666 | IPC_CREAT);
    if (msqid == -1) {
        perror("ipc_msg_get failed");
    }
    return msqid;
}

// Envoie un message dans la file spécifiée
int ipc_msg_send(int msqid, const void *msg, size_t size, long type) {
    if (size > MAX_MSG_SIZE) {
        fprintf(stderr, "Message size too large (%zu > %d)\n", size, MAX_MSG_SIZE);
        return -1;
    }

    // On utilise une structure temporaire respectant le format attendu par msgsnd
    struct {
        long mtype;
        char mtext[MAX_MSG_SIZE];
    } tmp_msg;

    tmp_msg.mtype = type;
    memcpy(tmp_msg.mtext, msg, size);

    // msgsnd : l'appel système pour envoyer le message
    // On passe size (taille des données réelles) et non la taille totale de la structure
    if (msgsnd(msqid, &tmp_msg, size, 0) == -1) {
        perror("ipc_msg_send failed");
        return -1;
    }
    return 0;
}

// Reçoit un message de la file spécifiée
int ipc_msg_receive(int msqid, void *msg, size_t size, long type) {
    struct {
        long mtype;
        char mtext[MAX_MSG_SIZE];
    } tmp_msg;

    // msgrcv : l'appel système pour lire un message
    // type : permet de filtrer les messages par mtype (0 = premier message disponible)
    ssize_t nbytes = msgrcv(msqid, &tmp_msg, MAX_MSG_SIZE, type, 0);
    if (nbytes == -1) {
        if (errno != ENOMSG) { // On ignore l'erreur si c'est juste qu'il n'y a pas de message
            perror("ipc_msg_receive failed");
        }
        return -1;
    }

    // On copie les données reçues dans le buffer de destination
    size_t copy_size = (size < (size_t)nbytes) ? size : (size_t)nbytes;
    memcpy(msg, tmp_msg.mtext, copy_size);
    return (int)nbytes;
}

// Supprime la file de messages du système Linux
int ipc_msg_delete(int msqid) {
    if (msgctl(msqid, IPC_RMID, NULL) == -1) {
        perror("ipc_msg_delete failed");
        return -1;
    }
    return 0;
}

/**
 * --- MÉMOIRE PARTAGÉE (Shared Memory) ---
 */

// Crée ou accède à un segment de mémoire partagée
int ipc_shm_get(key_t key, size_t size) {
    int shmid = shmget(key, size, 0666 | IPC_CREAT);
    if (shmid == -1) {
        perror("ipc_shm_get failed");
    }
    return shmid;
}

// Mappe la mémoire partagée dans l'espace d'adressage du processus
void* ipc_shm_attach(int shmid) {
    void *shmaddr = shmat(shmid, NULL, 0);
    if (shmaddr == (void *) -1) {
        perror("ipc_shm_attach failed");
        return NULL;
    }
    return shmaddr;
}

// Détache la mémoire (le segment existe toujours mais n'est plus accessible par ce processus)
int ipc_shm_detach(const void *shmaddr) {
    if (shmdt(shmaddr) == -1) {
        perror("ipc_shm_detach failed");
        return -1;
    }
    return 0;
}

// Supprime le segment de mémoire partagée du système
int ipc_shm_delete(int shmid) {
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("ipc_shm_delete failed");
        return -1;
    }
    return 0;
}

/**
 * --- UTILITAIRES ---
 */

// Utilise ftok pour transformer un chemin et un ID en clé unique système
key_t ipc_get_key(const char *pathname, int proj_id) {
    key_t key = ftok(pathname, proj_id);
    if (key == -1) {
        perror("ipc_get_key (ftok) failed");
    }
    return key;
}
