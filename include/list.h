#ifndef _LIST_H_
#define _LIST_H_

typedef struct list_s *list_t;

list_t list_init();
void list_destroy(list_t list);
void list_append(list_t list, void *ptr);
void *list_search(list_t list, void *what, char (*cmpfn)(void *val, void *what));
void list_delete(list_t list, void *node);

#endif