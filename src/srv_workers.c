#include "../include/workers.h"

#include <pthread.h>
#include <stdlib.h>
#include <semaphore.h>
#include <unistd.h>

#include "../include/common.h"

enum {
    PIPE_RFD,
    PIPE_WFD
};

struct workers_s {
    pthread_t *workers;
    int n_workers;
    int pipefds[2];
    pthread_mutex_t piperd_mtx;
    // int pipewbfds[2];
    // pthread_mutex_t pipewrb_mtx;
};

workers_t workers_init(int n_workers) {
    workers_t workers;

    workers = (workers_t)malloc(sizeof *workers);
    workers->workers = malloc(n_workers * sizeof(pthread_t));
    workers->n_workers = n_workers;
    
    pipe(workers->pipefds);
    // pipe(workers->pipewbfds);
    pthread_mutex_init(&workers->piperd_mtx, NULL);
    // pthread_mutex_init(&workers->pipewrb_mtx, NULL);

    return workers;
}

void workers_delete(workers_t workers) {

    pthread_mutex_destroy(&workers->piperd_mtx);
    // pthread_mutex_destroy(&workers->pipewrb_mtx);

    close(workers->pipefds[PIPE_RFD]);
    close(workers->pipefds[PIPE_WFD]);
    // close(workers->pipewbfds[PIPE_RFD]);
    // close(workers->pipewbfds[PIPE_WFD]);

    free(workers->workers);
    free(workers);
}

void workers_start(workers_t workers, void *(*routine)(void *)) {
    int i;

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

int workers_piperead(workers_t workers, void *buf, size_t nbytes) {
    int retval;

    pthread_mutex_lock(&workers->piperd_mtx);
    retval = read(workers->pipefds[PIPE_RFD], buf, nbytes);
    pthread_mutex_unlock(&workers->piperd_mtx);

    return retval;
}

int workers_pipewrite(workers_t workers, void *buf, size_t nbytes) {
    return write(workers->pipefds[PIPE_WFD], buf, nbytes);
}

// int workers_pipereadback(workers_t workers, void *buf, size_t nbytes) {
//     return read(workers->pipewbfds[PIPE_RFD], buf, nbytes);
// }

// int workers_pipewriteback(workers_t workers, void *buf, size_t nbytes) {
//     int retval;

//     pthread_mutex_lock(&workers->pipewrb_mtx);
//     retval = write(workers->pipefds[PIPE_WFD], buf, nbytes);
//     pthread_mutex_unlock(&workers->pipewrb_mtx);

//     return retval;
// }

int workers_getmaxfd(workers_t workers) {
    return workers->pipefds[0] > workers->pipefds[1] ? workers->pipefds[0] : workers->pipefds[1];
}
