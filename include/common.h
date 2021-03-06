#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define O_CREATE 1
#define O_LOCK   2
#define O_NULL   4

#define E_NOSPACE -7
#define E_EXISTS  -6
#define E_DENIED  -5
#define E_NEXISTS -4
#define E_NOPEN   -3
#define E_LKNOACQ -2
#define E_GENERIC -1
#define E_ITSOK   +0
#define A_LKWAIT  +1


#define CHUNK_SIZE 256


#define PERROR_DIE(action, eq) if ((action) == eq) {\
        perror("ERROR! ");\
        exit(errno);}

#define IF_RET(action, eq, retval) if ((action) == eq) return retval;
#define IF_RETEQ(action, eq) if ((action) == eq) return eq;

char *newstrcat(const char *str1, const char *str2);
char *err_str(int errcode, char *buf);
void seterrno_of(int errcode);

#endif