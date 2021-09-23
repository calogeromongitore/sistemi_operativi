#include "../include/storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include "../include/fifo.h"
#include "../include/common.h"

#define NODE_SETNULL(node) ((node).locptr = NULL)
#define NODE_ISNULL(node) ((node).locptr == NULL)
#define NODE_FOREACH(storage, inode) for ((inode) = (storage)->memory; (inode) < (storage)->memory + (storage)->maxfiles; (inode)++)


struct lockstat {
    int index;
    char locked;
};

struct openstat {
    int clientid;
    char write;
};

struct node {
    char *filename;
    size_t filename_length;
    void *locptr;
    int size;
    struct lockstat locked;
    struct openstat openby[32];
    int openby_length;
};

struct storage_s {
    int maxfiles;
    size_t totstorage;
    size_t actual_storage;
    int actual_nfiles;
    struct node *memory;
    fifo_t fifo;
    pthread_mutex_t storagemtx;
};

static const struct lockstat lockstat_default = { .index = -1, .locked = 0 };

static void ___remove(storage_t storage, int index) {
    NODE_SETNULL(storage->memory[index]);
    free(storage->memory[index].locptr);
    free(storage->memory[index].filename);
    storage->actual_storage -= storage->memory[index].size;
    storage->actual_nfiles--;
}

static struct node * ___get_inode(storage_t storage, const char *filename) {
    struct node *inode;

    NODE_FOREACH(storage, inode) {
        if (!NODE_ISNULL(*inode) && !strcmp(inode->filename, filename)) {
            return inode;
        }
    }

    return NULL;
}

static int ___is_openedby(int clientid, struct node *inode) {
    int i;

    for (i = 0; i < inode->openby_length && inode->openby[i].clientid != clientid; i++);    

    return i >= inode->openby_length ? E_NOPEN : i;
}

static inline char ___is_accessible(int clientid, struct node *inode) {
    return ___is_openedby(clientid, inode) || !inode->locked.locked || (inode->locked.locked && inode->openby[inode->locked.index].clientid == clientid);
}

storage_t storage_init(size_t totstorage, int maxfiles) {
    storage_t storage;
    int i;

    storage = (storage_t)malloc(sizeof *storage);
    storage->memory = (struct node *)malloc(maxfiles * sizeof *storage->memory);
    storage->fifo = fifo_init(maxfiles * sizeof(int));
    storage->totstorage = totstorage;
    storage->maxfiles = maxfiles;
    storage->actual_nfiles = 0;

    pthread_mutex_init(&storage->storagemtx, NULL);

    for (i = 0; i < maxfiles; i++) {
        NODE_SETNULL(storage->memory[i]);
    }

    return storage;
}

void storage_destroy(storage_t storage) {
    free(storage->memory);
    fifo_destroy(storage->fifo);
    pthread_mutex_destroy(&storage->storagemtx);
    free(storage);
}

void storage_insert(storage_t storage, void *buf, size_t size, char *filename, void *bufret, size_t *sizeret) {
    int i;

    pthread_mutex_lock(&storage->storagemtx);

    i = 0;
    *sizeret = 0;

    if (storage->actual_storage + size > storage->totstorage || storage->actual_nfiles == storage->maxfiles) {

        fifo_dequeue(storage->fifo, (void *)&i, sizeof i);

        memcpy(bufret, storage->memory[i].locptr, storage->memory[i].size);
        *sizeret = storage->memory[i].size;

        ___remove(storage, i);

    } else {
        for (i = 0; i < storage->maxfiles && !NODE_ISNULL(storage->memory[i]); i++);
    }

    storage->memory[i].filename_length = strlen(filename);
    storage->memory[i].filename = (char *)malloc((storage->memory[i].filename_length + 1) * sizeof *storage->memory[i].filename);
    storage->memory[i].locked = lockstat_default;
    storage->memory[i].size = size;
    storage->memory[i].locptr = malloc(size);

    strcpy(storage->memory[i].filename, filename);
    memcpy(storage->memory[i].locptr, buf, size);

    fifo_enqueue(storage->fifo, (void *)&i, sizeof i);
    storage->actual_storage += size;
    storage->actual_nfiles++;

    pthread_mutex_unlock(&storage->storagemtx);
}

int storage_read(storage_t storage, int clientid, const char *filename, void *buf, size_t *size) {
    struct node *inode;
    int retval;

    pthread_mutex_lock(&storage->storagemtx);

    *size = 0;
    retval = E_GENERIC;

    inode = ___get_inode(storage, filename);
    if (___is_openedby(clientid, inode) == E_NOPEN) {
        retval = E_NOPEN;
    } else if (___is_accessible(clientid, inode)) {
        *size = inode->size;
        memcpy(buf, inode->locptr, *size);
        retval = E_ITSOK;
    } else {
        retval = E_LKNOACQ;
    }

    pthread_mutex_unlock(&storage->storagemtx);
    return retval;
}

int storage_getsize(storage_t storage, int clientid, const char *filename, size_t *size) {
    struct node *inode;
    int retval;

    pthread_mutex_lock(&storage->storagemtx);

    retval = E_GENERIC;
    *size = 0;

    inode = ___get_inode(storage, filename);
    if (___is_openedby(clientid, inode) == E_NOPEN) {
        retval = E_NOPEN;
    } else if (___is_accessible(clientid, inode)) {
        *size = inode->size;
        retval = E_ITSOK;
    } else {
        retval = E_LKNOACQ;
    }

    pthread_mutex_unlock(&storage->storagemtx);
    return retval;
}

int storage_open(storage_t storage, int clientid, const char *filename, int flags) {
    struct node *inode;
    int retval;

    pthread_mutex_lock(&storage->storagemtx);

    retval = E_ITSOK;
    inode = ___get_inode(storage, filename);

    if (inode == NULL || ___is_openedby(clientid, inode) == E_NOPEN) {

        if (inode == NULL) {
            retval = E_NEXISTS;
        } else if (inode->locked.locked && inode->openby[inode->locked.index].clientid != clientid) {
            retval = A_LKWAIT;
        } else {
            inode->openby[inode->openby_length].clientid = clientid;
            inode->openby[inode->openby_length].write = flags & O_CREATE;

            if (flags & O_LOCK) {
                inode->locked.index = inode->openby_length;
                inode->locked.locked = 1;
            }

            ++inode->openby_length;
        }

    }

    pthread_mutex_unlock(&storage->storagemtx);
    return retval;
}

int storage_close(storage_t storage, int clientid, const char *filename) {
    struct node *inode;
    int retval, i, idx;

    pthread_mutex_lock(&storage->storagemtx);

    retval = E_ITSOK;
    inode = ___get_inode(storage, filename);

    if (inode == NULL) {
        retval = E_NEXISTS;
    } else if ((idx = ___is_openedby(clientid, inode)) == E_NOPEN) {
        retval = E_NOPEN;
    } else {

        for (i = idx; i < inode->openby_length - 1; i++) {
            inode->openby[i] = inode->openby[i + 1];
        }

        if (inode->locked.locked && inode->locked.index == idx) {
            inode->locked.locked = 0;
        }

        --inode->openby_length;
    }

    pthread_mutex_unlock(&storage->storagemtx);
    return retval;
}

int storage_lock(storage_t storage, int clientid, const char *filename) {
    struct node *inode;
    int retval, i, idx;

    pthread_mutex_lock(&storage->storagemtx);

    retval = E_ITSOK;
    inode = ___get_inode(storage, filename);

    if (inode == NULL) {
        retval = E_NEXISTS;
    } else if ((idx = ___is_openedby(clientid, inode)) == E_NOPEN) {
        retval = E_NOPEN;
    } else {
        if (inode->locked.locked && inode->locked.index != idx) {
            retval = A_LKWAIT;
        } else {
            inode->locked.locked = 1;
            inode->locked.index = idx;
        }
    }

    pthread_mutex_unlock(&storage->storagemtx);
    return retval;
}

int storage_unlock(storage_t storage, int clientid, const char *filename) {
    struct node *inode;
    int retval, i, idx;

    pthread_mutex_lock(&storage->storagemtx);

    retval = E_ITSOK;
    inode = ___get_inode(storage, filename);

    if (inode == NULL) {
        retval = E_NEXISTS;
    } else if ((idx = ___is_openedby(clientid, inode)) == E_NOPEN) {
        retval = E_NOPEN;
    } else if (inode->locked.locked && inode->locked.index == idx) {
        inode->locked.locked = 0;
    }

    pthread_mutex_unlock(&storage->storagemtx);
    return retval;
}

int storage_remove(storage_t storage, int clientid, const char *filename) {
    struct node *inode;
    int retval, i, idx;

    pthread_mutex_lock(&storage->storagemtx);

    retval = E_ITSOK;
    inode = ___get_inode(storage, filename);

    if (inode == NULL) {
        retval = E_NEXISTS;
    } else if ((idx = ___is_openedby(clientid, inode)) == E_NOPEN) {
        retval = E_NOPEN;
    } else if (!___is_accessible(clientid, inode)) {
        retval = E_DENIED;
    } else if (!inode->locked.locked) {
        retval = E_LKNOACQ;
    } else {
        for (i = 0; strcmp(filename, storage->memory[i].filename); i++);
        ___remove(storage, i);
    }

    pthread_mutex_unlock(&storage->storagemtx);
    return retval;
}