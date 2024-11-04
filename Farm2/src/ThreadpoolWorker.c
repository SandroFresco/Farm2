#define _POSIX_SOURCE
#define _POSIX_C_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "../header/ThreadpoolWorker.h"
#include "../header/TaskQueue.h"
#include "../header/Worker.h"
#include "../header/Utils.h"
#include "../header/DirectoryManager.h"
#include "../header/Socket.h"

ThreadPool *threadpool = NULL;
extern pthread_mutex_t lockSignalFlag;
extern int terminate;

//-------------------------------------WORKER-------------------------------------------------
void threadPoolInitWorker(TaskQueue *fileQueue, int *nThread, int queueLength)
{
    // Allocazione della memoria per la struttura threadpool
    threadpool = safeMalloc(sizeof(ThreadPool));

    // Inizializzo le code
    queueInit(fileQueue, queueLength);

    // Alloco memoria per l'array di thread worker
    pthread_t *threads = safeMalloc(sizeof(pthread_t) * (*nThread));

    // Assegno i valori nella struttura threadpool
    threadpool->nThread = nThread;     // Assegno il numero di thread
    threadpool->threads = threads;     // Assegno il puntatore all'array di thread
    threadpool->fileQueue = fileQueue; // Assegno il puntatore alla coda di file
}

void threadPoolStartWorker() // start dei worker (in modalità detached)
{
    // Avvio i thread worker
    for (int i = 0; i < *(threadpool->nThread); i++)
    {
        pthread_attr_t attr;
        int rc = pthread_attr_init(&attr);
        if (rc != 0)
        {
            fprintf(stderr, "Errore nell'inizializzazione degli attributi del thread: %s\n", strerror(rc));
            writeEndSignal();
            exit(EXIT_FAILURE);
        }

        rc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        if (rc != 0)
        {
            fprintf(stderr, "Errore nell'impostazione del detach state: %s\n", strerror(rc));
            pthread_attr_destroy(&attr); // Pulizia degli attributi prima di uscire
            writeEndSignal();
            exit(EXIT_FAILURE);
        }

        rc = pthread_create(&threadpool->threads[i], &attr, worker_thread, threadpool->fileQueue);
        if (rc != 0)
        {
            fprintf(stderr, "Errore nella creazione del thread: %s\n", strerror(rc));
            pthread_attr_destroy(&attr); // Pulizia degli attributi prima di uscire
            writeEndSignal();
            exit(EXIT_FAILURE);
        }

        pthread_attr_destroy(&attr); // Distruggi gli attributi dopo la creazione del thread
    }
}

void threadPoolDestroyWorker() // dealloca le risorse
{
    queueDestroy(threadpool->fileQueue);
    free(threadpool->threads);
    free(threadpool);
    if (pthread_mutex_destroy(&(lockSignalFlag)) != 0)
    {
        perror("Errore durante la distruzione del mutex");
        exit(EXIT_FAILURE);
    }
}

void decreaseWorker(pthread_t thread) // Terminazione di un thread
{
    LOCK_MUTEX(lockSignalFlag);
    int threadIndex = -1;

    // Trova l'indice del thread da terminare
    for (int i = 0; i < *(threadpool->nThread); i++)
    {
        if (thread == threadpool->threads[i])
        {
            threadIndex = i;
            break;
        }
    }

    // Se il thread è stato trovato, procedi alla rimozione
    if (threadIndex != -1)
    {
        // Sostituisci il thread da terminare con l'ultimo nella lista
        threadpool->threads[threadIndex] = threadpool->threads[*(threadpool->nThread) - 1];

        // Decrementa il numero di thread attivi
        *threadpool->nThread = *threadpool->nThread - 1;

        // Rialloca la memoria per i thread e i dati relativi
        threadpool->threads = realloc(threadpool->threads, sizeof(pthread_t) * *(threadpool->nThread));

        // Controlla eventuali errori di realloc
        if (threadpool->threads == NULL)
        {
            perror("Errore durante la realloc per i thread o i dati del thread (threadpool.c)");
            writeEndSignal();
            exit(EXIT_FAILURE);
        }
    }
    UNLOCK_MUTEX(lockSignalFlag);
}


void addTask(int argc, char *argv[], int optind, char **folderArray, int nFolder, int folderFlag)
{
    if (argc - optind > 0)
    {

        // Itera sugli argomenti e processa i file
        for (int i = optind; i < argc; i++)
        {
            // Passa il file e la coda alla funzione process_file
            process_file(argv[i], threadpool->fileQueue);
            if (terminate)
            {
                return;
            }
        }
    }
    if (folderFlag == 1)
    {
        for (int i = 0; i < nFolder; i++)
        {
            explore_directory(folderArray[i], threadpool->fileQueue);
            if (terminate)
            {
                return;
            }
        }
    }
}