#include "../include/storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include "../include/fifo.h"
#include "../include/list.h"
#include "../include/common.h"

#define NODE_SETNULL(node) ((node).locptr = NULL)
#define NODE_ISNULL(node) ((node).locptr == NULL)
#define NODE_FOREACH(storage, inode) for ((inode) = (storage)->memory; (inode) < (storage)->memory + (storage)->maxfiles; (inode)++)

#define STORAGE_SETSIZ(__storage, __newsize) {\
    __storage->actual_storage = __newsize;\
    if (__storage->actual_storage > __storage->info.maxsize)\
        __storage->info.maxsize = __storage->actual_storage;\
}

#define STORAGE_INCN(__storage) {\
    if (++__storage->actual_nfiles > __storage->info.maxnum)\
        __storage->info.maxnum = __storage->actual_nfiles;\
}

#define STORAGE_DECN(__storage) --__storage->actual_nfiles;


struct lockstat {
    int index;
    char locked;
};

struct openstat {
    int clientid;
    char *filename;
};

struct node {
    char *filename;
    size_t filename_length;
    void *locptr; // contiene il dato vero e proprio del file
    int size; // dimensione file
    struct lockstat locked;
    int openby[32]; // contiene i client che hanno aperto questo file
    int openby_length; // numero client che hanno aperto il file
};

struct storage_s {
    int maxfiles;   // numero massimo di file memorizzabili
    size_t totstorage; // dimensione massima dello storage
    size_t actual_storage; // dimensione attuale dello storage
    int actual_nfiles; // numero attuale di file memorizzati
    struct node *memory; // array che contiene tutti i file
    list_t tempopen; // lista di OPEN temporanee
    fifo_t fifo; // fifo contenente gli indici dei file in *memory in ordine di inserimento
    fifo_t fiforet; // fifo utilizzata per ritornare un file dopo un cache overload. contiene una copia del file stesso
    pthread_mutex_t storagemtx; // mutex
    struct storage_info info; // info storage
};

static const struct lockstat lockstat_default = { .index = -1, .locked = 0 };

static void ___remove(storage_t storage, int index, char dofree) {

    if (dofree) {
        free(storage->memory[index].locptr);
        free(storage->memory[index].filename);
    }

    STORAGE_SETSIZ(storage, storage->actual_storage - storage->memory[index].size);
    STORAGE_DECN(storage);
    NODE_SETNULL(storage->memory[index]);
}

static struct node * ___get_inode(storage_t storage, const char *filename) {
    struct node *inode;

    NODE_FOREACH(storage, inode) {
        if (!NODE_ISNULL(*inode) && !strcmp(inode->filename, filename)) {
            return inode;
        }
    }

    return NULL;
}

static int ___is_openedby(int clientid, struct node *inode) {
    int i;

    if (inode == NULL) {
        return E_GENERIC;
    }

    for (i = 0; i < inode->openby_length && inode->openby[i] != clientid; i++);    

    return i >= inode->openby_length ? E_NOPEN : i;
}

static inline char ___is_accessible(int clientid, struct node *inode) {
    return ___is_openedby(clientid, inode) || !inode->locked.locked || (inode->locked.locked && inode->openby[inode->locked.index] == clientid);
    // return ___is_openedby(clientid, inode) || !inode->locked.locked;
}

static int ___full_remove(storage_t storage, size_t size, struct node *inode) {
    int i, nfiles, inode_i;

    i = inode_i = -1;
    nfiles = storage->actual_nfiles;

    // la terza condizione viene effettuata solo se passo NULL come terzo argomento
    // in questo caso, come mai non controllo anche che il numero di file non sia stato superato?
    // perch?? inode sar?? diverso da NULL solo nel caso in cui voglia fare un APPEND
    // ci?? significa che non sto aggiungendo nessun file quindi l'unica condizione
    // da controllare ?? solo quella sulla dimensione dello storage
    while (nfiles > 0 && storage->actual_storage + size > storage->totstorage || (!inode && storage->actual_nfiles == storage->maxfiles)) {

        fifo_dequeue(storage->fifo, (void *)&i, sizeof i);
        if (NODE_ISNULL(storage->memory[i])) {
            continue;
        }

        if (inode != &storage->memory[i]) {
            fifo_enqueue(storage->fiforet, &storage->memory[i], sizeof storage->memory[i]);
            ___remove(storage, i, 0); // qui passo 0 per non fare la free. Sar?? la getremoved a farne la free
            storage->info.nkills++;
        } else {
            inode_i = i;
        }

        nfiles--;
    }

    // nel caso in cui inode_i >= 0 significa che prima durante il while
    // ho trovato il file a cui voglio appendere, che non va cancellato
    // quindi lo reinserisco nella fifo
    if (inode_i >= 0) {
        fifo_enqueue(storage->fifo, (void *)&i, sizeof i);
    }

    return i;
}

static void ___flush_fifo(storage_t storage) {
    void *ptr;
    int i;

    while (fifo_usedspace(storage->fifo) > 0) {
        if ((ptr = fifo_getfirst(storage->fifo)) && (fifo_read(storage->fifo, ptr, &i, sizeof i), !NODE_ISNULL(storage->memory[i]))) {
            break;
        }

        fifo_dequeue(storage->fifo, NULL, sizeof i);
    }

}

storage_t storage_init(size_t totstorage, int maxfiles) {
    storage_t storage;
    int i, j;

    storage = (storage_t)malloc(sizeof *storage);
    storage->memory = (struct node *)malloc(maxfiles * sizeof *storage->memory);
    storage->fifo = fifo_init(maxfiles * sizeof(int));
    storage->fiforet = fifo_init(maxfiles * sizeof(struct node));
    storage->totstorage = totstorage;
    storage->maxfiles = maxfiles;
    storage->actual_nfiles = 0;
    storage->actual_storage = 0;
    storage->tempopen = list_init();

    storage->info.maxnum = 0;
    storage->info.maxsize = 0;
    storage->info.nkills = 0;

    pthread_mutex_init(&storage->storagemtx, NULL);

    for (i = 0; i < maxfiles; i++) {
        NODE_SETNULL(storage->memory[i]);
        storage->memory[i].openby_length = 0;
        for (j = 0; j < 32; j++) {
            storage->memory[i].openby[j] = -1;
        }
    }

    return storage;
}

void storage_destroy(storage_t storage) {
    int i;
    void *val;

    pthread_mutex_lock(&storage->storagemtx);
    while (fifo_usedspace(storage->fifo) > 0) {
        fifo_dequeue(storage->fifo, &i, sizeof i);
        ___remove(storage, i, 1);
    }

    while (val = list_getfirst(storage->tempopen)) {
        free(val);
    }

    pthread_mutex_unlock(&storage->storagemtx);

    free(storage->memory);
    fifo_destroy(storage->fifo);
    fifo_destroy(storage->fiforet);
    list_destroy(storage->tempopen);
    pthread_mutex_destroy(&storage->storagemtx);
    free(storage);
}

void storage_getremoved(storage_t storage, size_t *n, void **data, size_t *datasize, char *filename, size_t *filenamesize) {
    struct node inode;

    pthread_mutex_lock(&storage->storagemtx);

    if (*n = fifo_usedspace(storage->fiforet) / sizeof(struct node)) {
        fifo_dequeue(storage->fiforet, (void *)&inode, sizeof inode);

        *datasize = inode.size;
        // se il chiamante passa data != NULL, sar?? compito suo fare la free del puntatore ritornato
        // altrimento ?? compito dello storage
        if (data) {
            *data = inode.locptr;
        } else {
            free(inode.locptr);
        }

        strcpy(filename, inode.filename);
        *filenamesize = inode.filename_length;
        
        free(inode.filename);
    }

    pthread_mutex_unlock(&storage->storagemtx);
}

void storage_insert(storage_t storage, void *buf, size_t size, char *filename) {
    int i;

    pthread_mutex_lock(&storage->storagemtx);

    while (fifo_usedspace(storage->fiforet)) {
        fifo_dequeue(storage->fiforet, NULL, sizeof(struct node));
    }

    // svuoto la fifo da possibili indici non pi?? utilizzati
    ___flush_fifo(storage);
    if ((i = ___full_remove(storage, size, NULL)) < 0) {
        for (i = 0; i < storage->maxfiles && !NODE_ISNULL(storage->memory[i]); i++);
    }

    storage->memory[i].filename_length = strlen(filename);
    storage->memory[i].filename = (char *)malloc((storage->memory[i].filename_length + 1) * sizeof *storage->memory[i].filename);
    storage->memory[i].locked = lockstat_default;
    storage->memory[i].size = size;
    storage->memory[i].locptr = malloc(size);

    strcpy(storage->memory[i].filename, filename);
    memcpy(storage->memory[i].locptr, buf, size);

    fifo_enqueue(storage->fifo, (void *)&i, sizeof i);
    STORAGE_SETSIZ(storage, storage->actual_storage + size);
    STORAGE_INCN(storage);

    pthread_mutex_unlock(&storage->storagemtx);
}

int storage_read(storage_t storage, int clientid, const char *filename, void **buf, size_t *size) {
    struct node *inode;
    int retval;

    pthread_mutex_lock(&storage->storagemtx);

    *size = 0;
    retval = E_GENERIC;

    inode = ___get_inode(storage, filename);
    if (inode == NULL) {
        retval = E_NEXISTS;
    } else if (___is_openedby(clientid, inode) == E_NOPEN) {
        retval = E_NOPEN;
    } else if (___is_accessible(clientid, inode)) {
        *size = inode->size;
        *buf = inode->locptr;
        retval = E_ITSOK;
    } else {
        retval = E_LKNOACQ;
    }

    pthread_mutex_unlock(&storage->storagemtx);
    return retval;
}

int storage_getsize(storage_t storage, int clientid, const char *filename, size_t *size) {
    struct node *inode;
    int retval;

    pthread_mutex_lock(&storage->storagemtx);

    retval = E_GENERIC;
    *size = 0;

    inode = ___get_inode(storage, filename);
    if (inode == NULL) {
        retval = E_NEXISTS;
    } else if (___is_openedby(clientid, inode) == E_NOPEN) {
        retval = E_NOPEN;
    } else if (___is_accessible(clientid, inode)) {
        *size = inode->size;
        retval = E_ITSOK;
    } else {
        retval = E_LKNOACQ;
    }

    pthread_mutex_unlock(&storage->storagemtx);
    return retval;
}

int storage_open(storage_t storage, int clientid, const char *filename, int flags) {
    struct node *inode;
    int retval;
    struct openstat *open;

    pthread_mutex_lock(&storage->storagemtx);

    retval = E_ITSOK;
    inode = ___get_inode(storage, filename);

    if (inode == NULL) {
        if (!(flags & O_CREATE)) {
            retval = E_NEXISTS;
        } else if (flags & O_LOCK) {
            open = (struct openstat *)malloc(sizeof *open);
            open->filename = malloc((strlen(filename) + 1) * sizeof *filename);
            open->clientid = clientid;

            strcpy(open->filename, filename);
            list_append(storage->tempopen, open);
        }
    } else if (flags & O_CREATE) { // se ho O_CREATE ma non ho O_LOCK e arrivo a questo if significa che il file esiste gi??
        retval = E_EXISTS;
    } else if (___is_openedby(clientid, inode) == E_NOPEN) {

        // if (inode->locked.locked && inode->openby[inode->locked.index] != clientid) {
        if (inode->locked.locked) { // se ?? gi?? bloccato da qualche altro client faccio wait
            retval = A_LKWAIT;
        } else {
            inode->openby[inode->openby_length] = clientid;

            if (flags & O_LOCK) {
                inode->locked.index = inode->openby_length;
                inode->locked.locked = 1;
            }

            ++inode->openby_length;
        }

    }

    pthread_mutex_unlock(&storage->storagemtx);
    return retval;
}

int storage_close(storage_t storage, int clientid, const char *filename) {
    struct node *inode;
    int retval, i, idx;

    pthread_mutex_lock(&storage->storagemtx);

    retval = E_ITSOK;
    inode = ___get_inode(storage, filename);

    if (inode == NULL) {
        retval = E_NEXISTS;
    } else if ((idx = ___is_openedby(clientid, inode)) == E_NOPEN) {
        retval = E_NOPEN;
    } else {

        for (i = idx; i < inode->openby_length - 1; i++) {
            inode->openby[i] = inode->openby[i + 1];
        }

        // if (inode->locked.locked && inode->locked.index == idx) {
        //     inode->locked.locked = 0;
        // }

        --inode->openby_length;
    }

    pthread_mutex_unlock(&storage->storagemtx);
    return retval;
}

int storage_lock(storage_t storage, int clientid, const char *filename) {
    struct node *inode;
    int retval, i, idx;

    pthread_mutex_lock(&storage->storagemtx);

    retval = E_ITSOK;
    inode = ___get_inode(storage, filename);

    if (inode == NULL) {
        retval = E_NEXISTS;
    } else if ((idx = ___is_openedby(clientid, inode)) == E_NOPEN) {
        retval = E_NOPEN;
    } else {
        // if (inode->locked.locked && inode->locked.index != idx) {
        if (inode->locked.locked) {
            retval = A_LKWAIT;
        } else {
            inode->locked.locked = 1;
            inode->locked.index = idx;
        }
    }

    pthread_mutex_unlock(&storage->storagemtx);
    return retval;
}

int storage_unlock(storage_t storage, int clientid, const char *filename) {
    struct node *inode;
    int retval, i, idx;

    pthread_mutex_lock(&storage->storagemtx);

    retval = E_ITSOK;
    inode = ___get_inode(storage, filename);

    if (inode == NULL) {
        retval = E_NEXISTS;
    // } else if ((idx = ___is_openedby(clientid, inode)) == E_NOPEN) {
    //     retval = E_NOPEN;
    // } else if (inode->locked.locked && inode->locked.index == idx) {
    } else if (inode->locked.locked) {
        inode->locked.locked = 0;
    }

    pthread_mutex_unlock(&storage->storagemtx);
    return retval;
}

int storage_remove(storage_t storage, int clientid, const char *filename) {
    struct node *inode;
    int retval, i, idx, j;

    pthread_mutex_lock(&storage->storagemtx);

    retval = E_ITSOK;
    inode = ___get_inode(storage, filename);

    if (inode == NULL) {
        retval = E_NEXISTS;
    } else if ((idx = ___is_openedby(clientid, inode)) == E_NOPEN) {
        retval = E_NOPEN;
    } else if (!___is_accessible(clientid, inode)) {
        retval = E_DENIED;
    } else if (!inode->locked.locked) {
        retval = E_LKNOACQ;
    } else {
        for (i = 0; strcmp(filename, storage->memory[i].filename); i++);
        ___remove(storage, i, 1);

        j = fifo_usedspace(storage->fifo);
        while (j > 0) {
            fifo_dequeue(storage->fifo, &idx, sizeof idx);
            j -= sizeof idx;

            // essendo nella fifo gli indici in ordine di inserimento
            // ad ogni dequeue devo controllare che sia quello che ho rimosso
            // altriemnti inserisco di nuovo in fifo
            if (idx != i) {
                fifo_enqueue(storage->fifo, &idx, sizeof idx);
            }
        }
    }

    pthread_mutex_unlock(&storage->storagemtx);
    return retval;
}

static char ___searchfn(void *val, void *what) {
    struct openstat *t1, *t2;
    char ret;

    t1 = (struct openstat *)val;
    t2 = (struct openstat *)what;

    ret = t1->clientid == t2->clientid && !strcmp(t1->filename, t2->filename);

    if (ret) {
        t2->filename = t1->filename;
    }

    return ret;
}

int storage_write(storage_t storage, int clientid, void *buf, size_t size, char *filename) {
    void *opened;
    int retval;
    struct openstat t1;
    struct node *inode;
    size_t storagesize;

    t1.filename = filename;
    t1.clientid = clientid;


    retval = E_ITSOK;
    pthread_mutex_lock(&storage->storagemtx);
    storagesize = storage->totstorage;
    // vado a ripescare il file temporaneo creato durante la OPEN con O_CREATE && O_LOCK
    opened = list_search(storage->tempopen, &t1, ___searchfn); // t1 ?? il file tempoeraneo che sto cercando
    pthread_mutex_unlock(&storage->storagemtx);

    if (opened == NULL) {
        retval = E_NEXISTS;
    } else if (size > storagesize) {
        retval = E_NOSPACE;
    } else {
        storage_insert(storage, buf, size, filename); // creazione file effettivo e salvataggio nello storage
        retval = storage_open(storage, clientid, filename, O_LOCK);

        pthread_mutex_lock(&storage->storagemtx);
        opened = list_search(storage->tempopen, &t1, ___searchfn);
        free(t1.filename); // free del puntatore originale al filename (malloc fatta durante la open)
        free(list_getvalue(storage->tempopen, opened));
        list_delete(storage->tempopen, opened);
        pthread_mutex_unlock(&storage->storagemtx);
    }

    return retval;
}

int storage_append(storage_t storage, int clientid, void *buf, size_t size, char *filename) {
    int retval;
    struct node *inode;
    void *copyloc;

    pthread_mutex_lock(&storage->storagemtx);

    retval = E_ITSOK;
    inode = ___get_inode(storage, filename);

    if (inode == NULL) {
        retval = E_NEXISTS;
    } else if (!___is_accessible(clientid, inode)) {
        retval = E_DENIED;
    } else if (inode->size + size > storage->totstorage) {
        retval = E_NOSPACE;
    } else {
        // free(t1.filename); TODO

        // libero spazio se necessario
        ___full_remove(storage, size, inode);

        // copia del chunk originale
        copyloc = malloc(inode->size + size);
        memcpy(copyloc, inode->locptr, inode->size);

        // copia del nuovo chunk
        memcpy(copyloc + inode->size, buf, size);

        // rimozione del chunk originale e rimpiazzo con quello
        free(inode->locptr);
        inode->locptr = copyloc;
        inode->size += size;
        STORAGE_SETSIZ(storage, storage->actual_storage + size);

    }

    pthread_mutex_unlock(&storage->storagemtx);
    return retval;
}

int storage_retrieve(storage_t storage, int clientid, int N) {
    struct node *inode;
    struct node cnode;
    int retval;

    pthread_mutex_lock(&storage->storagemtx);

    retval = E_ITSOK;
    
    if (!N) {
        N = storage->actual_nfiles;
    }

    NODE_FOREACH(storage, inode) {
        if (!NODE_ISNULL(*inode) && (clientid <= 0 || ___is_accessible(clientid, inode))) {

            cnode = *inode;
            cnode.filename = (char *)malloc(inode->filename_length + 1);
            cnode.locptr = malloc(inode->size);

            memcpy(cnode.locptr, inode->locptr, inode->size);
            strcpy(cnode.filename, inode->filename);
            for (cnode.openby_length = 0; cnode.openby_length < inode->openby_length; cnode.openby_length++) {
                cnode.openby[cnode.openby_length] = inode->openby[cnode.openby_length];
            }

            fifo_enqueue(storage->fiforet, &cnode, sizeof cnode);
            if (!--N) {
                break;
            }
        }
    }

    pthread_mutex_unlock(&storage->storagemtx);
    return retval;
}

void storage_getinfo(storage_t storage, struct storage_info *info) {
    *info = storage->info;
}