#ifndef _REQ_FRAME_H_
#define _REQ_FRAME_H_

#include <stdio.h>

typedef enum {
    REQ_OPEN = (char)0,
    REQ_CLOSECONN = (char)1,
    REQ_READ = (char)2,
    REQ_GETSIZ = (char)3,
    REQ_FAILED = (char)4,
    REQ_SUCCESS = (char)5,
    REQ_CLOSEFILE = (char)6,
    REQ_LOCK = (char)7,
    REQ_UNLOCK = (char)8,
    REQ_REMOVE = (char)9
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