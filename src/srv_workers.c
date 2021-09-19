#include "../include/workers.h"

#include <pthread.h>
#include <stdlib.h>

struct workers_s {
    pthread_t *workers;
    int n_workers;
    pthread_mutex_t mtx;
    void *args;
    pthread_mutex_t args_mtx;
};

workers_t workers_init(int n_workers, void *args) {
    workers_t workers;

    workers = (workers_t)malloc(sizeof *workers);
    workers->workers = malloc(n_workers * sizeof(pthread_t));
    workers->args = args;
    workers->n_workers = n_workers;
    
    pthread_mutex_init(&workers->mtx, NULL);
    pthread_mutex_init(&workers->args_mtx, NULL);

    return workers;
}

void workers_delete(workers_t workers) {

    pthread_mutex_destroy(&workers->args_mtx);
    pthread_mutex_destroy(&workers->mtx);

    free(workers->workers);
    free(workers);
}

void workers_start(workers_t workers, void *(*routine)(void *)) {
    int i;

    // after init, the mutex is at 1. After lock it's at 0
    pthread_mutex_lock(&workers->mtx);

    for (i = 0; i < workers->n_workers; i++) {
        pthread_create(&workers->workers[i], NULL, routine, (void *)workers);
    }

}

void workers_mainloop(workers_t workers) {
    int i;

    for (i = 0; i < workers->n_workers; i++) {
        pthread_join(workers->workers[i], NULL);
    }

}

pthread_mutex_t *get_mtxptr(workers_t workers) {
    return &workers->mtx;
}

void *get_args(workers_t workers) {
    return workers->args;
}

void workers_wakeup(workers_t workers) {
    pthread_mutex_unlock(&workers->mtx);
}

void workers_args_lock(workers_t workers) {
    pthread_mutex_lock(&workers->args_mtx);
}

void workers_args_unlock(workers_t workers) {
    pthread_mutex_unlock(&workers->args_mtx);
}
