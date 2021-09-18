#ifndef _WORKERS_H_
#define _WORKERS_H_

#include <pthread.h>

typedef struct workers_s *workers_t;

workers_t workers_init(int n_workers, void *commonptr);
void workers_delete(workers_t workers);
void workers_start(workers_t workers, void *(*routine)(void *));
void workers_mainloop(workers_t workers);
pthread_mutex_t *get_mtxptr(workers_t workers);

#endif