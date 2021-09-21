#ifndef _FIFO_H_
#define _FIFO_H_

#include <stdio.h>

typedef struct fifo_s *fifo_t;
fifo_t fifo_init(size_t sizebytes);
void fifo_destroy(fifo_t fifo);
int fifo_read(fifo_t fifo, void *idx, void *buf, size_t len);
int fifo_read_destroy(fifo_t fifo, void *readptr);
void *fifo_enqueue(fifo_t fifo, void *buf, size_t len);
int fifo_dequeue(fifo_t fifo, void *dst, size_t len);

#endif