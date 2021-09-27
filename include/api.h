#ifndef _API_H_
#define _API_H_

#include <time.h>

int openConnection(const char* sockname, int msec, const struct timespec abstime);
int closeConnection(const char* sockname);
int openFile(const char* pathname, int flags);
int readFile(const char* pathname, void** buf, size_t* size);
int closeFile(const char* pathname);
int lockFile(const char* pathname);
int unlockFile(const char* pathname);
int removeFile(const char* pathname);
int writeFile(const char* pathname, const char* dirname);

#endif