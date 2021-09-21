#include "../include/storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include "../include/fifo.h"

#define NODE_SETNULL(node) (node.locptr = NULL)
#define NODE_ISNULL(node) (node.locptr == NULL)

struct node {
    char *filename;
    size_t filename_length;
    void *locptr;
    int size;
    char locked;
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

static void ___remove(storage_t storage, int index) {

    NODE_SETNULL(storage->memory[index]);
    free(storage->memory[index].locptr);
    free(storage->memory[index].filename);
    storage->actual_storage -= storage->memory[index].size;
    storage->actual_nfiles--;

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
    storage->memory[i].locked = 0;
    storage->memory[i].size = size;
    storage->memory[i].locptr = malloc(size);

    strcpy(storage->memory[i].filename, filename);
    memcpy(storage->memory[i].locptr, buf, size);

    fifo_enqueue(storage->fifo, (void *)&i, sizeof i);
    storage->actual_storage += size;
    storage->actual_nfiles++;

    pthread_mutex_unlock(&storage->storagemtx);
}
