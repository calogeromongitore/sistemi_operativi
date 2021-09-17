#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/logger.h"

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


int main(int argc, char **argv) {
    int len_args;
    arg_t *args;
    config_t conf;
    int *map_args;
    
    parse_args(argc, argv, &args, &len_args, &map_args);
    conf = parse_config((char *)(args[map_args[ARG_SETTINGS]].value));

    // TODO: check if config values are correct (like workers > 0)




    free(args);
    free(map_args);

    return 0;
};
