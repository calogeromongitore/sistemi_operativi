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

    if (reqc->N != -1) {
        buf[loc++] = PARAM_N;

        memcpy(buf + loc, &reqc->N, sizeof reqc->N);
        loc += sizeof reqc->N;

        buf[loc++] = PARAM_SEP;
    }
    
    *size = loc;

}

char *req_str(reqcode_t req, char *buf) {

    switch (req) {
        case REQ_OPEN:
            strcpy(buf, "REQ_OPEN");
            break;
        
        case REQ_RNDREAD:
            strcpy(buf, "REQ_RNDREAD");
            break;
        
        case REQ_APPEND:
            strcpy(buf, "REQ_APPEND");
            break;
        
        case REQ_WRITE:
            strcpy(buf, "REQ_WRITE");
            break;
        
        case REQ_REMOVE:
            strcpy(buf, "REQ_REMOVE");
            break;
        
        case REQ_UNLOCK:
            strcpy(buf, "REQ_UNLOCK");
            break;
        
        case REQ_LOCK:
            strcpy(buf, "REQ_LOCK");
            break;
        
        case REQ_CLOSEFILE:
            strcpy(buf, "REQ_CLOSEFILE");
            break;
        
        case REQ_SUCCESS:
            strcpy(buf, "REQ_SUCCESS");
            break;
        
        case REQ_FAILED:
            strcpy(buf, "REQ_FAILED");
            break;
        
        case REQ_GETSIZ:
            strcpy(buf, "REQ_GETSIZ");
            break;
        
        case REQ_READ:
            strcpy(buf, "REQ_READ");
            break;
        
        case REQ_CLOSECONN:
            strcpy(buf, "REQ_CLOSECONN");
            break;
        
        default:
            strcpy(buf, "UNDEFINED");
            break;
    }
    
    return buf;
}