#ifndef _ARGS_H_
#define _ARGS_H_

#define ARGS_VALUE(__args, __type) (__args.args[__args.map[__type]].value)
#define ARGS_ISNULL(__args, __type) (__args.map[__type] == -1)


typedef enum {
    ARG_SETTINGS,
    ARG_SOCKETFILE,
    ARG_HELP,
    ARG_WRITELIST,
    ARG_BIGD,
    ARG_SMALLD,
    ARG_READS,
    ARG_RNDREAD,
    ARG_REMOVE,
    ARG_DELAY,
    ARG_PRINT,
    ARG_LOCK,
    ARG_UNLOCK,
    ARG_LAST_NOTVALID
} flagarg_t;

typedef struct {
    flagarg_t arg_type;
    char *value;   
} arg_t;

typedef struct {
    arg_t *args;
    int *map;
    int length;
} args__cont__t;

void parse_args(int argc, char **argv, args__cont__t *argscont);
void args_free(args__cont__t *argscont);

#endif