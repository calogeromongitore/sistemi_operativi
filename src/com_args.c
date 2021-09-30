#include "../include/args.h"

#include <stdlib.h>
#include <string.h>

void parse_args(int argc, char **argv, args__cont__t *argscont) {
    int i, idx;

    argscont->length = ((argc-1)/2); 
    argscont->args = (arg_t *)malloc(argscont->length * sizeof *argscont->args);
    argscont->map = (int *)malloc(ARG_LAST_NOTVALID * sizeof *argscont->map);

    for (i = 0; i < ARG_LAST_NOTVALID; i++) {
        argscont->map[i] = -1;
    }
    
    for (idx = 0, i = 1; i < argc; i++) {

        if ((i+1) >= argc) {
            continue;
        } else if (strcmp(argv[i], "-s") == 0) {
            argscont->args[idx].arg_type = ARG_SETTINGS;
        } else if (strcmp(argv[i], "-f") == 0) {
            argscont->args[idx].arg_type = ARG_SOCKETFILE;
        } else if (strcmp(argv[i], "-h") == 0) {
            argscont->args[idx].arg_type = ARG_HELP;
        } else if (strcmp(argv[i], "-t") == 0) {
            argscont->args[idx].arg_type = ARG_TIME;
        } else if (strcmp(argv[i], "-w") == 0) {
            argscont->args[idx].arg_type = ARG_WRITELIST;
        } else if (strcmp(argv[i], "-D") == 0) {
            argscont->args[idx].arg_type = ARG_BIGD;
        } else if (strcmp(argv[i], "-r") == 0) {
            argscont->args[idx].arg_type = ARG_READS;
        } else {
            continue;
        }

        argscont->args[idx].value = argv[i+1];
        argscont->map[argscont->args[idx].arg_type] = idx;
        ++idx;

    }
}

void args_free(args__cont__t *argscont) {
    free(argscont->args);
    free(argscont->map);
}