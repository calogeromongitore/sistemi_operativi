#include "../include/fifo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALLOCATED_SIZE 512

#define CHECK_BOUND(__fifo, __idx) (__idx >= __fifo->memfifo && __idx < __fifo->memfifo + __fifo->length)


struct fifo_s {
    void *memfifo;
    int first;
    int last;
    size_t length;
};


static void __get_len(fifo_t fifo, size_t len, size_t startpoint, size_t *tillend, size_t *remain) {
    *tillend = startpoint + len >= fifo->length ? fifo->length - startpoint : len;
    *remain = len - *tillend;
}


fifo_t fifo_init(size_t sizebytes) {
    fifo_t fifo;

    fifo = (fifo_t)malloc(sizeof *fifo);
    fifo->memfifo = malloc(sizebytes);
    fifo->length = sizebytes;
    fifo->first = fifo->length-1;
    fifo->last = 0;

    return fifo;
}

void fifo_destroy(fifo_t fifo) {
    free(fifo->memfifo);
    free(fifo);
}

int fifo_read(fifo_t fifo, void *idx, void *buf, size_t len) {
    size_t tillend, remain;

    if (!CHECK_BOUND(fifo, idx)) {
        return -1;
    }

    __get_len(fifo, len, (size_t)idx - (size_t)fifo->memfifo, &tillend, &remain);

    memcpy(buf, idx, tillend);
    memcpy(buf + tillend, fifo->memfifo, remain);
    return 0;
}

int fifo_read_destroy(fifo_t fifo, void *readptr) {
    
    if (!CHECK_BOUND(fifo, readptr)) {
        return -1;
    }

    free(readptr);
    return 0;
}

void *fifo_enqueue(fifo_t fifo, void *buf, size_t len) {
    size_t len_tillend, len_remain;
    void *ptr;

    if (fifo->last + len > ((fifo->first + 1) % fifo->length) + fifo->length) {
        return NULL;
    }

    ptr = fifo->memfifo + fifo->last;
    __get_len(fifo, len, fifo->last, &len_tillend, &len_remain);

    memcpy(ptr, buf, len_tillend);
    memcpy(fifo->memfifo, buf + len_tillend, len_remain);

    fifo->last += len;
    fifo->last %= fifo->length;

    return ptr;
}

int fifo_dequeue(fifo_t fifo, void *dst, size_t len) {
    size_t tillend, remain, newpos;

    if (fifo->first + 1 == fifo->last) {
        return -1;
    }

    newpos = (size_t)(fifo->first + 1) % fifo->length;
    __get_len(fifo, len, newpos, &tillend, &remain);

    memcpy(dst, fifo->memfifo + newpos, tillend);
    memcpy(dst + tillend, fifo->memfifo, remain);

    fifo->first = (fifo->first + len) % fifo->length;

    return 0;
}
