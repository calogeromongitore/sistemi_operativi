#include "../include/common.h"

#include <stdlib.h>
#include <string.h>

char *newstrcat(const char *str1, const char *str2) {
    char *buf;

    buf = malloc(strlen(str1) + strlen(str2) + 1);
    memcpy(buf, str1, strlen(str1));
    memcpy(buf + strlen(str1), str2, strlen(str2) + 1);

    return buf;
}

char *err_str(int errcode, char *buf) {

    switch (errcode) {
        case E_ITSOK:
            strcpy(buf, "E_ITSOK");
            break;
        
        case E_GENERIC:
            strcpy(buf, "E_GENERIC");
            break;
        
        case E_LKNOACQ:
            strcpy(buf, "E_LKNOACQ");
            break;
        
        case E_NOPEN:
            strcpy(buf, "E_NOPEN");
            break;
        
        case E_NEXISTS:
            strcpy(buf, "E_NEXISTS");
            break;
        
        case E_DENIED:
            strcpy(buf, "E_DENIED");
            break;
        
        case E_EXISTS:
            strcpy(buf, "E_EXISTS");
            break;
        
        case E_NOSPACE:
            strcpy(buf, "E_NOSPACE");
            break;
        
        default:
            strcpy(buf, "UNDEFINED");
            break;
    }
    
    return buf;
}

void seterrno_of(int errcode) {

    switch (errcode) {
        case E_GENERIC:
            errno = ECANCELED;
            break;

        case E_LKNOACQ:
            errno = EPERM;
            break;

        case E_NOPEN:
            errno = EBADF;
            break;

        case E_NEXISTS:
            errno = ENOENT;
            break;

        case E_DENIED:
            errno = EACCES;
            break;

        case E_EXISTS:
            errno = EEXIST;
            break;

        case E_NOSPACE:
            errno = ENOSPC;
            break;
        
        default:
            break;
    }

}