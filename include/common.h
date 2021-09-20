#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
#include <errno.h>

#define PERROR_DIE(action, eq) if ((action) == eq) {\
        perror("ERROR! ");\
        exit(errno);}


#endif