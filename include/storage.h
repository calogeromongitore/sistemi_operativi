#ifndef _STORAGE_H_
#define _STORAGE_H_

#include <stdio.h>

typedef struct storage_s *storage_t;

storage_t storage_init(size_t totstorage, int maxfiles);
void storage_destroy(storage_t storage);
void storage_insert(storage_t storage, void *buf, size_t size, char *filename);
int storage_read(storage_t storage, int clientid, const char *filename, void *buf, size_t *size);
int storage_getsize(storage_t storage, int clientid, const char *filename, size_t *size);
int storage_open(storage_t storage, int clientid, const char *filename, int flags);
int storage_close(storage_t storage, int clientid, const char *filename);
int storage_lock(storage_t storage, int clientid, const char *filename);
int storage_unlock(storage_t storage, int clientid, const char *filename);
int storage_remove(storage_t storage, int clientid, const char *filename);
int storage_write(storage_t storage, int clientid, void *buf, size_t size, char *filename);
int storage_append(storage_t storage, int clientid, void *buf, size_t size, char *filename);
void storage_getremoved(storage_t storage, size_t *n, void *data, size_t *datasize, char *filename, size_t *filenamesize);

#endif