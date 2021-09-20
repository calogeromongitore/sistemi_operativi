
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
#include <sys/epoll.h>

#include "include/logger.h"
#include "include/workers.h"

#define PERROR_DIE(action, eq) if ((action) == eq) {\
        perror("ERROR! ");\
        exit(-1);}

#define BUF_SIZ 512

typedef enum {
    ARG_SETTINGS,
    ARG_SOCKETFILE,
    ARG_LAST_NOTVALID
} flagarg_t;

typedef struct {
    flagarg_t arg_type;
    void *value;   
} arg_t;

typedef struct {
    int workers;
    int total_mb;
    int chunks;
} config_t;

typedef struct {
    char *data;
    int sfd2;
    int bytes;
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

void parse_args(int argc, char **argv, arg_t **result, int *length, int **map) {
    int i, idx;

    *length = ((argc-1)/2); 
    *result = (arg_t *)malloc(*length + sizeof **result);
    *map = (int *)malloc(ARG_LAST_NOTVALID * sizeof **map);

    for (i = 0; i < ARG_LAST_NOTVALID; i++) {
        (*map)[i] = -1;
    }
    
    for (idx = 0, i = 1; i < argc; i += 2) {

        if (strcmp(argv[i], "-s") == 0 && (i+1) <= argc) {
            (*result)[idx].arg_type = ARG_SETTINGS;
            (*result)[idx].value = argv[i+1];
            (*map)[ARG_SETTINGS] = idx++;
        } else if (strcmp(argv[i], "-f") == 0 && (i+1) <= argc) {
            (*result)[idx].arg_type = ARG_SOCKETFILE;
            (*result)[idx].value = argv[i+1];
            (*map)[ARG_SOCKETFILE] = idx++;
        }

    }
}

void *th_routine(void *args) {
    workers_t workers;
    thargs_t thargs_cpy;

    workers = (workers_t)args;

    while(1) {
        pthread_mutex_lock(get_mtxptr(workers));

        workers_args_lock(workers);
        thargs_cpy = *((thargs_t *)get_args(workers));
        workers_args_unlock(workers);

        // printf("Read done from fd=%d! Bytes read: %d\n", thargs_cpy.sfd2, bytes);
        if (thargs_cpy.data[0] == 'a') sleep(5);
        write(thargs_cpy.sfd2, thargs_cpy.data, thargs_cpy.bytes);
        free(thargs_cpy.data);

    }

    return NULL;
}

void hdl_SIGUSR1(int sig) {


}

int main(int argc, char **argv) {
    int len_args, sfd, sfd2, t, epollfd, evt_cnt, i, bytes;
    struct epoll_event event, events[4];
    struct sockaddr_un local, remote;
    arg_t *args;
    config_t conf;
    int *map_args;
    workers_t workers;
    thargs_t thargs;
    char buf[1024];
    
    parse_args(argc, argv, &args, &len_args, &map_args);

    if (len_args == 0) {
        fprintf(stderr, "ERORR: 0 args. At least '-s' is needed\n");
        exit(-1);
    } else if (map_args[ARG_SETTINGS] == -1) {
        fprintf(stderr, "ERORR: argument '-s' is required\n");
        exit(-1);
    }

    conf = parse_config((char *)(args[map_args[ARG_SETTINGS]].value));

    //TODO: check if config values are correct (like workers > 0)


    PERROR_DIE(sfd = socket(AF_UNIX, SOCK_STREAM, 0), -1);

    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, "./socket.sk");
    unlink(local.sun_path);

    PERROR_DIE(bind(sfd, (struct sockaddr *)&local, strlen(local.sun_path) + sizeof(local.sun_family)), -1);
    PERROR_DIE(listen(sfd, 10), -1);

    PERROR_DIE(epollfd = epoll_create1(0), -1);

    event.events = EPOLLIN;
    event.data.fd = sfd;

    PERROR_DIE(epoll_ctl(epollfd, EPOLL_CTL_ADD, sfd, &event), -1);

    workers = workers_init(4, &thargs);

    signal(SIGUSR1, hdl_SIGUSR1);
    workers_start(workers, th_routine);

    while (1) {

        PERROR_DIE(evt_cnt = epoll_wait(epollfd, events, 4, -1), -1);

        for (i = 0; i < evt_cnt; i++) {

            if (events[i].data.fd == sfd) {
                t = sizeof(remote);
                PERROR_DIE(sfd2 = accept(events[i].data.fd, (struct sockaddr *)&remote, &t), -1);

                // fcntl(sfd2, F_SETFL, fcntl(sfd2, F_GETFL, 0) | O_NONBLOCK);
                event.events = EPOLLIN;
                event.data.fd = sfd2;

                PERROR_DIE(epoll_ctl(epollfd, EPOLL_CTL_ADD, sfd2, &event), -1);

            } else {
                
                //fd is non blocking, so do a while and call read till it returns -1
                //check if errno == EAGAIN or EWOULDBLOCK, if so means ok
                //if it returns 0 the fd must be deleted from the epoll
                //at each successful read, realloc the buffer
                if (!(bytes = read(events[i].data.fd, (void *)buf, 1024))) {
                    PERROR_DIE(epoll_ctl(epollfd, EPOLL_CTL_DEL, events[i].data.fd, &event), -1);
                    close(events[i].data.fd);
                    continue;
                }

                // read(events[i].data.fd, (void *)buf, 1024);
                // perror("READ");
                // printf("errno: %d\n", errno);

                workers_args_lock(workers);
                thargs.sfd2 = events[i].data.fd;
                thargs.bytes = bytes;
                thargs.data = (char *)malloc(bytes * sizeof *thargs.data);
                memcpy(thargs.data, buf, bytes);
                workers_args_unlock(workers);

                workers_wakeup(workers);

            }

        }

    }

    //TODO: mutex is not ok, it doesn't consider a lot of "unlocks" because
    //mutex can go up to 1 and not higher values. Consider a queue or a semaphore instead of a mutex
    workers_mainloop(workers);


    /** FREE EVERYTHING **/
    workers_delete(workers);
    close(epollfd);
    close(sfd);

    free(args);
    free(map_args);

    return 0;
};
