#include "../include/storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include "../include/fifo.h"
#include "../include/list.h"
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
    char *filename;
};

struct node {
    char *filename;
    size_t filename_length;
    void *locptr;
    int size;
    struct lockstat locked;
    int openby[32];
    int openby_length;
};

struct storage_s {
    int maxfiles;
    size_t totstorage;
    size_t actual_storage;
    int actual_nfiles;
    struct node *memory;
    list_t tempopen;
    fifo_t fifo;
    fifo_t fiforet;
    pthread_mutex_t storagemtx;
};

static const struct lockstat lockstat_default = { .index = -1, .locked = 0 };

static void ___remove(storage_t storage, int index, char dofree) {
    NODE_SETNULL(storage->memory[index]);

    if (dofree) {
        free(storage->memory[index].locptr);
        free(storage->memory[index].filename);
    }

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

    if (inode == NULL) {
        return E_GENERIC;
    }

    for (i = 0; i < inode->openby_length && inode->openby[i] != clientid; i++);    

    return i >= inode->openby_length ? E_NOPEN : i;
}

static inline char ___is_accessible(int clientid, struct node *inode) {
    return ___is_openedby(clientid, inode) || !inode->locked.locked || (inode->locked.locked && inode->openby[inode->locked.index] == clientid);
}

static int ___full_remove(storage_t storage, size_t size, struct node *inode) {
    int i, firsti, nfiles, inode_i;

    i = firsti = inode_i = -1;
    nfiles = storage->actual_nfiles;

    while (nfiles > 0 && storage->actual_storage + size > storage->totstorage || (!inode && storage->actual_nfiles == storage->maxfiles)) {

        fifo_dequeue(storage->fifo, (void *)&i, sizeof i);

        if (inode != &storage->memory[i]) {
            fifo_enqueue(storage->fiforet, &storage->memory[i], sizeof storage->memory[i]);
            ___remove(storage, i, 0);
        } else {
            inode_i = i;
        }

        if (firsti == -1) {
            firsti = i;
        }

        nfiles--;
    }

    if (inode_i >= 0) {
        fifo_enqueue(storage->fifo, (void *)&i, sizeof i);
    }

    return i;
}

storage_t storage_init(size_t totstorage, int maxfiles) {
    storage_t storage;
    int i;

    storage = (storage_t)malloc(sizeof *storage);
    storage->memory = (struct node *)malloc(maxfiles * sizeof *storage->memory);
    storage->fifo = fifo_init(maxfiles * sizeof(int));
    storage->fiforet = fifo_init(maxfiles * sizeof(struct node));
    storage->totstorage = totstorage;
    storage->maxfiles = maxfiles;
    storage->actual_nfiles = 0;
    storage->tempopen = list_init();

    pthread_mutex_init(&storage->storagemtx, NULL);

    for (i = 0; i < maxfiles; i++) {
        NODE_SETNULL(storage->memory[i]);
    }

    return storage;
}

void storage_destroy(storage_t storage) {
    free(storage->memory);
    fifo_destroy(storage->fifo);
    fifo_destroy(storage->fiforet);
    list_destroy(storage->tempopen);
    pthread_mutex_destroy(&storage->storagemtx);
    free(storage);
}

void storage_getremoved(storage_t storage, size_t *n, void **data, size_t *datasize, char *filename, size_t *filenamesize) {
    struct node inode;

    *n = fifo_usedspace(storage->fiforet) / sizeof(struct node);
    if (*n) {
        fifo_dequeue(storage->fiforet, (void *)&inode, sizeof inode);

        *data = inode.locptr;
        *datasize = inode.size;

        strcpy(filename, inode.filename);
        *filenamesize = inode.filename_length;
        
        free(inode.filename);
    }

}

void storage_insert(storage_t storage, void *buf, size_t size, char *filename) {
    int i;

    pthread_mutex_lock(&storage->storagemtx);

    while (fifo_usedspace(storage->fiforet)) {
        fifo_dequeue(storage->fiforet, NULL, sizeof(struct node));
    }

    if ((i = ___full_remove(storage, size, NULL)) < 0) {
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

int storage_read(storage_t storage, int clientid, const char *filename, void **buf, size_t *size) {
    struct node *inode;
    int retval;

    pthread_mutex_lock(&storage->storagemtx);

    *size = 0;
    retval = E_GENERIC;

    inode = ___get_inode(storage, filename);
    if (inode == NULL) {
        retval = E_NEXISTS;
    } else if (___is_openedby(clientid, inode) == E_NOPEN) {
        retval = E_NOPEN;
    } else if (___is_accessible(clientid, inode)) {
        *size = inode->size;
        *buf = inode->locptr;
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
    if (inode == NULL) {
        retval = E_NEXISTS;
    } else if (___is_openedby(clientid, inode) == E_NOPEN) {
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
    struct openstat *open;

    pthread_mutex_lock(&storage->storagemtx);

    retval = E_ITSOK;
    inode = ___get_inode(storage, filename);

    if (inode == NULL) {
        if (!(flags & O_CREATE)) {
            retval = E_NEXISTS;
        } else if (flags & O_LOCK) {
            open = (struct openstat *)malloc(sizeof *open);
            open->filename = malloc((strlen(filename) + 1) * sizeof *filename);
            open->clientid = clientid;

            strcpy(open->filename, filename);
            list_append(storage->tempopen, open);
        }
    } else if (flags & O_CREATE) {
        retval = E_EXISTS;
    } else if (___is_openedby(clientid, inode) == E_NOPEN) {

        if (inode->locked.locked && inode->openby[inode->locked.index] != clientid) {
            retval = A_LKWAIT;
        } else {
            inode->openby[inode->openby_length] = clientid;

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
        ___remove(storage, i, 1);
    }

    pthread_mutex_unlock(&storage->storagemtx);
    return retval;
}

static char ___searchfn(void *val, void *what) {
    struct openstat *t1, *t2;
    char ret;

    t1 = (struct openstat *)val;
    t2 = (struct openstat *)what;

    ret = t1->clientid == t2->clientid && !strcmp(t1->filename, t2->filename);

    if (ret) t1->filename = t2->filename;

    return ret;
}

int storage_write(storage_t storage, int clientid, void *buf, size_t size, char *filename) {
    void *opened;
    int retval;
    struct openstat t1;
    struct node *inode;

    t1.filename = filename;
    t1.clientid = clientid;

    retval = E_ITSOK;
    opened = list_search(storage->tempopen, &t1, ___searchfn);

    if (opened == NULL) {
        retval = E_NEXISTS;
    } else if (size > storage->totstorage) {
        retval E_NOSPACE;
    } else {
        // free(t1.filename); TODO
        storage_insert(storage, buf, size, filename);
        list_delete(storage->tempopen, opened);

        retval = storage_open(storage, clientid, filename, O_LOCK);
    }

    return retval;
}

int storage_append(storage_t storage, int clientid, void *buf, size_t size, char *filename) {
    int retval;
    struct node *inode;
    void *copyloc;

    retval = E_ITSOK;
    inode = ___get_inode(storage, filename);

    if (inode == NULL) {
        retval = E_NEXISTS;
    } else if (!___is_accessible(clientid, inode)) {
        retval = E_DENIED;
    } else if (inode->size + size > storage->totstorage) {
        retval = E_NOSPACE;
    } else {
        // free(t1.filename); TODO

        // making free space
        ___full_remove(storage, size, inode);

        // original chunk copy
        copyloc = malloc(inode->size + size);
        memcpy(copyloc, inode->locptr, inode->size);

        // new chunk append
        memcpy(copyloc + inode->size, buf, size);

        // original chunk pointer remove & replace
        free(inode->locptr);
        inode->locptr = copyloc;
        inode->size += size;
        storage->actual_storage += size;

    }

    return retval;

}