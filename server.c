
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>

#include "include/common.h"
#include "include/logger.h"
#include "include/workers.h"
#include "include/args.h"
#include "include/storage.h"
#include "include/reqframe.h"
#include "include/fifo.h"
#include "include/list.h"

#define SET_FDMAX(actual, newfd) actual = ((newfd > actual) ? newfd : actual)

#define BUF_SIZ 512

typedef struct {
    int workers;
    int total_mb;
    int chunks;
} config_t;

typedef struct {
    char *data;
    int sfd2;
    int bytes;
    storage_t storage;
    fifo_t fifo;
    workers_t workers;
    workers_t workersqueue;
    int reqid;
} thargs_t;


config_t parse_config(char *path_config) {
    FILE *fp;
    config_t conf;
    char buff[BUF_SIZ];
    char *ptr;

    if ((fp = fopen(path_config, "r")) == NULL) {
        fprintf(stderr, "ERORR: unable to open %s\n", path_config);
        exit(-1);
    }

    while (fgets(buff, BUF_SIZ, fp) != NULL) {

        for (ptr = buff + strlen(buff) - 1; ptr != buff-1; ptr--) {
            if (*ptr == '\n') {
                *ptr = '\0';
            } else if (*ptr == ':') {
                *ptr = '\0';
                ptr += 2;
                break;
            }
        }

        if (strcmp(buff, "workers") == 0) {
            conf.workers = atoi(ptr);
        } else if (strcmp(buff, "totalmb") == 0) {
            conf.total_mb = atoi(ptr);
        } else if (strcmp(buff, "chunks") == 0) {
            conf.chunks = atoi(ptr);
        }

    }

    fclose(fp);
    return conf;
}


void *th_routine(void *args) {
    workers_t workers;
    thargs_t thargs_cpy;
    reqcode_t req, reqst;
    size_t loc, fileretsize, rem;
    struct reqcall reqc;
    char rbuf2[1024], buf4[1024];
    char *buf2;
    int retval, len;
    int thid;

    workers = (workers_t)args;
    thid = rand() % 0x40;

    while(1) {

        workers_piperead(workers, &thargs_cpy, sizeof thargs_cpy);

        printf("[#%02d] Data read at 0x%p\n", thid, thargs_cpy.data);

        loc = 0;
        buf2 = rbuf2;
        reqcall_default(&reqc);
        if (thargs_cpy.data[loc++] == PARAM_SEP) {

            req = thargs_cpy.data[loc++];

            while (loc < thargs_cpy.bytes) {
                switch (((char *)thargs_cpy.data)[loc++]) {
                    case PARAM_PATHNAME:

                        memcpy(&len, thargs_cpy.data + loc, sizeof len);
                        loc += sizeof len;

                        reqc.pathname = thargs_cpy.data + loc;
                        loc += len;

                        break;

                    case PARAM_FLAGS:
                        memcpy(&reqc.flags, thargs_cpy.data + loc, sizeof reqc.flags);
                        loc += sizeof reqc.flags;
                        break;

                    case PARAM_SIZE:
                        memcpy(&reqc.size, thargs_cpy.data + loc, sizeof reqc.size);
                        loc += sizeof reqc.size;
                        break;
                    
                    case PARAM_BUF:
                        reqc.buf = thargs_cpy.data + loc;
                        loc += reqc.size;
                        break;

                    case PARAM_N:
                        memcpy(&reqc.N, thargs_cpy.data + loc, sizeof reqc.N);
                        loc += sizeof reqc.N;

                    default:
                        break;

                }

                if (thargs_cpy.data[loc++] != PARAM_SEP) {
                    break;
                }

            }

            switch(req) {

                case REQ_OPEN:
                    retval = storage_open(thargs_cpy.storage, thargs_cpy.sfd2, reqc.pathname, reqc.flags);
                    if (retval == A_LKWAIT) {
                        fifo_enqueue(thargs_cpy.fifo, &thargs_cpy, sizeof thargs_cpy);
                        printf("queued reqid %d\n", thargs_cpy.reqid);
                        continue;
                    }

                    loc = 0;
                    break;

                case REQ_CLOSEFILE:
                    retval = storage_close(thargs_cpy.storage, thargs_cpy.sfd2, reqc.pathname);
                    workers_pipewrite(thargs_cpy.workersqueue, &thargs_cpy.fifo, sizeof thargs_cpy.fifo);
                    loc = 0;
                    break;

                case REQ_READ:
                    retval = storage_read(thargs_cpy.storage, thargs_cpy.sfd2, reqc.pathname, (void *)&buf2, &loc);
                    break;

                case REQ_LOCK:
                    retval = storage_lock(thargs_cpy.storage, thargs_cpy.sfd2, reqc.pathname);
                    if (retval == A_LKWAIT) {
                        fifo_enqueue(thargs_cpy.fifo, &thargs_cpy, sizeof thargs_cpy);
                        printf("queued reqid %d\n", thargs_cpy.reqid);
                        continue;
                    }

                    loc = 0;
                    break;

                case REQ_UNLOCK:
                    retval = storage_unlock(thargs_cpy.storage, thargs_cpy.sfd2, reqc.pathname);
                    loc = 0;
                    break;
                
                case REQ_REMOVE:
                    retval = storage_remove(thargs_cpy.storage, thargs_cpy.sfd2, reqc.pathname);
                    workers_pipewrite(thargs_cpy.workersqueue, &thargs_cpy.fifo, sizeof thargs_cpy.fifo);
                    loc = 0;
                    break;

                case REQ_WRITE:
                    retval = storage_write(thargs_cpy.storage, thargs_cpy.sfd2, reqc.buf, reqc.size, (char *)reqc.pathname);
                    loc = 0;
                    break;

                case REQ_APPEND:
                    retval = storage_append(thargs_cpy.storage, thargs_cpy.sfd2, reqc.buf, reqc.size, (char *)reqc.pathname);
                    loc = 0;
                    break;

                case REQ_GETSIZ:
                    retval = storage_getsize(thargs_cpy.storage, thargs_cpy.sfd2, reqc.pathname, &loc);
                    memcpy(buf2, &loc, sizeof loc);
                    loc = sizeof loc;
                    break;

                case REQ_RNDREAD:
                    retval = storage_retrieve(thargs_cpy.storage, thargs_cpy.sfd2, reqc.N);
                    break;

            }
        }

        reqst = (retval != E_ITSOK) ? REQ_FAILED : REQ_SUCCESS; 
        write(thargs_cpy.sfd2, &reqst, sizeof reqst);

        if (reqst == REQ_FAILED) {
            write(thargs_cpy.sfd2, &retval, sizeof retval);
        } else if (req == REQ_WRITE || req == REQ_APPEND || req == REQ_RNDREAD) {

            len = -1;
            while (storage_getremoved(thargs_cpy.storage, &rem, (void **)&buf2, &loc, buf4, &fileretsize), ++len, rem) {

                if (!len) {
                    write(thargs_cpy.sfd2, (void *)&rem, sizeof(int));
                }

                write(thargs_cpy.sfd2, &loc, sizeof loc);
                write(thargs_cpy.sfd2, buf2, loc);
                write(thargs_cpy.sfd2, &fileretsize, sizeof fileretsize);
                write(thargs_cpy.sfd2, buf4, fileretsize);

                free(buf2);
            }

            if (!len) {
                write(thargs_cpy.sfd2, (void *)&len, sizeof len);
            }

        } else {
            write(thargs_cpy.sfd2, buf2, loc);
        }

        free(thargs_cpy.data);
    }

    return NULL;
}

void *th_routine_queue(void *args) {
    thargs_t thargscpy;
    fifo_t fifo;
    workers_t workers;
    size_t fifosize;

    workers = (workers_t)args;

    while(1) {

        workers_piperead(workers, (void *)&fifo, sizeof fifo);
        printf("Queue checking waked up!\n");

        fifosize = fifo_usedspace(fifo);
        while (fifosize > 0 && fifosize >= sizeof thargscpy) {

            fifo_dequeue(fifo, &thargscpy, sizeof thargscpy);
            printf("dequeued reqid %d\n", thargscpy.reqid);

            workers_pipewrite(thargscpy.workers, &thargscpy, sizeof thargscpy);
            fifosize -= sizeof thargscpy;
        }

    }

    return NULL;
}

int main(int argc, char **argv) {
    int sfd, sfd2, t, ready_fds, i, bytes, fdmax;
    fd_set rfds, rfds_cpy;
    struct sockaddr_un local, remote;
    args__cont__t args;
    config_t conf;
    workers_t workers, workers_queue;
    thargs_t thargs;
    storage_t storage;
    fifo_t fifo;
    char buf[2048];
    char *ptr, *data1, *data2;
    char buf2[1024], buf3[1024], buf4[1024];
    int i2, i3, i4;
    int reqid = 0;
    
    parse_args(argc, argv, &args);

    if (ARGS_ISNULL(args, ARG_SETTINGS)) {
        fprintf(stderr, "ERORR: argument '-s' is required\n");
        exit(-1);
    } else if (ARGS_ISNULL(args, ARG_SOCKETFILE)) {
        fprintf(stderr, "ERORR: argument '-f' is required\n");
        exit(-1);
    }

    fdmax = STDERR_FILENO;
    conf = parse_config((char *)ARGS_VALUE(args, ARG_SETTINGS));

    //TODO: check if config values are correct (like workers > 0)


    PERROR_DIE(sfd = socket(AF_UNIX, SOCK_STREAM, 0), -1);
    SET_FDMAX(fdmax, sfd);

    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, (char *)ARGS_VALUE(args, ARG_SOCKETFILE));
    unlink(local.sun_path);

    PERROR_DIE(bind(sfd, (struct sockaddr *)&local, strlen(local.sun_path) + sizeof(local.sun_family)), -1);
    PERROR_DIE(listen(sfd, 10), -1);

    FD_ZERO(&rfds_cpy);
    FD_ZERO(&rfds);
    FD_SET(sfd, &rfds);

    signal(SIGPIPE, SIG_IGN);

    workers = workers_init(4);
    workers_start(workers, th_routine);
    SET_FDMAX(fdmax, workers_getmaxfd(workers));

    storage = storage_init(22690, 4);
    fifo = fifo_init(64 * sizeof(thargs_t));

    workers_queue = workers_init(1);
    workers_start(workers_queue, th_routine_queue);
    SET_FDMAX(fdmax, workers_getmaxfd(workers_queue));

    while (1) {
        rfds_cpy = rfds;
        ready_fds = select(fdmax + 1, &rfds_cpy, NULL, NULL, NULL);

        //STDERR_FILENO = 2
        for (i = STDERR_FILENO + 1; i <= fdmax && ready_fds; i++) {

            if (!FD_ISSET(i, &rfds_cpy)) {
                continue;
            }

            --ready_fds;
            if (i == sfd) {
                t = sizeof(remote);
                PERROR_DIE(sfd2 = accept(i, (struct sockaddr *)&remote, &t), -1);

                // fcntl(sfd2, F_SETFL, fcntl(sfd2, F_GETFL, 0) | O_NONBLOCK);
                FD_SET(sfd2, &rfds);
                SET_FDMAX(fdmax, sfd2);

            } else {
                
                //fd is non blocking, so do a while and call read till it returns -1
                //check if errno == EAGAIN or EWOULDBLOCK, if so means ok
                //if it returns 0 the fd must be deleted from the epoll
                //at each successful read, realloc the buffer
                if ((bytes = read(i, (void *)buf, 1024)) <= 0) {
                    FD_CLR(i, &rfds);
                    // close(i);
                    // TODO: do a funciton like storage.closeallfilesfrom(i) in order
                    // to close all the files opened by clientid = i
                    // then wake up the fifo thread
                    // workers_pipewrite(workers_queue, &fifo, sizeof fifo);
                    continue;
                }

                // read(events[i].data.fd, (void *)buf, 1024);
                // perror("READ");
                // printf("errno: %d\n", errno);
                thargs.sfd2 = i;
                thargs.bytes = bytes;
                thargs.data = (char *)malloc(bytes * sizeof *thargs.data);
                thargs.storage = storage;
                thargs.fifo = fifo;
                thargs.workers = workers;
                thargs.workersqueue = workers_queue;
                thargs.reqid = reqid++;
                memcpy(thargs.data, buf, bytes);
                workers_pipewrite(workers, &thargs, sizeof thargs);

                // printf("\n\nData wrote at 0x%p: %c\n", thargs.data, thargs.data[0]);
            }

        }

    }

    workers_mainloop(workers);


    /** FREE EVERYTHING **/
    workers_delete(workers);
    args_free(&args);
    close(sfd);


    return 0;
};
