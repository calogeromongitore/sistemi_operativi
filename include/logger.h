#ifndef _LOGGER_
#define _LOGGER_

#define LOG_DBG "DEBUG"

int llogp(char *level, char *message);
int llogf(int fd, char *level, char *message);
void ptrace(char *frmt, ...);
void trace(char *frmt, ...);

#endif
