#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "include/logger.h"
#include "include/workers.h"

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
    int sfd2;
    struct sockaddr_un remote;
    char *buf;
} thargs_t;


static pthread_mutex_t *mtxptr;

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
    int bytes;

    workers = (workers_t)args;

    while(1) {
        pthread_mutex_lock(get_mtxptr(workers));

        printf("Client connected!\n");

        workers_args_lock(workers);
        thargs_cpy = *((thargs_t *)get_args(workers));
        workers_args_unlock(workers);

        while ((bytes = read(thargs_cpy.sfd2, (void *)thargs_cpy.buf, 1024)) > 0) {
            write(STDOUT_FILENO, thargs_cpy.buf, bytes);
        }

        printf("Client disconnected!\n");
        free(thargs_cpy.buf);
    }

    return NULL;
}

void hdl_SIGUSR1(int sig) {

    pthread_mutex_unlock(mtxptr);

}

int main(int argc, char **argv) {
    int len_args, sfd, t;
    arg_t *args;
    config_t conf;
    int *map_args;
    workers_t workers;
    struct sockaddr_un local;
    thargs_t thargs;
    
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


    if ((sfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("ERORR: unable to initialize the socket");
        exit(-1);
    }

    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, "./socket.sk");
    unlink(local.sun_path);

    if (bind(sfd, (struct sockaddr *)&local, strlen(local.sun_path) + sizeof(local.sun_family)) == -1) {
        perror("ERORR: unable to bind the socket");
        exit(-1);
    }

    if (listen(sfd, 10) == -1) {
        perror("ERORR: unable to listen the socket");
        exit(-1);
    }


    workers = workers_init(4, &thargs);
    mtxptr = get_mtxptr(workers);

    signal(SIGUSR1, hdl_SIGUSR1);
    workers_start(workers, th_routine);


    while(1) {
        t = sizeof(thargs.remote);
        if ((thargs.sfd2 = accept(sfd, (struct sockaddr *)&thargs.remote, &t)) == -1) {
            perror("ERORR: unable to accept the incoming connection");
            exit(-1);
        }

        thargs.buf = malloc(1024 * sizeof *thargs.buf);
        workers_wakeup(workers);
    }


    //TODO: mutex is not ok, it doesn't consider a lot of "unlocks" because
    //mutex can go up to 1 and not higher values. Consider a queue or a semaphore instead of a mutex
    workers_mainloop(workers);


    /** FREE EVERYTHING **/
    workers_delete(workers);
    close(sfd);

    free(args);
    free(map_args);

    return 0;
};
