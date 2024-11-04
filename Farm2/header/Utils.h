#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

// Macro per l'inizializzazione di un mutex
#define INIT_MUTEX(mutex)                                      \
                                                               \
    if (pthread_mutex_init(&(mutex), NULL) != 0)               \
    {                                                          \
        perror("Errore durante l'inizializzazione del mutex"); \
        exit(EXIT_FAILURE);                                    \
    }

// Macro per il lock di un mutex
#define LOCK_MUTEX(mutex)                           \
                                                    \
    if (pthread_mutex_lock(&(mutex)) != 0)          \
    {                                               \
        perror("Errore durante il lock del mutex"); \
        exit(EXIT_FAILURE);                         \
    }

// Macro per l'unlock di un mutex
#define UNLOCK_MUTEX(mutex)                          \
                                                     \
    if (pthread_mutex_unlock(&(mutex)) != 0)         \
    {                                                \
        perror("Errore durante l'unlock del mutex"); \
        exit(EXIT_FAILURE);                          \
    }

// Macro per la distruzione di un mutex
#define DESTROY_MUTEX(mutex)                               \
                                                           \
    if (pthread_mutex_destroy(&(mutex)) != 0)              \
    {                                                      \
        perror("Errore durante la distruzione del mutex"); \
        exit(EXIT_FAILURE);                                \
    }

//-----------------------------DICHIARAZIONE FUNZIONI-------------------------------------
void safeJoin(pthread_t thread);
void *safeMalloc(size_t size);
long safeStrtol(const char *str, int base);
int readn(int fd, void *buf, size_t size);
int writen(long fd, void *buf, size_t size);
#endif
