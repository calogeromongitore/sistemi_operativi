#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#include "include/logger.h"
#include "include/common.h"
#include "include/args.h"
#include "include/api.h"

#define IFTRACE(__cond, __frmt...) if (__cond) ptrace(__frmt)
#define IFDO(__cond, __cmd) if (__cond) __cmd

void check_args(args__cont__t args) {

    if (ARGS_ISNULL(args, ARG_SOCKETFILE)) {
        fprintf(stderr, "ERORR: argument '-f' is required\n");
        exit(-1);
    }

    if (!ARGS_ISNULL(args, ARG_BIGD) && ARGS_ISNULL(args, ARG_WRITELIST)) {
        fprintf(stderr, "-D is requested with -W\n");
        exit(-1);
    }

    if (!ARGS_ISNULL(args, ARG_SMALLD) && (ARGS_ISNULL(args, ARG_READS) && ARGS_ISNULL(args, ARG_RNDREAD))) {
        fprintf(stderr, "-d is requested with -r or -R\n");
        exit(-1);
    }

}

int main(int argc, char **argv) {
    args__cont__t args;
    struct timespec absVal = {.tv_sec = 0, .tv_nsec = 800000000};
    char *buf, *rejstore_path = NULL, *readstore_path = NULL;
    void *data;
    char chunkbuf[CHUNK_SIZE];
    int len, i, j, k, fd, chunkread, retval;
    size_t size;
    struct stat st;
    int flock;

    parse_args(argc, argv, &args);

    if (!ARGS_ISNULL(args, ARG_HELP)) {
        printf("HELP HERE!\n");
        return 0;
    }

    if (ARGS_ISNULL(args, ARG_DELAY)) {
        set_interval(0);
    } else {
        set_interval(atoi(ARGS_VALUE(args, ARG_DELAY)));
    }

    flock = !ARGS_ISNULL(args, ARG_LOCK) << O_LOCK;

    check_args(args);
    if (!ARGS_ISNULL(args, ARG_SOCKETFILE)) {
        IFTRACE(!ARGS_ISNULL(args, ARG_PRINT), "Opening socket file %s", ARGS_VALUE(args, ARG_SOCKETFILE));
        PERROR_DIE(openConnection((char *)ARGS_VALUE(args, ARG_SOCKETFILE), 500, absVal), -1);
        IFTRACE(!ARGS_ISNULL(args, ARG_PRINT), "Socket file %s opened!", ARGS_VALUE(args, ARG_SOCKETFILE));
    }

    if (!ARGS_ISNULL(args, ARG_UNLOCK)) {

        len = strlen(ARGS_VALUE(args, ARG_UNLOCK));
        for (i = j = 0; i <= len; i++) {
            if (ARGS_VALUE(args, ARG_UNLOCK)[i] == ',') {
                ARGS_VALUE(args, ARG_UNLOCK)[i] = '\0';
            }

            if (ARGS_VALUE(args, ARG_UNLOCK)[i] == '\0') {

                retval = unlockFile(ARGS_VALUE(args, ARG_UNLOCK) + j);
                IFTRACE(!retval && !ARGS_ISNULL(args, ARG_PRINT), "TYPE: %s - FILE: %s - EXIT VAL: %d (%s)", "UNLOCK", ARGS_VALUE(args, ARG_UNLOCK) + j, retval, strerror(errno));

                if (i < len) {
                    ARGS_VALUE(args, ARG_UNLOCK)[i] = ',';
                }

                j = i + 1;
            }
        }

    }

    if (!ARGS_ISNULL(args, ARG_REMOVE)) {

        len = strlen(ARGS_VALUE(args, ARG_REMOVE));
        for (i = j = 0; i <= len + 2; i++) {
            if (ARGS_VALUE(args, ARG_REMOVE)[i] == ',') {
                ARGS_VALUE(args, ARG_REMOVE)[i] = '\0';
            }

            if (ARGS_VALUE(args, ARG_REMOVE)[i] == '\0') {

                retval = openFile(ARGS_VALUE(args, ARG_REMOVE) + j, O_LOCK);
                IFTRACE(!retval && !ARGS_ISNULL(args, ARG_PRINT), "TYPE: %s - FILE: %s - EXIT VAL: %d (%s)", "OPEN", ARGS_VALUE(args, ARG_REMOVE) + j, retval, strerror(errno));

                IFDO(!retval, retval = removeFile(ARGS_VALUE(args, ARG_REMOVE) + j));
                IFTRACE(!retval && !ARGS_ISNULL(args, ARG_PRINT), "TYPE: %s - FILE: %s - EXIT VAL: %d (%s)", "REMOVE", ARGS_VALUE(args, ARG_REMOVE) + j, retval, strerror(errno));

                ARGS_VALUE(args, ARG_REMOVE)[i] = ',';
                j = i + 1;
            }
        }
    }

    if (!ARGS_ISNULL(args, ARG_BIGD)) {
        rejstore_path = newstrcat(ARGS_VALUE(args, ARG_BIGD), "/");
    }

    if (!ARGS_ISNULL(args, ARG_WRITELIST)) {

        len = strlen(ARGS_VALUE(args, ARG_WRITELIST));
        for (i = j = 0; i <= len + 2; i++) {
            if (ARGS_VALUE(args, ARG_WRITELIST)[i] == ',') {
                ARGS_VALUE(args, ARG_WRITELIST)[i] = '\0';
            }

            if (ARGS_VALUE(args, ARG_WRITELIST)[i] == '\0') {
                retval = openFile(ARGS_VALUE(args, ARG_WRITELIST) + j, O_CREATE | O_LOCK);
                IFTRACE(!ARGS_ISNULL(args, ARG_PRINT), "TYPE: %s - FILE: %s - EXIT VAL: %d (%s)", "OPEN", ARGS_VALUE(args, ARG_WRITELIST) + j, retval, strerror(errno));

                stat(ARGS_VALUE(args, ARG_WRITELIST) + j, &st);
                if (st.st_size <= CHUNK_SIZE) {
                    IFDO(!retval, retval = writeFile(ARGS_VALUE(args, ARG_WRITELIST) + j, rejstore_path));
                    IFTRACE(!retval && !ARGS_ISNULL(args, ARG_PRINT), "TYPE: %s - FILE: %s - SIZE: %d - EXIT VAL: %d (%s)", "WRITE", ARGS_VALUE(args, ARG_WRITELIST) + j, st.st_size, retval, strerror(errno));
                } else {

                    fd = open(ARGS_VALUE(args, ARG_WRITELIST) + j, O_RDONLY);
                    IFDO(!retval, retval = writeFile(ARGS_VALUE(args, ARG_WRITELIST) + j, rejstore_path));
                    IFTRACE(!retval && !ARGS_ISNULL(args, ARG_PRINT), "TYPE: %s - FILE: %s - SIZE: %d - EXIT VAL: %d (%s)", "WRITE", ARGS_VALUE(args, ARG_WRITELIST) + j, 0, retval, strerror(errno));

                    for (k = 0; !retval && k <= st.st_size / CHUNK_SIZE; k++) {
                        // printf("\t:: file size %ld\n", st.st_size);
                        // printf("\t-- chunk %d/%ld\n", k + 1, st.st_size / CHUNK_SIZE + 1);
                        chunkread = read(fd, chunkbuf, CHUNK_SIZE);
                        if (chunkread > 0) {
                            IFDO(!retval, retval = appendToFile(ARGS_VALUE(args, ARG_WRITELIST) + j, chunkbuf, chunkread, rejstore_path));
                            IFTRACE(!ARGS_ISNULL(args, ARG_PRINT), "TYPE: %s - FILE: %s - SIZE: %d - EXIT VAL: %d (%s)", "APPEND", ARGS_VALUE(args, ARG_WRITELIST) + j, chunkread, retval, strerror(errno));
                        }
                    }


                    close(fd);
                }

                // if (!retval && ARGS_ISNULL(args, ARG_LOCK) || !strstr(ARGS_VALUE(args, ARG_LOCK), ARGS_VALUE(args, ARG_WRITELIST) + j)) {
                //     IFDO(!retval, retval = unlockFile(ARGS_VALUE(args, ARG_WRITELIST) + j));
                //     IFTRACE(!ARGS_ISNULL(args, ARG_PRINT), "TYPE: %s - FILE: %s - SIZE: %d - EXIT VAL: %d (%s)", "UNLOCK", ARGS_VALUE(args, ARG_WRITELIST) + j, chunkread, retval, strerror(errno));
                // }

                IFDO(!retval, retval = unlockFile(ARGS_VALUE(args, ARG_WRITELIST) + j));
                IFTRACE(!ARGS_ISNULL(args, ARG_PRINT), "TYPE: %s - FILE: %s - SIZE: %d - EXIT VAL: %d (%s)", "UNLOCK", ARGS_VALUE(args, ARG_WRITELIST) + j, chunkread, retval, strerror(errno));

                IFDO(!retval, retval = closeFile(ARGS_VALUE(args, ARG_WRITELIST) + j));
                IFTRACE(!retval && !ARGS_ISNULL(args, ARG_PRINT), "TYPE: %s - FILE: %s - EXIT VAL: %d (%s)", "CLOSE", ARGS_VALUE(args, ARG_WRITELIST) + j, retval, strerror(errno));

                if (i < len) {
                    ARGS_VALUE(args, ARG_WRITELIST)[i] = ',';
                }

                j = i + 1;
            }
        }
    }

    if (!ARGS_ISNULL(args, ARG_SMALLD)) {
        readstore_path = newstrcat(ARGS_VALUE(args, ARG_SMALLD), "/");
    }

    if (!ARGS_ISNULL(args, ARG_READS)) {

        len = strlen(ARGS_VALUE(args, ARG_READS));
        for (i = j = 0; i <= len; i++) {
            if (ARGS_VALUE(args, ARG_READS)[i] == ',') {
                ARGS_VALUE(args, ARG_READS)[i] = '\0';
            }

            if (ARGS_VALUE(args, ARG_READS)[i] == '\0') {

                if (ARGS_ISNULL(args, ARG_LOCK)) {
                    retval = openFile(ARGS_VALUE(args, ARG_READS) + j, 0);
                } else {
                    retval = openFile(ARGS_VALUE(args, ARG_READS) + j, (strstr(ARGS_VALUE(args, ARG_LOCK), ARGS_VALUE(args, ARG_READS) + j) != NULL) << O_LOCK);
                }

                IFTRACE(!ARGS_ISNULL(args, ARG_PRINT), "TYPE: %s - FILE: %s - EXIT VAL: %d (%s)", "OPEN", ARGS_VALUE(args, ARG_READS) + j, retval, strerror(errno));

                IFDO(!retval, retval = readFile(ARGS_VALUE(args, ARG_READS) + j, &data, &size));
                IFTRACE(!retval && !ARGS_ISNULL(args, ARG_PRINT), "TYPE: %s - FILE: %s - EXIT VAL: %d (%s)", "READ", ARGS_VALUE(args, ARG_READS) + j, retval, strerror(errno));

                if (!retval && readstore_path) {
                    buf = newstrcat(readstore_path, ARGS_VALUE(args, ARG_READS) + j);
                    PERROR_DIE(fd = open(buf, O_WRONLY | O_CREAT, 0644), -1);
                    PERROR_DIE(write(fd, data, size), -1);
                    close(fd);
                    free(buf);
                }

                IFDO(!retval, retval = closeFile(ARGS_VALUE(args, ARG_READS) + j));
                IFTRACE(!retval && !ARGS_ISNULL(args, ARG_PRINT), "TYPE: %s - FILE: %s - EXIT VAL: %d (%s)", "CLOSE", ARGS_VALUE(args, ARG_READS) + j, retval, strerror(errno));
                free(data);

                if (i < len) {
                    ARGS_VALUE(args, ARG_READS)[i] = ',';
                }

                j = i + 1;
            }
        }

    }

    if (!ARGS_ISNULL(args, ARG_RNDREAD)) {
        readNFiles(ARGS_VALUE(args, ARG_RNDREAD)[0] == '-' ? 0 : atoi(ARGS_VALUE(args, ARG_RNDREAD)), readstore_path);
    }

    if (!ARGS_ISNULL(args, ARG_LOCK)) {

        len = strlen(ARGS_VALUE(args, ARG_LOCK));
        for (i = j = 0; i <= len; i++) {
            if (ARGS_VALUE(args, ARG_LOCK)[i] == ',') {
                ARGS_VALUE(args, ARG_LOCK)[i] = '\0';
            }

            if (ARGS_VALUE(args, ARG_LOCK)[i] == '\0') {

                retval = openFile(ARGS_VALUE(args, ARG_LOCK) + j, 0);
                IFTRACE(!ARGS_ISNULL(args, ARG_PRINT), "TYPE: %s - FILE: %s - EXIT VAL: %d (%s)", "OPEN", ARGS_VALUE(args, ARG_LOCK) + j, retval, strerror(errno));

                IFDO(!retval, retval = lockFile(ARGS_VALUE(args, ARG_LOCK) + j));
                IFTRACE(!retval && !ARGS_ISNULL(args, ARG_PRINT), "TYPE: %s - FILE: %s - EXIT VAL: %d (%s)", "LOCK", ARGS_VALUE(args, ARG_LOCK) + j, retval, strerror(errno));

                IFDO(!retval, retval = closeFile(ARGS_VALUE(args, ARG_LOCK) + j));
                IFTRACE(!retval && !ARGS_ISNULL(args, ARG_PRINT), "TYPE: %s - FILE: %s - EXIT VAL: %d (%s)", "CLOSE", ARGS_VALUE(args, ARG_LOCK) + j, retval, strerror(errno));

                if (i < len) {
                    ARGS_VALUE(args, ARG_UNLOCK)[i] = ',';
                }

                j = i + 1;
            }
        }

    }

    PERROR_DIE(closeConnection((char *)ARGS_VALUE(args, ARG_SOCKETFILE)), -1);

    return 0;
};
