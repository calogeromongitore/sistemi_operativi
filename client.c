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

void check_args(args__cont__t args) {

    if (ARGS_ISNULL(args, ARG_SOCKETFILE)) {
        fprintf(stderr, "ERORR: argument '-f' is required\n");
        exit(-1);
    }

    if (!ARGS_ISNULL(args, ARG_BIGD) && ARGS_ISNULL(args, ARG_WRITELIST)) {
        fprintf(stderr, "-D is requested with -w\n");
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
    int len, i, j, k, fd, chunkread;
    size_t size;
    struct stat st;

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


    check_args(args);
    if (!ARGS_ISNULL(args, ARG_SOCKETFILE)) {
        llogp(LOG_DBG, "Opening socket file");
        PERROR_DIE(openConnection((char *)ARGS_VALUE(args, ARG_SOCKETFILE), 500, absVal), -1);
    }

    if (!ARGS_ISNULL(args, ARG_REMOVE)) {
        llogp(LOG_DBG, "Delete file requested!");

        len = strlen(ARGS_VALUE(args, ARG_REMOVE));
        for (i = j = 0; i <= len + 2; i++) {
            if (ARGS_VALUE(args, ARG_REMOVE)[i] == ',') {
                ARGS_VALUE(args, ARG_REMOVE)[i] = '\0';
            }

            if (ARGS_VALUE(args, ARG_REMOVE)[i] == '\0') {
                llogp(LOG_DBG, ARGS_VALUE(args, ARG_REMOVE) + j);

                PERROR_DIE(openFile(ARGS_VALUE(args, ARG_REMOVE) + j, O_LOCK), -1);
                llogp(LOG_DBG, "opened!");
                PERROR_DIE(removeFile(ARGS_VALUE(args, ARG_REMOVE) + j), -1);

                ARGS_VALUE(args, ARG_REMOVE)[i] = ',';
                j = i + 1;
            }
        }
    }

    if (!ARGS_ISNULL(args, ARG_BIGD)) {
        llogp(LOG_DBG, "Storing rejected files in:");
        llogp(LOG_DBG, ARGS_VALUE(args, ARG_BIGD));
        rejstore_path = newstrcat(ARGS_VALUE(args, ARG_BIGD), "/");
    }

    if (!ARGS_ISNULL(args, ARG_WRITELIST)) {
        llogp(LOG_DBG, "Writing file requested!");

        len = strlen(ARGS_VALUE(args, ARG_WRITELIST));
        for (i = j = 0; i <= len + 2; i++) {
            if (ARGS_VALUE(args, ARG_WRITELIST)[i] == ',') {
                ARGS_VALUE(args, ARG_WRITELIST)[i] = '\0';
            }

            if (ARGS_VALUE(args, ARG_WRITELIST)[i] == '\0') {
                llogp(LOG_DBG, ARGS_VALUE(args, ARG_WRITELIST) + j);
                PERROR_DIE(openFile(ARGS_VALUE(args, ARG_WRITELIST) + j, O_CREATE | O_LOCK), -1);

                stat(ARGS_VALUE(args, ARG_WRITELIST) + j, &st);
                if (st.st_size <= CHUNK_SIZE) {
                    PERROR_DIE(writeFile(ARGS_VALUE(args, ARG_WRITELIST) + j, rejstore_path), -1);
                } else {

                    fd = open(ARGS_VALUE(args, ARG_WRITELIST) + j, O_RDONLY);
                    PERROR_DIE(writeFile(ARGS_VALUE(args, ARG_WRITELIST) + j, rejstore_path), -1);

                    for (k = 0; k <= st.st_size / CHUNK_SIZE; k++) {
                        printf("\t:: file size %ld\n", st.st_size);
                        printf("\t-- chunk %d/%ld\n", k + 1, st.st_size / CHUNK_SIZE + 1);
                        chunkread = read(fd, chunkbuf, CHUNK_SIZE);
                        if (chunkread > 0) {
                            PERROR_DIE(appendToFile(ARGS_VALUE(args, ARG_WRITELIST) + j, chunkbuf, chunkread, rejstore_path), -1);
                        }
                    }


                    close(fd);
                }

                PERROR_DIE(closeFile(ARGS_VALUE(args, ARG_WRITELIST) + j), -1);

                if (i < len) {
                    ARGS_VALUE(args, ARG_WRITELIST)[i] = ',';
                }

                j = i + 1;
            }
        }
    }

    if (!ARGS_ISNULL(args, ARG_SMALLD)) {
        llogp(LOG_DBG, "Storing read files in:");
        llogp(LOG_DBG, ARGS_VALUE(args, ARG_SMALLD));
        readstore_path = newstrcat(ARGS_VALUE(args, ARG_SMALLD), "/");
    }

    if (!ARGS_ISNULL(args, ARG_READS)) {
        llogp(LOG_DBG, "Reading file requested:");
        llogp(LOG_DBG, ARGS_VALUE(args, ARG_READS));

        len = strlen(ARGS_VALUE(args, ARG_READS));
        for (i = j = 0; i <= len; i++) {
            if (ARGS_VALUE(args, ARG_READS)[i] == ',') {
                ARGS_VALUE(args, ARG_READS)[i] = '\0';
            }

            if (ARGS_VALUE(args, ARG_READS)[i] == '\0') {
                llogp(LOG_DBG, ARGS_VALUE(args, ARG_READS) + j);

                PERROR_DIE(openFile(ARGS_VALUE(args, ARG_READS) + j, 0), -1);
                PERROR_DIE(readFile(ARGS_VALUE(args, ARG_READS) + j, &data, &size), -1);

                if (readstore_path) {
                    buf = newstrcat(readstore_path, ARGS_VALUE(args, ARG_READS) + j);
                    PERROR_DIE(fd = open(buf, O_WRONLY | O_CREAT, 0644), -1);
                    PERROR_DIE(write(fd, data, size), -1);
                    close(fd);
                    free(buf);
                }

                PERROR_DIE(closeFile(ARGS_VALUE(args, ARG_READS) + j), -1);
                free(data);

                if (i < len) {
                    ARGS_VALUE(args, ARG_READS)[i] = ',';
                }

                j = i + 1;
            }
        }

    }

    if (!ARGS_ISNULL(args, ARG_RNDREAD)) {
        llogp(LOG_DBG, "Random read file requested:");
        llogp(LOG_DBG, ARGS_VALUE(args, ARG_RNDREAD));
        readNFiles(ARGS_VALUE(args, ARG_RNDREAD)[0] == '-' ? 0 : atoi(ARGS_VALUE(args, ARG_RNDREAD)), readstore_path);
    }

    // PERROR_DIE(openConnection((char *)ARGS_VALUE(args, ARG_SOCKETFILE), 500, absVal), -1);

    // PERROR_DIE(openFile("/home/gabriele97/repos/sistemi_operativi/config.txt", O_CREATE | O_LOCK), -1);
    // PERROR_DIE(writeFile("/home/gabriele97/repos/sistemi_operativi/config.txt", "./out/"), -1);
    // PERROR_DIE(openFile("suca.txt", O_LOCK), -1);
    // PERROR_DIE(readFile("suca.txt", (void *)&buf, &size), -1);

    // printf("%s\n", buf);

    // PERROR_DIE(closeFile("suca.txt"), -1);

    // PERROR_DIE(appendToFile("/home/gabriele97/repos/sistemi_operativi/config.txt", buf, size, "./out/"), -1);
    // PERROR_DIE(readFile("/home/gabriele97/repos/sistemi_operativi/config.txt", (void *)&buf, &size), -1);

    // printf("%s\n", buf);

    // while(1) {
    //     sleep(20);
    // }


    PERROR_DIE(closeConnection((char *)ARGS_VALUE(args, ARG_SOCKETFILE)), -1);
    args_free(&args);

    return 0;
};
