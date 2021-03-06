#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdarg.h>

#include "../include/logger.h"

#define BUF_SIZ     2048
#define LOG_FORMAT  "[%s %s]: %s\n"

int llogp(char *level, char *message) {
    return llogf(STDOUT_FILENO, level, message);
}

int llogf(int fd, char *level, char *message) {
    char *buf; 
    char ct_str[0x14];
    int bytes;
    time_t ct;
    struct tm *localt;

    ct = time(NULL);
    localt = localtime(&ct);

    buf = (char *)malloc(BUF_SIZ * sizeof(char));
    sprintf(ct_str, "%02d/%02d/%04d %02d:%02d:%02d", localt->tm_mday, localt->tm_mon + 1, localt->tm_year + 1900, localt->tm_hour, localt->tm_min, localt->tm_sec);
    bytes = sprintf(buf, LOG_FORMAT, ct_str, level, message);

    bytes = write(fd, buf, bytes);

    free(buf);
    return bytes;
}

void ptrace(char *frmt, ...) {
    time_t ct;
    struct tm *localt;
    va_list argp;

    ct = time(NULL);
    localt = localtime(&ct);

    va_start(argp, frmt);

    printf("[%02d/%02d/%04d %02d:%02d:%02d] ", localt->tm_mday, localt->tm_mon + 1, localt->tm_year + 1900, localt->tm_hour, localt->tm_min, localt->tm_sec);
    vprintf(frmt, argp);
    printf("\n");

    va_end(argp);
}

void trace(char *frmt, ...) {
    time_t ct;
    struct tm *localt;
    va_list argp;
    FILE *fp;

    ct = time(NULL);
    localt = localtime(&ct);

    fp = fopen("./log.txt", "a+");

    va_start(argp, frmt);

    fprintf(fp, "[%02d/%02d/%04d %02d:%02d:%02d] ", localt->tm_mday, localt->tm_mon + 1, localt->tm_year + 1900, localt->tm_hour, localt->tm_min, localt->tm_sec);
    vfprintf(fp, frmt, argp);
    fprintf(fp, "\n");

    va_end(argp);
    fclose(fp);
}