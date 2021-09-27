#include "../include/reqframe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/common.h"

void reqcall_default(struct reqcall *req) {
    req->buf = NULL;
    req->diname = NULL;
    req->flags = O_NULL;
    req->N = -1;
    req->pathname = NULL;
    req->size = -1;
}

void prepareRequest(char *buf, size_t *size, reqcode_t req, struct reqcall *reqc) {
    size_t loc;
    int len;

    buf[0] = PARAM_SEP;
    buf[1] = req;

    loc = 2;

    if (reqc->size != -1) {
        buf[loc++] = PARAM_SIZE;
        memcpy(buf + loc, &reqc->size, sizeof reqc->size);
        loc += sizeof reqc->size;
        buf[loc++] = PARAM_SEP;
    }

    if (reqc->buf != NULL && reqc->size != -1) {
        buf[loc++] = PARAM_BUF;
        memcpy(buf + loc, reqc->buf, reqc->size);
        loc += reqc->size;
        buf[loc++] = PARAM_SEP;
    }

    if (reqc->pathname != NULL) {
        len = strlen(reqc->pathname);
        buf[loc++] = PARAM_PATHNAME;

        memcpy(buf + loc, &len, sizeof len);
        loc += sizeof len;

        strcpy(buf + loc, reqc->pathname);
        loc += len;
        buf[loc++] = PARAM_SEP;
    }

    if (reqc->flags != O_NULL) {
        buf[loc++] = PARAM_FLAGS;

        memcpy(buf + loc, &reqc->flags, sizeof reqc->flags);
        loc += sizeof reqc->flags;

        buf[loc++] = PARAM_SEP;
    }
    
    *size = loc;

}