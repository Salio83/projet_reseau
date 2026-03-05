#include "ipc_utils.h"
#include <string.h>

/**
 * FILES DE MESSAGES (Message Queues)
 */

int ipc_msg_get(key_t key) {
    int msqid = msgget(key, 0666 | IPC_CREAT);
    if (msqid == -1) {
        perror("ipc_msg_get failed");
    }
    return msqid;
}

int ipc_msg_send(int msqid, const void *msg, size_t size, long type) {
    if (size > MAX_MSG_SIZE) {
        fprintf(stderr, "Message size too large (%zu > %d)\n", size, MAX_MSG_SIZE);
        return -1;
    }

    struct {
        long mtype;
        char mtext[MAX_MSG_SIZE];
    } tmp_msg;

    tmp_msg.mtype = type;
    memcpy(tmp_msg.mtext, msg, size);

    if (msgsnd(msqid, &tmp_msg, size, 0) == -1) {
        perror("ipc_msg_send failed");
        return -1;
    }
    return 0;
}

int ipc_msg_receive(int msqid, void *msg, size_t size, long type) {
    struct {
        long mtype;
        char mtext[MAX_MSG_SIZE];
    } tmp_msg;

    // msgrcv retourne le nombre d'octets copiés dans mtext
    ssize_t nbytes = msgrcv(msqid, &tmp_msg, MAX_MSG_SIZE, type, 0);
    if (nbytes == -1) {
        if (errno != ENOMSG) {
            perror("ipc_msg_receive failed");
        }
        return -1;
    }

    size_t copy_size = (size < (size_t)nbytes) ? size : (size_t)nbytes;
    memcpy(msg, tmp_msg.mtext, copy_size);
    return (int)nbytes;
}

int ipc_msg_delete(int msqid) {
    if (msgctl(msqid, IPC_RMID, NULL) == -1) {
        perror("ipc_msg_delete failed");
        return -1;
    }
    return 0;
}

/**
 * MÉMOIRE PARTAGÉE (Shared Memory)
 */

int ipc_shm_get(key_t key, size_t size) {
    int shmid = shmget(key, size, 0666 | IPC_CREAT);
    if (shmid == -1) {
        perror("ipc_shm_get failed");
    }
    return shmid;
}

void* ipc_shm_attach(int shmid) {
    void *shmaddr = shmat(shmid, NULL, 0);
    if (shmaddr == (void *) -1) {
        perror("ipc_shm_attach failed");
        return NULL;
    }
    return shmaddr;
}

int ipc_shm_detach(const void *shmaddr) {
    if (shmdt(shmaddr) == -1) {
        perror("ipc_shm_detach failed");
        return -1;
    }
    return 0;
}

int ipc_shm_delete(int shmid) {
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("ipc_shm_delete failed");
        return -1;
    }
    return 0;
}

/**
 * UTILITAIRES
 */

key_t ipc_get_key(const char *pathname, int proj_id) {
    key_t key = ftok(pathname, proj_id);
    if (key == -1) {
        perror("ipc_get_key (ftok) failed");
    }
    return key;
}
