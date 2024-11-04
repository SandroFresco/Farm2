#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sys/wait.h>

#define SOCKNAME "./farm2.sck"

#include "../header/MasterThread.h"
#include "../header/Collector.h"
#include "../header/TaskQueue.h"
#include "../header/Worker.h"
#include "../header/Utils.h"
#include "../header/Socket.h"

// creo e inizializzo i flag
volatile sig_atomic_t terminate = 0;   // flag per terminazione
volatile sig_atomic_t THREADFLAG1 = 0; // flag per aumentare il numero dei thread
volatile sig_atomic_t THREADFLAG2 = 0; // flag per diminuire il numero dei thread

volatile sig_atomic_t signalFlagStart = 0;
int countWorkerTerminate = 0;

int nThread = 4;     // numero di worker
int queueLength = 8; // lunghezza coda

TaskQueue fileQueue; // coda passata ai thread worker
pthread_t *threads;  // array di worker

pthread_mutex_t lockSignalFlag;  // lock per gestire sigsur1 e sigsur2
pthread_mutex_t lockTermination; // lock per flag di terminazione

//------------------------------------GESTIONE SEGNALI----------------------------------------

int setSignalMask(sigset_t *mask)
{
    // Azzero la maschera
    if (sigemptyset(mask) == -1)
    {
        perror("sigemptyset failed");
        return -1;
    }

    // Aggiungo i segnali alla maschera
    if (sigaddset(mask, SIGHUP) == -1 ||
        sigaddset(mask, SIGINT) == -1 ||
        sigaddset(mask, SIGQUIT) == -1 ||
        sigaddset(mask, SIGTERM) == -1 ||
        sigaddset(mask, SIGUSR1) == -1 ||
        sigaddset(mask, SIGUSR2) == -1)
    {
        perror("sigaddset failed");
        return -1;
    }

    // Ignoro SIGPIPE
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &sa, NULL) == -1)
    {
        perror("sigaction(SIGPIPE) failed");
        return -1;
    }

    // Applico la sigmask
    if (pthread_sigmask(SIG_BLOCK, mask, NULL) != 0)
    {
        perror("pthread_sigmask failed");
        return -1;
    }

    return 0;
}

void handlerFlag()
{
    if (THREADFLAG1 == 1)
    {
        // Aumenta il numero di thread di 1
        nThread++;
        threads = realloc(threads, sizeof(pthread_t) * nThread);
        if (threads == NULL)
        {
            perror("Errore durante la realloc per i thread");
            writeEndSignal();
            exit(EXIT_FAILURE);
        }

        // Inizializza gli attributi del thread
        pthread_attr_t attr;
        int result = pthread_attr_init(&attr);
        if (result != 0)
        {
            fprintf(stderr, "Errore durante l'inizializzazione degli attributi del thread: %s\n", strerror(result));
            writeEndSignal();
            exit(EXIT_FAILURE);
        }

        // Setta lo stato detached
        result = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        if (result != 0)
        {
            fprintf(stderr, "Errore durante il settaggio dello stato detached: %s\n", strerror(result));
            pthread_attr_destroy(&attr); // Libera risorse prima di uscire
            writeEndSignal();
            exit(EXIT_FAILURE);
        }

        // Crea un nuovo thread detached
        result = pthread_create(&threads[nThread - 1], &attr, worker_thread, &fileQueue);
        if (result != 0)
        {
            fprintf(stderr, "Errore nella creazione del thread: %s\n", strerror(result));
            pthread_attr_destroy(&attr); // Libera risorse prima di uscire
            writeEndSignal();
            exit(EXIT_FAILURE);
        }

        // Distruggi gli attributi del thread
        pthread_attr_destroy(&attr);

        THREADFLAG1 = 0; // Reset THREADFLAG1
    }

    if (THREADFLAG2 == -1 && nThread > 1)
    {
        // Diminuisce il numero di thread di 1
        countWorkerTerminate++;

        // Risveglia eventuali thread bloccati sulla condizione
        pthread_cond_broadcast(&fileQueue.notEmpty);

        THREADFLAG2 = 0; // Reset THREADFLAG2
    }
}

static void *signalHandler(void *arg)
{
    int sig;
    while (!terminate)
    {
        if (sigwait((sigset_t *)arg, &sig) != 0)
        {
            perror("fatal error: sigwait");
            return NULL;
        }

        if (sig == SIGHUP || sig == SIGINT || sig == SIGQUIT || sig == SIGTERM)
        {
            // Gestione dei segnali di terminazione
            LOCK_MUTEX(lockTermination);

            terminate = 1;
            UNLOCK_MUTEX(lockTermination);
            pthread_cond_broadcast(&fileQueue.notEmpty);
            pthread_cond_signal(&fileQueue.notFull);
        }
        else if (sig == SIGUSR1)
        {
            // Aumento del numero di thread
            LOCK_MUTEX(lockSignalFlag);
            if (signalFlagStart == 1)
            {
                THREADFLAG1 = 1;
                handlerFlag();
            }
            UNLOCK_MUTEX(lockSignalFlag);
        }
        else if (sig == SIGUSR2)
        {
            // Diminuzione del numero di thread
            LOCK_MUTEX(lockSignalFlag);
            if (signalFlagStart == 1)
            {
                THREADFLAG2 = -1;
                handlerFlag();
            }
            UNLOCK_MUTEX(lockSignalFlag);
        }
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc == 1) return 0;
    
    pid_t pid;
    int status;

    // Creo il processo figlio
    pid = fork();

    if (pid < 0)
    { // Errore nella creazione del processo figlio
        perror("Fork fallita");
        exit(EXIT_FAILURE);
    }

    if (pid == 0)
    { // Codice del processo figlio

        Collector(); // eseguo Collector
    }
    else
    { // codice del padre
        INIT_MUTEX(lockTermination);
        INIT_MUTEX(lockSignalFlag);

        sigset_t mask;
        if (setSignalMask(&mask) == -1)
        {
            fprintf(stderr, "fatal error: unable to configure signal handling\n");
            return -1;
        }
        pthread_t signalHandlerThread; // creo il thread che gestisce il signal handler
        if (pthread_create(&signalHandlerThread, NULL, signalHandler, (void *)&mask) != 0)
        {
            perror("pthread_create");
            return -1;
        }

        MasterThread(argc, argv); // eseguo MasterThread

        safeJoin(signalHandlerThread); // attendo la terminazione del thread
    }
    wait(&status);
    unlink(SOCKNAME); // elimina il socket
    return 0;
}