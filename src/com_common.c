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
