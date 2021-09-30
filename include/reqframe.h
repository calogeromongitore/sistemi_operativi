#ifndef _REQ_FRAME_H_
#define _REQ_FRAME_H_

#include <stdio.h>

typedef enum {
    REQ_OPEN = 0,
    REQ_CLOSECONN = 1,
    REQ_READ = 2,
    REQ_GETSIZ = 3,
    REQ_FAILED = 4,
    REQ_SUCCESS = 5,
    REQ_CLOSEFILE = 6,
    REQ_LOCK = 7,
    REQ_UNLOCK = 8,
    REQ_REMOVE = 9,
    REQ_WRITE = 10,
    REQ_APPEND = 11
} reqcode_t;

typedef enum {
    PARAM_SEP = (char)-1,
    PARAM_N = (char)0,
    PARAM_DIRNAME,
    PARAM_PATHNAME,
    PARAM_BUF,
    PARAM_SIZE,
    PARAM_FLAGS
} param_t;

struct reqcall {
    const char *pathname;
    int flags;
    int N;
    const char *diname;
    void *buf;
    size_t size;
};

void reqcall_default(struct reqcall *req);
void prepareRequest(char *buf, size_t *size, reqcode_t req, struct reqcall *reqc);

#endif