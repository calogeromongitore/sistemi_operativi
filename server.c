#include <stdio.h>

#include "include/logger.h"

int main() {

    printf("Hello, I'm the server!\n");

    llogp(LOG_DBG, "suca");

    return 0;
};
