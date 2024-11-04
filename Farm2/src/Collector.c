#define _POSIX_C_SOURCE 200112L
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "../header/Collector.h"
#include "../header/Tree.h"
#include "../header/Utils.h"
#include "../header/Socket.h"

#define UNIX_PATH_MAX 108

int fd_c;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

volatile sig_atomic_t stopThread = 0; // Flag per fermare il thread di stampa
TreeNode *root = NULL;
pthread_t printThread;

void Collector()
{
    //--------------------------------------------------GESTIONE SEGNALI------------------------------
    // Ignora SIGPIPE usando sigaction
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;  // Ignora il segnale
    sigemptyset(&sa.sa_mask); // Non mascherare nessun altro segnale
    sa.sa_flags = 0;          // Nessun flag aggiuntivo

    if (sigaction(SIGPIPE, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // Maschera segnali SIGHUP, SIGINT, SIGQUIT, SIGTERM, SIGUSR1, SIGUSR2
    sigset_t set;
    if (sigemptyset(&set) != 0)
    {
        perror("sigemptyset");
        exit(EXIT_FAILURE);
    }

    if (sigaddset(&set, SIGHUP) != 0 ||
        sigaddset(&set, SIGINT) != 0 ||
        sigaddset(&set, SIGQUIT) != 0 ||
        sigaddset(&set, SIGTERM) != 0 ||
        sigaddset(&set, SIGUSR1) != 0 ||
        sigaddset(&set, SIGUSR2) != 0)
    {
        perror("sigaddset");
        exit(EXIT_FAILURE);
    }

    // Blocca i segnali specificati
    if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0)
    {
        perror("pthread_sigmask");
        exit(EXIT_FAILURE);
    }

    //------------------------------------COMUNICAZIONE CLIENT-SERVER-------------------------------
    int fd_skt;
    openServer(&fd_skt);
    int startThread = 0;
    while (1)
    {

        long num;
        char *filename = NULL;

        if (receiveFromClient(&num, &filename) == 0)
        {
            // Inserisci il nuovo elemento nell'albero
            LOCK_MUTEX(mutex);
            root = insertTree(root, num, filename);
            UNLOCK_MUTEX(mutex);

            if (startThread == 0)









































            
            {
                startThread = 1;
                if (pthread_create(&printThread, NULL, printTreeThread, (void *)&root) != 0)
                {
                    perror("Errore durante la creazione del thread");
                    close(fd_skt);
                    close(fd_c);
                }
            }
        }
        else
            break;
    }

    // Segnala al thread di stampa di fermarsi
    stopThread = 1;

    // Attendi che il thread di stampa termini
    safeJoin(printThread);

    // Stampa l'albero in ordine (stampa finale)
    printTree(root);

    // Libera l'albero
    freeTree(root);

    close(fd_skt);
    close(fd_c);
    pthread_mutex_destroy(&mutex);
    exit(EXIT_SUCCESS);
}