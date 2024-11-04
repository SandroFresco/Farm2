#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include<unistd.h>

#include "../header/Utils.h"
#include "../header/TaskQueue.h"
#include "../header/DirectoryManager.h"
#include "../header/Socket.h"

long safeStrtol(const char *str, int base)
{
    char *endptr;
    errno = 0; // Resetta errno prima della chiamata

    long result = strtol(str, &endptr, base);

    // Controlla se ci sono stati problemi di overflow o underflow
    if (errno == ERANGE)
    {
        if (result == LONG_MAX)
        {
            fprintf(stderr, "Overflow: il numero è troppo grande.\n");
        }
        else if (result == LONG_MIN)
        {
            fprintf(stderr, "Underflow: il numero è troppo piccolo.\n");
        }
        writeEndSignal();
        exit(EXIT_FAILURE);
    }

    // Controlla se non è stato trovato alcun numero valido
    if (endptr == str)
    {
        fprintf(stderr, "Errore: input non valido, nessun numero trovato.\n");
        writeEndSignal();
        exit(EXIT_FAILURE);
    }

    // Controlla se ci sono caratteri non numerici dopo il numero
    if (*endptr != '\0')
    {
        fprintf(stderr, "Errore: input contiene caratteri non validi dopo il numero.\n");
        writeEndSignal();
        exit(EXIT_FAILURE);
    }

    return result;
}

void safeJoin(pthread_t thread)
{
    pthread_cancel(thread);
    if (pthread_join(thread, NULL) != 0)
    {
        fprintf(stderr, "error: pthread_join failed\n");
    }
}

void *safeMalloc(size_t size)
{
    void *ptr = malloc(size);
    if (ptr == NULL)
    {
        perror("Errore durante la malloc");
        writeEndSignal();
        exit(EXIT_FAILURE);
    }
    return ptr;
}

int writen(long fd, void *buf, size_t size)
{
    size_t left = size;
    int r;
    char *bufptr = (char *)buf;
    while (left > 0)
    {
        if ((r = write((int)fd, bufptr, left)) == -1)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (r == 0)
            return 0;
        left -= r;
        bufptr += r;
    }
    return 1;
}

int readn(int fd, void *buf, size_t size)
{
    size_t left = size;
    ssize_t r;
    char *bufptr = (char *)buf;
    while (left > 0)
    {
        r = read(fd, bufptr, left);
        if (r == -1)
        {
            if (errno == EINTR)
                continue; // Ignora i segnali
            return -1;    // Errore lettura
        }
        if (r == 0)
            return 0; // EOF
        left -= r;
        bufptr += r;
    }
    return size;
}
