#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "include/common.h"
#include "include/args.h"
#include "include/api.h"

void check_args(args__cont__t args) {

    if (ARGS_ISNULL(args, ARG_SOCKETFILE)) {
        fprintf(stderr, "ERORR: argument '-f' is required\n");
        exit(-1);
    }

}

int main(int argc, char **argv) {
    args__cont__t args;
    struct timespec absVal = {.tv_sec = 0, .tv_nsec = 800000000};
    char *buf;
    size_t size;

    parse_args(argc, argv, &args);
    check_args(args);

    PERROR_DIE(openConnection((char *)ARGS_VALUE(args, ARG_SOCKETFILE), 500, absVal), -1);

    PERROR_DIE(openFile("/home/gabriele97/repos/sistemi_operativi/config.txt", O_CREATE | O_LOCK), -1);
    PERROR_DIE(writeFile("/home/gabriele97/repos/sistemi_operativi/config.txt", "./out/"), -1);
    PERROR_DIE(openFile("suca.txt", O_LOCK), -1);
    PERROR_DIE(readFile("suca.txt", (void *)&buf, &size), -1);

    printf("%s\n", buf);

    while(1) {
        sleep(20);
    }


    PERROR_DIE(closeConnection((char *)ARGS_VALUE(args, ARG_SOCKETFILE)), -1);
    args_free(&args);

    return 0;
};
