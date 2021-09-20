#ifndef _API_H_
#define _API_H_

#include <time.h>

int openConnection(const char* sockname, int msec, const struct timespec abstime);

#endif