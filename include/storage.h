#ifndef _STORAGE_H_
#define _STORAGE_H_

#include <stdio.h>

typedef struct storage_s *storage_t;

storage_t storage_init(size_t totstorage, int maxfiles);
void storage_destroy(storage_t storage);
void storage_insert(storage_t storage, void *buf, size_t size, char *filename, void *bufret, size_t *sizeret);
int storage_read(storage_t storage, int clientid, const char *filename, void *buf, size_t *size);
int storage_getsize(storage_t storage, int clientid, const char *filename, size_t *size);

#endif