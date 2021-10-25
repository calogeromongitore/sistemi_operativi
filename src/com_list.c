#include "../include/list.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct lnode {
    void *value;
    struct lnode *next;
};

struct list_s {
    struct lnode *head;
    int N;
};

list_t list_init() {
    list_t list;

    list = (list_t)malloc(sizeof *list);
    list->N = 0;
    list->head = NULL;

    return list;
}

void list_destroy(list_t list) {
    struct lnode *node;

    while(list->N) {
        node = list->head->next;
        free(list->head);
        list->head = node;
        list->N--;
    }    

    free(list);
}

void list_append(list_t list, void *ptr) {
    struct lnode *node, *toadd;

    toadd = malloc(sizeof *toadd);

    for (node = list->head; node != NULL; node = node->next) {
        if (!node->next) {
            node->next = toadd;
            break;
        }
    }

    toadd->next = NULL;
    toadd->value = ptr;

    if (!list->head) {
        list->head = toadd;
    }

    ++list->N;
}

void *list_search(list_t list, void *what, char (*cmpfn)(void *val, void *what)) {
    struct lnode *node;

    for (node = list->head; node != NULL; node = node->next) {
        // what è ciò che sto cercando
        if (cmpfn(node->value, what)) {
            return (void *)node;
        }
    }

    return NULL;
}

void list_delete(list_t list, void *nodeptr) {
    struct lnode *node, *prevnode;

    for (node = list->head, prevnode = NULL; node != NULL; prevnode = node, node = node->next) {
        if (node == nodeptr) {

            if (prevnode) {
                prevnode->next = node->next;
            } else {
                list->head = node->next;
            }

            list->N--;

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