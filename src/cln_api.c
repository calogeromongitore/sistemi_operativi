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


static inline void timespec_diff(struct timespec *a, struct timespec *b, struct timespec *result) {

    result->tv_sec = a->tv_sec - b->tv_sec;
    result->tv_nsec = a->tv_nsec - b->tv_nsec;

    if (result->tv_nsec < 0) {
        --result->tv_sec;
        result->tv_nsec += 1000000000L;
    }

}

static inline int timecmp(struct timespec t1, struct timespec t2) {

    if (t1.tv_sec == t2.tv_sec) {
        return (t1.tv_nsec > t2.tv_nsec) ? 1 : (t1.tv_nsec == t2.tv_nsec) ? 0 : -1;
    } else {
        return (t1.tv_sec > t2.tv_sec) ? 1 : (t1.tv_sec == t2.tv_sec) ? 0 : -1;
    }

}

int retry(struct timespec timeout, struct timespec *abstime) {
    struct timespec cmpres;
    int cmp;

    cmp = timecmp(timeout, *abstime);

    if (cmp > 0) {
        // printf ("abstime: %ld sec, %ld ns\n", abstime->tv_sec, abstime->tv_nsec);
        nanosleep(abstime, NULL);
    } else {
        // printf ("timeout: %ld sec, %ld ns\n", timeout.tv_sec, timeout.tv_nsec);
        nanosleep(&timeout, NULL);

        timespec_diff(abstime, &timeout, &cmpres);
        *abstime = cmpres;
    }

    return cmp;
}

int openConnection(const char* sockname, int msec, const struct timespec abstime) {
    struct timespec timeout, abstime_int;
    int len;

    PERROR_DIE(sfd = socket(AF_UNIX, SOCK_STREAM, 0), -1);

    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, sockname);

    timeout.tv_nsec = (msec % 1000) * 1000000;
    timeout.tv_sec = (msec / 1000);
    abstime_int = abstime;

    len = strlen(remote.sun_path) + sizeof(remote.sun_family);
    while(connect(sfd, (struct sockaddr *)&remote, len) < 0) {
        if (retry(timeout, &abstime_int) > 0) {
            return -1;
        }
    }

    return 1;
}
