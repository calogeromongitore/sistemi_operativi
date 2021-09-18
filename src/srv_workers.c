#include "../include/workers.h"

#include <pthread.h>
#include <stdlib.h>

struct workers_s {
    pthread_t *workers;
    int n_workers;
    pthread_mutex_t mtx;
    void *commonptr;
};

workers_t workers_init(int n_workers, void *commonptr) {
    workers_t workers;

    workers = (workers_t)malloc(sizeof(struct workers_s));
    workers->workers = (pthread_t *)malloc(n_workers * sizeof(pthread_t));
    workers->commonptr = commonptr;
    workers->n_workers = n_workers;
    
    pthread_mutex_init(&workers->mtx, NULL);

    return workers;
}

void workers_delete(workers_t workers) {

    pthread_mutex_destroy(&workers->mtx);

    free(workers->workers);
    free(workers);
}

void workers_start(workers_t workers, void *(*routine)(void *)) {
    int i;

    // after init, the mutex is at 1. After lock it's at 0
    pthread_mutex_lock(&workers->mtx);

    for (i = 0; i < workers->n_workers; i++) {
        pthread_create(&workers->workers[i], NULL, routine, &workers);
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