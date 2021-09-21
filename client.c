#include <stdio.h>
#include <time.h>

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

    parse_args(argc, argv, &args);
    check_args(args);

    PERROR_DIE(openConnection((char *)ARGS_VALUE(args, ARG_SOCKETFILE), 500, absVal), -1);


    PERROR_DIE(closeConnection((char *)ARGS_VALUE(args, ARG_SOCKETFILE)), -1);
    args_free(&args);

    return 0;
};
