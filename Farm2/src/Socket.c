#define UNIX_PATH_MAX 108
#define SOCKNAME "./farm2.sck"
#define MAX_RETRIES 10

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <signal.h>

#include "../header/Utils.h"
#include "../header/Socket.h"

extern int fd_skt; // File descriptor per il socket
extern int NUMBER_OF_ELEMENT;

extern int fd_c; // file descriptor per le accept del server

extern pthread_mutex_t lockSocket;       // lock per scrivere nel socket lato client


//---------------------------------------OPERAZIONI CLIENT-----------------------------------------
void connectToServer()
{
    int retryCount = 0;
    struct sockaddr_un sa;

    memset(&sa, 0, sizeof(struct sockaddr_un));
    sa.sun_family = AF_UNIX;
    strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);

    fd_skt = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd_skt == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // provi a connettermi se la connessione fallisce ritento, dopo MAX_RETRIES tentativi andati a male termino
    while (connect(fd_skt, (struct sockaddr *)&sa, sizeof(sa)) == -1)
    {
        if (errno == ENOENT || errno == ECONNREFUSED)
        {
            retryCount++;
            if (retryCount >= MAX_RETRIES)
            {
                fprintf(stderr, "Errore: impossibile connettersi al server dopo %d tentativi\n", MAX_RETRIES);
                close(fd_skt);
                exit(EXIT_FAILURE);
            }
            sleep(1); // Aspetta un secondo prima di riprovare
        }
        else
        {
            perror("connect");
            close(fd_skt);
            exit(EXIT_FAILURE);
        }
    }
}

void sendToServer(long sum, char *filename)
{
    // Invia la somma
    if (writen(fd_skt, &sum, sizeof(sum)) == -1)
    {
        perror("Errore invio somma");
    }

    // Invia la lunghezza del filename
    size_t filenameLength = strlen(filename) + 1;

    if (writen(fd_skt, &filenameLength, sizeof(size_t)) == -1)
    {
        perror("Errore invio lunghezza filename");
    }

    // Invia il filename
    if (writen(fd_skt, filename, filenameLength) == -1)
    {
        perror("Errore invio filename");
    }
}

void writeEndSignal()
{
    LOCK_MUTEX(lockSocket);
    long endSignal = -1;
    // Invia il segnale di fine
    if (writen(fd_skt, &endSignal, sizeof(long)) == -1)
    {
        perror("Errore durante l'invio del segnale di fine al server");
    }
    UNLOCK_MUTEX(lockSocket);
}

//---------------------------------------OPERAZIONI SERVER-----------------------------------------

void openServer(int *fd_skt)
{
    struct sockaddr_un sa_un;

    memset(&sa_un, 0, sizeof(struct sockaddr_un));
    sa_un.sun_family = AF_UNIX;
    strncpy(sa_un.sun_path, SOCKNAME, UNIX_PATH_MAX);

    *fd_skt = socket(AF_UNIX, SOCK_STREAM, 0);
    if (*fd_skt == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    if (bind(*fd_skt, (struct sockaddr *)&sa_un, sizeof(sa_un)) == -1)
    {
        perror("bind");
        close(*fd_skt);
        exit(EXIT_FAILURE);
    }

    if (listen(*fd_skt, SOMAXCONN) == -1)
    {
        perror("listen");
        close(*fd_skt);
        exit(EXIT_FAILURE);
    }

    fd_c = accept(*fd_skt, NULL, NULL);
    if (fd_c == -1)
    {
        perror("accept");
        close(*fd_skt);
        exit(EXIT_FAILURE);
    }
}


int receiveFromClient(long *num, char **filename)
{
    ssize_t bytesRead;
    ssize_t totalBytesRead = 0;
    bytesRead = read(fd_c, (char *)num + totalBytesRead, sizeof(*num) - totalBytesRead);

    if (bytesRead < 0)
    { // Errore durante la lettura
        perror("Errore lettura socket");
        exit(EXIT_FAILURE);
    }
    else if (bytesRead == 0)
        exit(EXIT_FAILURE); // Connessione chiusa dal client

    totalBytesRead += bytesRead;

    if (totalBytesRead != sizeof(*num))
        exit(EXIT_FAILURE);

    // Controlla se il numero è -1, il segnale speciale di fine
    if (*num == -1)
        return -1; // Termina il ciclo se il client ha inviato -1

    // Ricevi la lunghezza del nome del file
    size_t filenameLength;
    if (readn(fd_c, &filenameLength, sizeof(size_t)) <= 0)
    {
        perror("Errore lettura filenameLength dal socket");
        exit(EXIT_FAILURE);
    }

    // Alloca memoria per il nome del file
    if (filenameLength <= 0)
    {
        fprintf(stderr, "Errore: lunghezza del nome del file non valida: %zu\n", filenameLength);
        exit(EXIT_FAILURE);
    }

    *filename = (char *)malloc(filenameLength);

    // Ricevi il nome del file
    if (readn(fd_c, *filename, filenameLength) <= 0)
    {
        perror("Errore lettura nome file dal socket");
        free(*filename);
        exit(EXIT_FAILURE);
    }

    return 0;
}
