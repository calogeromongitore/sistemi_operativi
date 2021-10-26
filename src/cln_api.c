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

static struct sockaddr_un remote; 
static int sfd = -1;
static char socketname[108] = "";
static struct timespec _ms = {.tv_sec = 0, .tv_nsec = 0};
static struct timespec lastcall = {.tv_sec = 0, .tv_nsec = 0};


static inline void timespec_diff(struct timespec *a, struct timespec *b, struct timespec *result) {

    result->tv_sec = a->tv_sec - b->tv_sec;
    result->tv_nsec = a->tv_nsec - b->tv_nsec;

    // nel caso in cui a < b,
    // il risultato viene sistemato calcolando l'eccedenza della sottrazione
    // non dovrebbe mai verificarsi perchè esternamente a > b sempre
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

    // possono accadere due casi: timeout > abstime o l'inverso
    cmp = timecmp(timeout, *abstime);

    // se cmp > 0 significa che timeout > abstime, quindi devo solo aspettare abstime
    if (cmp > 0) {
        // printf ("abstime: %ld sec, %ld ns\n", abstime->tv_sec, abstime->tv_nsec);
        nanosleep(abstime, NULL);
    } else {
        // printf ("timeout: %ld sec, %ld ns\n", timeout.tv_sec, timeout.tv_nsec);
        nanosleep(&timeout, NULL);

        // sottraggo ad abstime il valore di timeout.
        // il risultato viene salvato in cmpres
        timespec_diff(abstime, &timeout, &cmpres);
        *abstime = cmpres;
    }

    return cmp;
}

static void wait_interval() {
    struct timespec nowcall, diffcall, diffwait;

    // prendo il tempo di esecuzione attuale del thread e lo salvo in nowcall
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &nowcall);

    if (!(lastcall.tv_sec == 0 && lastcall.tv_nsec == 0)) {
        timespec_diff(&nowcall, &lastcall, &diffcall); // calcolo tempo trascorso dall'attuale wait_interval() e la precedente
        if (timecmp(diffcall, _ms) < 0) { // se è passato troppo poco tempo rispetto al timeout preimpostato, devo aspettare
            timespec_diff(&_ms, &diffcall, &diffwait); // l'attesa è pari alla differenza di tempo tra le due chiamate e il timeout
            nanosleep(&diffwait, NULL);
        }
    }

    lastcall = nowcall;
}

void set_interval(int ms) {
    _ms.tv_sec = ms / 1000;
    _ms.tv_nsec = (ms % 1000) * 1e6;
}

int openConnection(const char* sockname, int msec, const struct timespec abstime) {
    struct timespec timeout, abstime_int;
    int len;

    // torno errore se sfd != -1, cioè se è già stata effettuata una connessione con successo
    if (sfd != -1) {
        errno = EMFILE;
        return -1;
    }

    // controllo che l'operazione di creazionel del socket vada a buon fine
    // se torna -1, allora faccio return -1
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
        errno = ENOENT; // No such file or directory
        return -1;
    } 

    sleep(1);
    IF_RETEQ(close(sfd), -1);
    sfd = -1;

    return 1;
}

int openFile(const char* pathname, int flags) {
    char reqframe[2048];
    struct reqcall reqc;
    size_t reqsize;
    int err, len;

    // controllo che è stata effettuata la connessione, altrimenti torno errore
    if (sfd == -1) {
        errno = ENOTCONN;
        return -1; 
    }

    // trovo la prima slash partendo dalla fine del path e mi memorizzo 
    // la sua posizione in len
    for (len = strlen(pathname) - 1; len >= 0; len--) {
        if (pathname[len] == '/') {
            break;
        }
    }

    // preparo struct per la richiesta
    reqcall_default(&reqc);
    reqc.pathname = pathname + ++len;
    reqc.N = 1;
    reqc.flags = flags;

    // preparo i byte da inviare al server
    prepareRequest((char *)reqframe, &reqsize, REQ_OPEN, &reqc);
    wait_interval();
    if (write(sfd, reqframe, reqsize) != reqsize) { // invio richiesta al server
        errno = EPIPE;
        return -1;
    }

    // aspetto e ricevo la risposta dal server
    // leggo reqframe
    read(sfd, reqframe, sizeof(reqcode_t)); 
    if (*((reqcode_t *)reqframe) == REQ_FAILED) {
        read(sfd, &err, sizeof err);
        seterrno_of(err);
        return -1;
    }

    return 0;
}

int readFile(const char* pathname, void** buf, size_t* size) {
    char reqframe[2048];
    struct reqcall reqc;
    size_t reqsize, filesize;
    int err, len; 

    if (sfd == -1) {
        errno = ENOTCONN;
        return -1; 
    }

    for (len = strlen(pathname) - 1; len >= 0; len--) {
        if (pathname[len] == '/') {
            break;
        }
    }

    *size = 0;

    reqcall_default(&reqc);
    reqc.pathname = pathname + ++len;
    reqc.N = 1;

    prepareRequest((char *)reqframe, &reqsize, REQ_GETSIZ, &reqc);
    wait_interval();
    if (write(sfd, reqframe, reqsize) != reqsize) {
        errno = EPIPE;
        return -1;
    }

    read(sfd, reqframe, sizeof(reqcode_t));
    if (*((reqcode_t *)reqframe) == REQ_FAILED) {
        read(sfd, &err, sizeof err);
        seterrno_of(err);
        return -1;
    }

    read(sfd, &filesize, sizeof filesize);

    prepareRequest((char *)reqframe, &reqsize, REQ_READ, &reqc);
    wait_interval();
    if (write(sfd, reqframe, reqsize) != reqsize) {
        errno = EPIPE;
        return -1;
    }

    read(sfd, reqframe, sizeof(reqcode_t));
    if (*((reqcode_t *)reqframe) == REQ_FAILED) {
        read(sfd, &err, sizeof err);
        seterrno_of(err);
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
    int err, len;

    if (sfd == -1) {
        errno = ENOTCONN;
        return -1; 
    }

    for (len = strlen(pathname) - 1; len >= 0; len--) {
        if (pathname[len] == '/') {
            break;
        }
    }

    reqcall_default(&reqc);
    reqc.pathname = pathname + ++len;
    reqc.N = 1;

    prepareRequest((char *)reqframe, &reqsize, REQ_CLOSEFILE, &reqc);
    wait_interval();
    if (write(sfd, reqframe, reqsize) != reqsize) {
        errno = EPIPE;
        return -1;
    }

    read(sfd, reqframe, sizeof(reqcode_t));
    if (*((reqcode_t *)reqframe) == REQ_FAILED) {
        read(sfd, &err, sizeof err);
        seterrno_of(err);
        return -1;
    }

    return 0;
}

int lockFile(const char* pathname) {
    char reqframe[2048];
    struct reqcall reqc;
    size_t reqsize;
    int err, len;

    if (sfd == -1) {
        errno = ENOTCONN;
        return -1;
    }

    for (len = strlen(pathname) - 1; len >= 0; len--) {
        if (pathname[len] == '/') {
            break;
        }
    }

    reqcall_default(&reqc);
    reqc.pathname = pathname + ++len;
    reqc.N = 1;

    prepareRequest((char *)reqframe, &reqsize, REQ_LOCK, &reqc);
    wait_interval();
    if (write(sfd, reqframe, reqsize) != reqsize) {
        errno = EPIPE;
        return -1;
    }

    read(sfd, reqframe, sizeof(reqcode_t));
    if (*((reqcode_t *)reqframe) == REQ_FAILED) {
        read(sfd, &err, sizeof err);
        seterrno_of(err);
        return -1;
    }

    return 0;
}

int unlockFile(const char* pathname) {
    char reqframe[2048];
    struct reqcall reqc;
    size_t reqsize;
    int err, len;

    if (sfd == -1) {
        errno = ENOTCONN;
        return -1;
    }

    for (len = strlen(pathname) - 1; len >= 0; len--) {
        if (pathname[len] == '/') {
            break;
        }
    }

    reqcall_default(&reqc);
    reqc.pathname = pathname + ++len; 
    reqc.N = 1;

    prepareRequest((char *)reqframe, &reqsize, REQ_UNLOCK, &reqc);
    wait_interval();
    if (write(sfd, reqframe, reqsize) != reqsize) {
        errno = EPIPE;
        return -1;
    }

    read(sfd, reqframe, sizeof(reqcode_t));
    if (*((reqcode_t *)reqframe) == REQ_FAILED) {
        read(sfd, &err, sizeof err);
        seterrno_of(err);
        return -1;
    }

    return 0;
}

int removeFile(const char* pathname) {
    char reqframe[2048];
    struct reqcall reqc;
    size_t reqsize;
    int err, len;

    if (sfd == -1) {
        errno = ENOTCONN;
        return -1;
    }

    for (len = strlen(pathname) - 1; len >= 0; len--) {
        if (pathname[len] == '/') {
            break;
        }
    }

    reqcall_default(&reqc);
    reqc.pathname = pathname + ++len;
    reqc.N = 1;

    prepareRequest((char *)reqframe, &reqsize, REQ_REMOVE, &reqc);
    wait_interval();
    if (write(sfd, reqframe, reqsize) != reqsize) {
        errno = EPIPE;
        return -1;
    }

    read(sfd, reqframe, sizeof(reqcode_t));
    if (*((reqcode_t *)reqframe) == REQ_FAILED) {
        read(sfd, &err, sizeof err);
        seterrno_of(err);
        return -1;
    }

    return 0;
}

int writeFile(const char* pathname, const char* dirname) {
    char reqframe[2048], *fname;
    char *buf2;
    struct reqcall reqc;
    size_t reqsize, filesize, rem;
    char *buf;
    int fd, len;
    struct stat st;
    int err;

    if (sfd == -1) {
        errno = ENOTCONN;
        return -1;
    }

    fd = open(pathname, O_RDONLY);
    if (fd < 0) {
        errno = ENOENT;
        return -1;
    }

    reqcall_default(&reqc);
    stat(pathname, &st); // richiedo informazioni sul file (sono interessato alla dimensione del file da scrivere sul server)

    for (len = strlen(pathname) - 1; len >= 0; len--) {
        if (pathname[len] == '/') {
            break;
        }
    }

    len++;
    reqc.size = st.st_size;
    // se voglio scrivere un file piccolo < CHUNK_SIZE, lo faccio direttamente con la write
    // quindi mando il dato con la richiesta (campo .buf)
    // altrimenti faccio una write di dimensione 0 e saranno successive append
    // a scrivere l'intero file
    reqc.buf = (st.st_size > CHUNK_SIZE) ? NULL : malloc(reqc.size);
    reqc.size = (st.st_size > CHUNK_SIZE) ? 0 : read(fd, reqc.buf, reqc.size);
    reqc.pathname = pathname + len;
    reqc.diname = dirname;
    reqc.N = 1;

    close(fd);
    prepareRequest((char *)reqframe, &reqsize, REQ_WRITE, &reqc);
    wait_interval();
    if (write(sfd, reqframe, reqsize) != reqsize) {
        errno = EPIPE;
        return -1;
    }

    // assegnazione a rem non utilizzata
    rem = read(sfd, reqframe, sizeof(reqcode_t));
    if (*((reqcode_t *)reqframe) == REQ_FAILED) {
        read(sfd, &err, sizeof err);
        seterrno_of(err);
        return -1;
    }

    read(sfd, &rem, sizeof rem);
    while (rem--) {
        // leggo lunghezza file 
        read(sfd, &filesize, sizeof filesize);
        buf2 = (char *)malloc(filesize * sizeof(char));
        // leggo l'intero file (conoscendone la lunghezza)
        read(sfd, buf2, filesize);

        // leggo lunghezza nome file
        read(sfd, &reqsize, sizeof reqsize);
        // leggo l'intero nome (conoscendone la lunghezza)
        read(sfd, reqframe, reqsize);
        reqframe[reqsize] = '\0';

        if (filesize > 0) {
            fname = newstrcat(dirname ? dirname : "/dev/", dirname ? reqframe : "null"); // se non è null considero prima opzione if
            PERROR_DIE(fd = open(fname, O_WRONLY | O_CREAT, 0644), -1);
            write(fd, buf2, filesize);
            close(fd);
            free(fname);
        }

        free(buf2);
    }

    return 0;
}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname) {
    char reqframe[2048], *fname;
    struct reqcall reqc;
    size_t reqsize, filesize, rem;
    char *buf2;
    int fd, len;
    int err;

    if (sfd == -1) {
        errno = ENOTCONN;
        return -1;
    }

    reqcall_default(&reqc);

    for (len = strlen(pathname) - 1; len >= 0; len--) {
        if (pathname[len] == '/') {
            break;
        }
    }

    reqc.buf = buf;
    reqc.size = size;
    reqc.pathname = pathname + ++len;
    reqc.diname = dirname;
    reqc.N = 1;

    prepareRequest((char *)reqframe, &reqsize, REQ_APPEND, &reqc);
    wait_interval();
    if (write(sfd, reqframe, reqsize) != reqsize) {
        errno = EPIPE;
        return -1;
    }

    rem = read(sfd, reqframe, sizeof(reqcode_t));
    if (*((reqcode_t *)reqframe) == REQ_FAILED) {
        read(sfd, &err, sizeof err);
        seterrno_of(err);
        return -1;
    }

    read(sfd, &rem, sizeof rem);
    while (rem--) {
        read(sfd, &filesize, sizeof filesize);
        buf2 = (char *)malloc(filesize * sizeof(char));
        read(sfd, buf2, filesize);

        read(sfd, &reqsize, sizeof reqsize);
        read(sfd, reqframe, reqsize);
        reqframe[reqsize] = '\0';

        if (filesize > 0) {
            fname = newstrcat(dirname ? dirname : "/dev/", dirname ? reqframe : "null");
            PERROR_DIE(fd = open(fname, O_WRONLY | O_CREAT, 0644), -1);
            write(fd, buf2, filesize);
            close(fd);
            free(fname);
        }

        free(buf2);
    }

    return 0;
}

int readNFiles(int N, const char* dirname) {
    char reqframe[2048], *fname;
    struct reqcall reqc;
    size_t reqsize, filesize, rem;
    char *buf2;
    int fd, len, lenret = 0;
    int err;

    if (sfd == -1) {
        errno = ENOTCONN;
        return -1;
    }

    reqcall_default(&reqc);
    reqc.diname = dirname;
    reqc.N = N;

    prepareRequest((char *)reqframe, &reqsize, REQ_RNDREAD, &reqc);
    wait_interval();
    if (write(sfd, reqframe, reqsize) != reqsize) {
        errno = EPIPE;
        return -1;
    }

    read(sfd, reqframe, sizeof(reqcode_t));
    if (*((reqcode_t *)reqframe) == REQ_FAILED) {
        read(sfd, &err, sizeof err);
        seterrno_of(err);
        return -1;
    }

    read(sfd, &rem, sizeof rem);
    lenret = rem;

    while (rem--) {
        read(sfd, &filesize, sizeof filesize);
        buf2 = (char *)malloc(filesize * sizeof(char));
        read(sfd, buf2, filesize);

        read(sfd, &reqsize, sizeof reqsize);
        read(sfd, reqframe, reqsize);
        reqframe[reqsize] = '\0';

        if (filesize > 0) {
            fname = newstrcat(dirname ? dirname : "/dev/", dirname ? reqframe : "null");
            PERROR_DIE(fd = open(fname, O_WRONLY | O_CREAT, 0644), -1);
            write(fd, buf2, filesize);
            close(fd);
            free(fname);
        }

        free(buf2);
    }

    return lenret;
}
