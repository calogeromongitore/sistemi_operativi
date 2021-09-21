#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define PERROR_DIE(action, eq) if ((action) == eq) {\
        perror("ERROR! ");\
        exit(errno);}

#define IF_RET(action, eq, retval) if ((action) == eq) return retval;
#define IF_RETEQ(action, eq) if ((action) == eq) return eq;


#endif