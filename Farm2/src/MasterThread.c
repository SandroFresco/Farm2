#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <stdint.h>

#include "../header/Utils.h"
#include "../header/ThreadpoolWorker.h"
#include "../header/Socket.h"
#include "../header/DirectoryManager.h"

int timer = -1;
int fd_skt; // File descriptor per il socket
int NUMBER_OF_ELEMENT = 0;
int nFolder = 0; // numero di cartelle

int nActiveWorker = 0;
int AtLeastWorkerHadStarted = 0; // flag per evitare che il main termini subito
pthread_mutex_t lockActiveWorker = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t allWorkerEnds = PTHREAD_COND_INITIALIZER;

pthread_mutex_t lockSocket; // lock per scrivere al server

extern TaskQueue fileQueue; // coda passata ai thread worker
extern int nThread;
extern int queueLength;

extern pthread_mutex_t lockSignalFlag;
extern volatile sig_atomic_t terminate;
extern volatile sig_atomic_t signalFlagStart;

//------------------------------------------MASTERTHREAD----------------------------------------
int MasterThread(int argc, char *argv[])
{
    int folderFlag = 0;
    int sum = 0;
    int count = 0;
    int opt;
    char **folderArray = NULL;
    int result = 0;

    // SOCKET
    connectToServer();

    //--------------------------------------------PARSING------------------------------------------
    while ((opt = getopt(argc, argv, "n:q:d:t:")) != -1)
    {
        switch (opt)
        {
        case 'n': // cambio il numero di thread worker
            nThread = safeStrtol(optarg, 10);
            if (nThread < 1)
            {
                fprintf(stderr, "ERROR: at least one thread required\n");
                writeEndSignal();
                _exit(EXIT_SUCCESS);
            }
            break;

        case 'q': // cambio la lunghezza della coda
            queueLength = safeStrtol(optarg, 10);
            if (queueLength < 1)
            {
                fprintf(stderr, "ERROR, THE QUEUE MUST HAVE AT LEAST ONE ELEMENT\n\n");
                writeEndSignal();
                _exit(EXIT_SUCCESS);
            }
            break;

        case 't': // imposto un timer tra un inserimento e un altro
            timer = safeStrtol(optarg, 10);
            if (timer <= 0)
            {
                printf("ERROR timer negative or zero\n");
                writeEndSignal();
                _exit(EXIT_SUCCESS);
            }
            break;

        case 'd': // passo una cartella
            count = countFiles(optarg);
            if (count == -1)
                break;
            sum = count + sum;

            // Rialloca l'array per accogliere una nuova cartella
            char **temp = realloc(folderArray, (nFolder + 1) * sizeof(char *));
            if (temp == NULL)
            {
                perror("Errore durante la realloc per folderArray");
                writeEndSignal();
                exit(EXIT_FAILURE);
            }

            folderArray = temp; // Assegna il puntatore riassegnato

            // Alloca memoria per la nuova
            folderArray[nFolder] = safeMalloc(strnlen(optarg, SIZE_MAX) + 1);
            // Copia il valore di optarg in folderArray[nFolder]
            strncpy(folderArray[nFolder], optarg, strnlen(optarg, SIZE_MAX) + 1); // Copia l'intera stringa inclusa la null

            nFolder++;
            folderFlag = 1;
            break;

        case ':':
            printf("error:option '%c' requires an argument\n", optopt);
            break;
        case '?':
            printf("error:option '%c' unrecognized\n", optopt);
            break;
        default:;
        }
    }

    if (optind >= argc && folderFlag == 0) // non ho una cartella e non ho elementi da processare
    {
        printf("ERROR, NOT ENOUGH ARGUMENTS\n");
        writeEndSignal();
        return EXIT_FAILURE;
    }

    NUMBER_OF_ELEMENT = sum + (argc - optind);

    INIT_MUTEX(lockSocket);

    //-------------------------------------------AVVIO THREAD--------------------------------------

    threadPoolInitWorker(&fileQueue, &nThread, queueLength);

    LOCK_MUTEX(lockSignalFlag);
    signalFlagStart = 1; // da ora gestisco i segnali
    UNLOCK_MUTEX(lockSignalFlag);

    // Avvio i thread worker
    threadPoolStartWorker();

    addTask(argc, argv, optind, folderArray, nFolder, folderFlag); // aggiungo gli elementi alla coda

    //--------------------------------------DEALLOCAZIONE DELLE RISORSE-----------------------------------
    LOCK_MUTEX(lockActiveWorker);

    while (nActiveWorker != 0 || AtLeastWorkerHadStarted == 0)
    {
        result = pthread_cond_wait(&allWorkerEnds, &lockActiveWorker);
        if (result != 0)
        {
            writeEndSignal();
            exit(EXIT_FAILURE);
        }
    }
    UNLOCK_MUTEX(lockActiveWorker);

    LOCK_MUTEX(lockSignalFlag);
    signalFlagStart = 0;
    UNLOCK_MUTEX(lockSignalFlag);

    writeEndSignal(); // scrivo al server per terminare

    for (int i = 0; i < nFolder; i++)
    {
        free(folderArray[i]);
    }
    free(folderArray);

    // scrivo il numero dei thread worker in un file
    FILE *file = fopen("nworkeratexit.txt", "w");
    if (file == NULL)
    {
        perror("Errore apertura file");
        exit(EXIT_FAILURE);
    }

    if (fprintf(file, "Numero di worker presenti all'uscita: %d\n", nThread) < 0)
    {
        perror("Errore scrittura nel file");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    if (fclose(file) != 0)
    {
        perror("Errore chiusura file");
        exit(EXIT_FAILURE);
    }

    threadPoolDestroyWorker(); // free delle risorse dei worker

    if (close(fd_skt) == -1)
    {
        perror("Errore chiusura socket");
    }
    return 0;
}
