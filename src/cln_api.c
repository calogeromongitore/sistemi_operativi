#include "../include/api.h"

#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>

#include "../include/common.h"
#include "../include/reqframe.h"

#define SIZE_OF(of) (sizeof(reqcode_t) + sizeof of)

static struct sockaddr_un remote; 
static int sfd = -1;
static char socketname[108] = "";


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

static int retry(struct timespec timeout, struct timespec *abstime) {
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

    if (sfd != -1) {
        errno = EMFILE;
        return -1;
    }

    IF_RETEQ(sfd = socket(AF_UNIX, SOCK_STREAM, 0), -1);

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

    strcpy(socketname, sockname);

    return 1;
}

int closeConnection(const char* sockname) {

    if (strcmp(socketname, sockname)) {
        errno = ENOENT;
        return -1;
    } 

    IF_RETEQ(close(sfd), -1);
    sfd = -1;

    return 1;
}

int openFile(const char* pathname, int flags) {
    char reqframe[2048];
    struct reqcall reqc;
    size_t reqsize;

    if (sfd == -1) {
        return -1; //TODO: set errno
    }

    reqcall_default(&reqc);
    reqc.pathname = pathname;
    reqc.N = 1;
    reqc.flags = flags;

    prepareRequest((char *)reqframe, &reqsize, REQ_OPEN, &reqc);
    if (write(sfd, reqframe, reqsize) != reqsize) {
        return -1;
    }

    read(sfd, reqframe, sizeof(reqcode_t));
    if (*((reqcode_t *)reqframe) == REQ_FAILED) {
        errno = EACCES;
        return -1;
    }

    return 0;
}

int readFile(const char* pathname, void** buf, size_t* size) {
    char reqframe[2048];
    struct reqcall reqc;
    size_t reqsize, filesize;

    if (sfd == -1) {
        return -1; //TODO: set errno
    }

    *size = 0;

    reqcall_default(&reqc);
    reqc.pathname = pathname;
    reqc.N = 1;

    prepareRequest((char *)reqframe, &reqsize, REQ_GETSIZ, &reqc);
    if (write(sfd, reqframe, reqsize) != reqsize) {
        return -1;
    }

    read(sfd, reqframe, SIZE_OF(filesize));
    if (*((reqcode_t *)reqframe) == REQ_FAILED) {
        errno = EACCES;
        return -1;
    }

    memcpy(&filesize, reqframe + sizeof(reqcode_t), sizeof filesize);


    prepareRequest((char *)reqframe, &reqsize, REQ_READ, &reqc);
    if (write(sfd, reqframe, reqsize) != reqsize) {
        return -1;
    }

    read(sfd, reqframe, sizeof(reqcode_t));
    if (*((reqcode_t *)reqframe) == REQ_FAILED) {
        errno = EACCES;
        return -1;
    }

    *buf = (char *)malloc(filesize * sizeof(char));
    *size = read(sfd, *buf, filesize);

    return 0;
}

int closeFile(const char* pathname) {
    char reqframe[2048];
    struct reqcall reqc;
    size_t reqsize;

    if (sfd == -1) {
        return -1; //TODO: set errno
    }

    reqcall_default(&reqc);
    reqc.pathname = pathname;
    reqc.N = 1;

    prepareRequest((char *)reqframe, &reqsize, REQ_CLOSEFILE, &reqc);
    if (write(sfd, reqframe, reqsize) != reqsize) {
        return -1;
    }

    read(sfd, reqframe, sizeof(reqcode_t));
    if (*((reqcode_t *)reqframe) == REQ_FAILED) {
        errno = EACCES;
        return -1;
    }

    return 0;
}

int lockFile(const char* pathname) {
    char reqframe[2048];
    struct reqcall reqc;
    size_t reqsize;

    if (sfd == -1) {
        return -1; //TODO: set errno
    }

    reqcall_default(&reqc);
    reqc.pathname = pathname;
    reqc.N = 1;

    prepareRequest((char *)reqframe, &reqsize, REQ_LOCK, &reqc);
    if (write(sfd, reqframe, reqsize) != reqsize) {
        return -1;
    }

    read(sfd, reqframe, sizeof(reqcode_t));
    if (*((reqcode_t *)reqframe) == REQ_FAILED) {
        errno = EACCES;
        return -1;
    }

    return 0;
}

int unlockFile(const char* pathname) {
    char reqframe[2048];
    struct reqcall reqc;
    size_t reqsize;

    if (sfd == -1) {
        return -1; //TODO: set errno
    }

    reqcall_default(&reqc);
    reqc.pathname = pathname;
    reqc.N = 1;

    prepareRequest((char *)reqframe, &reqsize, REQ_UNLOCK, &reqc);
    if (write(sfd, reqframe, reqsize) != reqsize) {
        return -1;
    }

    read(sfd, reqframe, sizeof(reqcode_t));
    if (*((reqcode_t *)reqframe) == REQ_FAILED) {
        errno = EACCES;
        return -1;
    }

    return 0;
}

int removeFile(const char* pathname) {
    char reqframe[2048];
    struct reqcall reqc;
    size_t reqsize;

    if (sfd == -1) {
        return -1; //TODO: set errno
    }

    reqcall_default(&reqc);
    reqc.pathname = pathname;
    reqc.N = 1;

    prepareRequest((char *)reqframe, &reqsize, REQ_REMOVE, &reqc);
    if (write(sfd, reqframe, reqsize) != reqsize) {
        return -1;
    }

    read(sfd, reqframe, sizeof(reqcode_t));
    if (*((reqcode_t *)reqframe) == REQ_FAILED) {
        errno = EACCES;
        return -1;
    }

    return 0;
}

int writeFile(const char* pathname, const char* dirname) {
    char reqframe[2048];
    struct reqcall reqc;
    size_t reqsize, filesize;
    char *buf;
    int fd;
    struct stat st;

    if (sfd == -1) {
        return -1; //TODO: set errno
    }

    fd = open(pathname, O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    reqcall_default(&reqc);
    stat(pathname, &st);

    reqc.size = st.st_size;
    reqc.buf = malloc(reqc.size);
    reqc.size = read(fd, reqc.buf, reqc.size);
    reqc.pathname = pathname;
    reqc.diname = dirname;
    reqc.N = 1;

    close(fd);
    prepareRequest((char *)reqframe, &reqsize, REQ_WRITE, &reqc);
    if (write(sfd, reqframe, reqsize) != reqsize) {
        return -1;
    }

    read(sfd, reqframe, SIZE_OF(filesize));
    if (*((reqcode_t *)reqframe) == REQ_FAILED) {
        errno = EACCES;
        return -1;
    }

    memcpy(&filesize, reqframe + sizeof(reqcode_t), sizeof filesize);

    if (dirname) {
        buf = (char *)malloc(filesize * sizeof(char));
        read(sfd, buf, filesize);

        read(sfd, &filesize, sizeof filesize);
        read(sfd, reqframe, filesize);
        reqframe[filesize] = '\0';
        if (filesize > 0) printf("returned file %s: %s\n", reqframe, buf);
    }

    return 0;
}