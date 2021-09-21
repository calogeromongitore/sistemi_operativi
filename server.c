
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

    workers = (workers_t)args;

    while(1) {

        workers_piperead(workers, &thargs_cpy, sizeof thargs_cpy);

        // printf("Data read at 0x%p: %c\n", thargs_cpy.data, thargs_cpy.data[0]);
        if (thargs_cpy.data[0] <= 'a') {
            sleep(10);
        }

        // TODO: write performed by the master thread. 
        // send the result to it through another pipe
        write(thargs_cpy.sfd2, thargs_cpy.data, thargs_cpy.bytes);

        free(thargs_cpy.data);
    }

    return NULL;
}

int main(int argc, char **argv) {
    int sfd, sfd2, t, ready_fds, i, bytes, fdmax;
    fd_set rfds, rfds_cpy;
    struct sockaddr_un local, remote;
    args__cont__t args;
    config_t conf;
    workers_t workers;
    thargs_t thargs;
    char buf[1024];
    char *ptr, *data1, *data2;
    char buf2[1024], buf3[1024];
    storage_t storage;
    int i2, i3;
    
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

    storage = storage_init(128, 4);

    i = sprintf(buf, "ciaoaoaoaoaoaoaoaoaoaoaoaaoaooaoaoaoaaoaoaoao\n");
    i2 = sprintf(buf2, "asdassadasdasdasdasdasdasdsdada\n");
    storage_insert(storage, buf, i, "/home/pietra/santa.txt", buf3, (size_t *)&i3);
    storage_insert(storage, buf2, i2, "/home/pietra/roccia.txt", buf3, (size_t *)&i3);
    storage_insert(storage, buf, i, "/home/pietra/santa2.txt", buf3, (size_t *)&i3);
    storage_insert(storage, buf2, i2, "/home/pietra/roccia2.txt", buf3, (size_t *)&i3);
    storage_insert(storage, buf2, 1, "/home/pietra/signle.txt", buf3, (size_t *)&i3);


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
                if (!(bytes = read(i, (void *)buf, 1024))) {
                    FD_CLR(i, &rfds);
                    // close(i);
                    continue;
                }

                // read(events[i].data.fd, (void *)buf, 1024);
                // perror("READ");
                // printf("errno: %d\n", errno);
                thargs.sfd2 = i;
                thargs.bytes = bytes;
                thargs.data = (char *)malloc(bytes * sizeof *thargs.data);
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
