
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
#include <sys/signalfd.h>

#include "include/common.h"
#include "include/logger.h"
#include "include/workers.h"
#include "include/args.h"
#include "include/storage.h"
#include "include/reqframe.h"
#include "include/fifo.h"
#include "include/list.h"

#define SET_FDMAX(actual, newfd) actual = ((newfd > actual) ? newfd : actual)
#define SOCKET_CLOSE(__sfd) if (__sfd) {\
    close(__sfd); \
    __sfd = 0;\
}

#define NFD_SET(__fd, __setptr, __cnt) {\
    FD_SET(__fd, __setptr);\
    __cnt++;\
}

#define NFD_CLR(__fd, __setptr, __cnt) {\
    FD_CLR(__fd, __setptr);\
    __cnt--;\
}

#define BUF_SIZ 512

struct config_data {
    int workers;
    int total_mb;
    int maxfiles;
    int maxqueue;
    int maxincomingdata;
};

typedef struct {
    char *data;
    int sfd2;
    int bytes;
    storage_t storage;
    fifo_t fifo;
    workers_t workers;
    workers_t workersqueue;
    int reqid;
    char quit;
} thargs_t;


struct config_data parse_config(char *path_config) {
    FILE *fp;
    struct config_data conf;
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

        if (!strcmp(buff, "workers")) {
            conf.workers = atoi(ptr);
        } else if (!strcmp(buff, "totalmb")) {
            conf.total_mb = atoi(ptr);
        } else if (!strcmp(buff, "maxfiles")) {
            conf.maxfiles = atoi(ptr);
        } else if (!strcmp(buff, "maxqueue")) {
            conf.maxqueue = atoi(ptr);
        } else if (!strcmp(buff, "maxincomingdata")) {
            conf.maxincomingdata = atoi(ptr);
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
    char rbuf2[1024], buf4[1024], reqstr[0x10], estr[0x20];
    char *buf2;
    int retval, len;
    int thid;

    workers = (workers_t)args;
    thid = rand() % 0x40;

    // printf("[#%02d] Ready to work!\n", thid);
    trace("[#%02d] Ready to work", thid);

    while(1) {

        workers_piperead(workers, &thargs_cpy, sizeof thargs_cpy);
        if (thargs_cpy.quit) {
            // printf("[#%02d] Closing thread!\n", thid);
            break;
        }

        // printf("[#%02d] Client %d sent %d bytes\n", thid, thargs_cpy.sfd2, thargs_cpy.bytes);
        trace("[#%02d] Client %d sent %d bytes", thid, thargs_cpy.sfd2, thargs_cpy.bytes);

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
                        // printf("queued reqid %d\n", thargs_cpy.reqid);
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
                        // printf("queued reqid %d\n", thargs_cpy.reqid);
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

        // printf("\t-- REQ: %3d (%10s)\t RETURN VALUE: %3d (%10s)\n", req, req_str(req, reqstr), retval, err_str(retval, estr));
        trace("\t-- REQ: %3d (%10s)\t RETURN VALUE: %3d (%10s)", req, req_str(req, reqstr), retval, err_str(retval, estr));

        reqst = (retval != E_ITSOK) ? REQ_FAILED : REQ_SUCCESS; 
        write(thargs_cpy.sfd2, &reqst, sizeof reqst);

        if (reqst == REQ_FAILED) {
            write(thargs_cpy.sfd2, &retval, sizeof retval);
        } else if (req == REQ_WRITE || req == REQ_APPEND || req == REQ_RNDREAD) {

            len = -1;
            while (storage_getremoved(thargs_cpy.storage, &rem, (void **)&buf2, &loc, buf4, &fileretsize), ++len, rem) {

                if (req != REQ_RNDREAD) {
                    trace("\t\t-- CACHE OVLD! Returning %s [%ld bytes]", buf4, loc);
                }

                if (!len) {
                    write(thargs_cpy.sfd2, (void *)&rem, sizeof rem);
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
        // printf("Queue checking waked up!\n");

        fifosize = fifo_usedspace(fifo);
        while (fifosize > 0 && fifosize >= sizeof thargscpy) {

            fifo_dequeue(fifo, &thargscpy, sizeof thargscpy);
            if (thargscpy.quit) {
                // printf("Exiting!\n");
                return NULL;
            }

            // printf("dequeued reqid %d\n", thargscpy.reqid);

            workers_pipewrite(thargscpy.workers, &thargscpy, sizeof thargscpy);
            fifosize -= sizeof thargscpy;
        }

    }

    return NULL;
}

int main(int argc, char **argv) {
    int sfd, sfd2, sigfd, t, ready_fds, i, bytes, fdmax, fdsetsiz = 0;
    fd_set rfds, rfds_cpy;
    struct sockaddr_un local, remote;
    struct signalfd_siginfo fdsi;
    struct storage_info storinfo;
    args__cont__t args;
    struct config_data conf;
    workers_t workers, workers_queue;
    sigset_t mask;
    thargs_t thargs;
    storage_t storage;
    fifo_t fifo;
    size_t s1, s2, s3;
    char quit = 0;
    char *buf;
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

    buf = (char *)malloc(conf.maxincomingdata * sizeof *buf);

    PERROR_DIE(sfd = socket(AF_UNIX, SOCK_STREAM, 0), -1);
    SET_FDMAX(fdmax, sfd);

    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, (char *)ARGS_VALUE(args, ARG_SOCKETFILE));
    unlink(local.sun_path);

    PERROR_DIE(bind(sfd, (struct sockaddr *)&local, strlen(local.sun_path) + sizeof(local.sun_family)), -1);
    PERROR_DIE(listen(sfd, 10), -1);

    FD_ZERO(&rfds_cpy);
    FD_ZERO(&rfds);
    NFD_SET(sfd, &rfds, fdsetsiz);

    signal(SIGPIPE, SIG_IGN);

    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGQUIT);
    sigaddset(&mask, SIGHUP);

    // SIGINT SIGQUIT SIGHUP only handled by filedescriptors. 
    // Disabling default handler or any other handler
    PERROR_DIE(sigprocmask(SIG_BLOCK, &mask, NULL), -1);
    PERROR_DIE(sigfd = signalfd(-1, &mask, 0), -1);
    NFD_SET(sigfd, &rfds, fdsetsiz);
    SET_FDMAX(fdmax, sigfd);

    workers = workers_init(conf.workers);
    workers_start(workers, th_routine);
    SET_FDMAX(fdmax, workers_getmaxfd(workers));

    storage = storage_init(conf.total_mb * 1024 * 1024, conf.maxfiles);
    fifo = fifo_init(conf.maxqueue * sizeof(thargs_t));

    workers_queue = workers_init(1);
    workers_start(workers_queue, th_routine_queue);
    SET_FDMAX(fdmax, workers_getmaxfd(workers_queue));

    // Cleraing possible struct pads added by compiler
    memset((void *)&thargs, 0, sizeof thargs);

    while (!quit && fdsetsiz > 0) {
        rfds_cpy = rfds;
        ready_fds = select(fdmax + 1, &rfds_cpy, NULL, NULL, NULL);

        if (FD_ISSET(sigfd, &rfds_cpy)) {
            read(sigfd, &fdsi, sizeof fdsi);

            switch (fdsi.ssi_signo) {
                case SIGINT:
                case SIGQUIT:
                    printf("SIGINT or SIGQUIT recv!\n");
                    quit = 1;
                    break;

                case SIGHUP:
                    printf("SIGHUP recv!\n");
                    NFD_CLR(sigfd, &rfds, fdsetsiz);
                    NFD_CLR(sfd, &rfds, fdsetsiz);
                    SOCKET_CLOSE(sfd);
                    break;
                
                default:
                    break;
            }

            continue;
        }

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
                NFD_SET(sfd2, &rfds, fdsetsiz);
                SET_FDMAX(fdmax, sfd2);

                // printf("Client %d connected\n", sfd2);
                trace("Client %d connected", sfd2);

            } else {
                
                //fd is non blocking, so do a while and call read till it returns -1
                //check if errno == EAGAIN or EWOULDBLOCK, if so means ok
                //if it returns 0 the fd must be deleted from the epoll
                //at each successful read, realloc the buffer
                if ((bytes = read(i, (void *)buf, 1024)) <= 0) {
                    NFD_CLR(i, &rfds, fdsetsiz);
                    trace("Client %d disconnected", sfd2);
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
                thargs.quit = quit;
                memcpy(thargs.data, buf, bytes);
                workers_pipewrite(workers, &thargs, sizeof thargs);

                // printf("\n\nData wrote at 0x%p: %c\n", thargs.data, thargs.data[0]);
            }

        }

    }

    printf("[#00] Stopping..\n");
    memset(&thargs, (char)0, sizeof thargs);
    thargs.quit = 1;
    workers_multicast(workers, &thargs, sizeof thargs);
    fifo_enqueue(fifo, &thargs, sizeof thargs);
    workers_pipewrite(workers_queue, &fifo, sizeof fifo);
    workers_mainloop(workers);
    workers_mainloop(workers_queue);
    printf("[#00] Stopping..\n");

    printf("\n\n");
    printf("\t\t ! SERVER STATISTICS !\n\n");

    storage_getinfo(storage, &storinfo);
    printf("%-38s:\t%d\n", "MAX STORED FILES NUMBER", storinfo.maxnum);
    printf("%-38s:\t%.2f MB\n", "MAX STORED FILES SIZE", (storinfo.maxsize/((float)1024*1024)));
    printf("%-38s:\t%d\n", "CACHE CLEAN NUMBER", storinfo.nkills);

    printf("\n%-30s %8s\n", "FILE NAME", "SIZE [B]");
    storage_retrieve(storage, 0, 0);
    while (storage_getremoved(storage, &s3, NULL, &s1, buf, &s2), s3) {
        printf("%-30s %8ld\n", buf, s1);
    }

    printf("\n\n");


    /** FREE EVERYTHING **/
    FD_ZERO(&rfds);
    FD_ZERO(&rfds_cpy);
    workers_delete(workers);
    workers_delete(workers_queue);
    storage_destroy(storage);
    fifo_destroy(fifo);
    args_free(&args);
    SOCKET_CLOSE(sfd);
    close(sigfd);
    free(buf);

    return 0;
}
