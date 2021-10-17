#include "../include/list.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct lnode {
    void *value;
    struct lnode *next;
    struct lnode *prev;
};

struct list_s {
    struct lnode *head;
    struct lnode *tail;
    int N;
};

list_t list_init() {
    list_t list;

    list = (list_t)malloc(sizeof *list);
    list->N = 0;
    list->head = list->tail = NULL;

    return list;
}

void list_destroy(list_t list) {

    while(list->N) {
        list_delete(list, 0);
    }    

    free(list);
}

void list_append(list_t list, void *ptr) {
    struct lnode *node, *toadd;

    toadd = malloc(sizeof *toadd);

    toadd->prev = list->tail;
    if (list->tail != NULL) {
        list->tail->next = toadd;
    }

    toadd->next = NULL;
    toadd->value = ptr;
    list->tail = toadd;

    if (!list->head) {
        list->head = list->tail;
    }

    ++list->N;
}

void *list_search(list_t list, void *what, char (*cmpfn)(void *val, void *what)) {
    struct lnode *node;

    for (node = list->head; node != NULL; node = node->next) {
        if (cmpfn(node->value, what)) {
            return (void *)node;
        }
    }

    return NULL;
}

void list_delete(list_t list, void *nodeptr) {
    struct lnode *node;

    for (node = list->head; node != NULL; node = node->next) {
        if (node == nodeptr) {

            if (node->prev == NULL) {
                list->head = node->next;
                if (node->next) {
                    node->next->prev = NULL;
                }
            } else {

                if (list->tail == nodeptr) {
                    list->tail = list->tail->prev;
                }

                if (node->prev) {
                    node->prev->next = node->next;
                }
            }

            list->N--;
            if (!list->N) {
                list->tail = list->head;
            }

            free(node);
            return;
        }
    }

}

void *list_getfirst(list_t list) {

    if (list->head == NULL) {
        return NULL;
    }

    return list->head->value;
}

void *list_getvalue(list_t list, void *nodeptr) {
    return ((struct lnode *)nodeptr)->value;
}