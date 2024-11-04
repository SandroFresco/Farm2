#define _POSIX_C_SOURCE 200809L
#include "../header/TaskQueue.h"
#include "../header/Utils.h"
#include "../header/Socket.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>

#define MAX_SIZE_STRING 255

// variabili esterne
extern int timer;
extern volatile sig_atomic_t terminate;

extern int countWorkerTerminate;
extern pthread_mutex_t lockSignalFlag;

int result;
// Inizializza una coda di stringhe (CODA CONCORRENTE DEI TASK DA ELABORARE)
void queueInit(TaskQueue *q, int capacity)
{
    q->data = malloc(sizeof(char *) * capacity);
    if (q->data == NULL)
    {
        perror("malloc failed");
        writeEndSignal();
        exit(EXIT_FAILURE);
    }
    q->capacity = capacity;
    q->size = 0;
    INIT_MUTEX(q->lock);

    if (pthread_cond_init(&q->notFull, NULL) != 0)
    {
        perror("Errore nell'inizializzazione della condizione notFull");
        writeEndSignal();
        exit(EXIT_FAILURE);
    }

    if (pthread_cond_init(&q->notEmpty, NULL) != 0)
    {
        perror("Errore nell'inizializzazione della condizione notEmpty");
        writeEndSignal();
        exit(EXIT_FAILURE);
    }
}

// free di una coda di stringhe
void queueDestroy(TaskQueue *q)
{
    for (int i = 0; i < q->size; i++)
    {
        free(q->data[i]);
    }
    free(q->data);

    result = pthread_mutex_destroy(&q->lock);
    if (result != 0)
    {

        exit(EXIT_FAILURE);
    }

    result = pthread_cond_destroy(&q->notFull);
    if (result != 0)
    {
        exit(EXIT_FAILURE);
    }

    result = pthread_cond_destroy(&q->notEmpty);
    if (result != 0)
    {
        exit(EXIT_FAILURE);
    }
}

// Rimuove e restituisce la stringa in testa alla coda
char *dequeueString(TaskQueue *q, int *exitResult)
{
    LOCK_MUTEX(q->lock);

    while (q->size == 0 && !terminate) // non ci sono elementi da rimuovere

    {
        LOCK_MUTEX(lockSignalFlag);
        if (countWorkerTerminate > 0)
        {
            countWorkerTerminate--;
            *exitResult = 1;
            UNLOCK_MUTEX(lockSignalFlag);
            UNLOCK_MUTEX(q->lock);
            return NULL;
        }
        UNLOCK_MUTEX(lockSignalFlag);

        result = pthread_cond_wait(&q->notEmpty, &q->lock);
        if (result != 0)
        {
            writeEndSignal();
            exit(EXIT_FAILURE);
        }
    }

    if (terminate && q->size == 0)
    {
        UNLOCK_MUTEX(q->lock);
        return NULL;
    }

    char *str = q->data[0];

    // Shift degli elementi nella coda
    for (int i = 1; i < q->size; i++)
    {
        q->data[i - 1] = q->data[i];
    }
    q->size--;

    pthread_cond_signal(&q->notFull);
    UNLOCK_MUTEX(q->lock);
    return str;
}

// aggiunge array alla coda
void enqueueString(TaskQueue *q, char *element)
{
    while (!terminate)
    {
        LOCK_MUTEX(q->lock);

        while (q->size == q->capacity && !terminate) // se la coda è piena attende
        {
            result = pthread_cond_wait(&q->notFull, &q->lock);
            if (result != 0)
            {
                writeEndSignal();
                exit(EXIT_FAILURE);
            }
        }

        if (terminate)
        {
            UNLOCK_MUTEX(q->lock);
            break;
        }

        if (q->size < q->capacity) // aggiunge un singolo elemento
        {
            q->data[q->size] = safeMalloc(sizeof(char) * MAX_SIZE_STRING);

            // copia con strncpy
            if (strncpy(q->data[q->size], element, MAX_SIZE_STRING) == NULL)
            {
                perror("Errore durante la copia dell'elemento nella coda");
                writeEndSignal();
                exit(EXIT_FAILURE);
            }

            q->size++;

            if (timer > 0) // se viene settato il timer attende
            {
                struct timespec req, rem;
                req.tv_sec = timer / 1000;
                req.tv_nsec = (timer % 1000) * 1000000L;
                if (nanosleep(&req, &rem) == -1)
                {
                    perror("nanosleep failed");
                }
            }
        }

        pthread_cond_signal(&q->notEmpty);
        UNLOCK_MUTEX(q->lock);
        break;
    }
}
