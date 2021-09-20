#ifndef _WORKERS_H_
#define _WORKERS_H_

#include <pthread.h>

typedef struct workers_s *workers_t;

workers_t workers_init(int n_workers);
void workers_delete(workers_t workers);
void workers_start(workers_t workers, void *(*routine)(void *));
void workers_mainloop(workers_t workers);
int workers_piperead(workers_t workers, void *buf, size_t nbytes);
int workers_pipewrite(workers_t workers, void *buf, size_t nbytes);
int workers_getmaxfd(workers_t workers);

#endif