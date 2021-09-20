#include "../include/api.h"

#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "../include/common.h"


static struct sockaddr_un remote; 
static int sfd;

struct timespec timecmp(struct timespec a, struct timespec b) {
    struct timespec ret;

    if (a.tv_sec == b.tv_sec) {
        ret.tv_sec = 0;
        ret.tv_nsec = a.tv_nsec - b.tv_nsec;
    } else {
        ret.tv_sec = a.tv_sec - b.tv_sec;
        ret.tv_nsec = 0;
    }

    return ret;
}

struct timespec retry(const struct timespec timeout, const struct timespec abstime) {
    struct timespec cmpres;

    cmpres = timecmp(timeout, abstime);

    if (cmpres.tv_nsec > 0 || cmpres.tv_sec > 0) {
        // printf ("abstime: %ld sec, %ld ns\n", abstime.tv_sec, abstime.tv_nsec);
        nanosleep(&abstime, NULL);
    } else {
        // printf ("timeout: %ld sec, %ld ns\n", timeout.tv_sec, timeout.tv_nsec);
        nanosleep(&timeout, NULL);
    }

    return cmpres;
}

int openConnection(const char* sockname, int msec, const struct timespec abstime) {
    int res_conn, maxtimens, timeoutns, last;
    struct timespec timeout, laststruct, laststruct2, abstime_int;

    PERROR_DIE(sfd = socket(AF_UNIX, SOCK_STREAM, 0), -1);

    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, sockname);

    timeout.tv_nsec = (msec % 1000) * 1000000;
    timeout.tv_sec = (msec / 1000);
    abstime_int = laststruct = abstime;

    while(connect(sfd, (struct sockaddr *)&remote, strlen(remote.sun_path) + sizeof(remote.sun_family)) < 0) {

        if (laststruct.tv_sec == 0 && laststruct.tv_nsec == 0) {
            return -1;
        }

        laststruct2 = retry(timeout, abstime_int);
        if (laststruct2.tv_sec > 0 || laststruct2.tv_nsec > 0) {
            return -1;
        }

        laststruct = laststruct2;
        abstime_int.tv_sec = abs(laststruct.tv_sec);
        abstime_int.tv_nsec = abs(laststruct.tv_nsec);

        // printf("trying again!\n");
    }

    return 1;
}
