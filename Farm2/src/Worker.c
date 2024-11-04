#include "../header/Worker.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>

#include "../header/ThreadpoolWorker.h"
#include "../header/Socket.h"
#include "../header/Utils.h"

// variabili esterne
extern volatile sig_atomic_t terminate; // flag di terminazione

extern int NUMBER_OF_ELEMENT;       // numero di elementi
extern int nActiveWorker;           // numero di worker attivi
extern int AtLeastWorkerHadStarted; // flag per non far terminare subito il main

extern pthread_mutex_t lockSocket;       // lock per scrivere nel socket
extern pthread_mutex_t lockTermination;  // lock in caso di arrivo di segnale di terminazione
extern pthread_mutex_t lockActiveWorker; // lock per far terminare il main
extern pthread_cond_t allWorkerEnds;     // condition per far terminare il main

int processed_count = 0;
//---------------------------------------------THREAD WORKER---------------------------------------------

void *worker_thread(void *arg)
{
    LOCK_MUTEX(lockActiveWorker); // incremento il numero di worker attivi
    nActiveWorker++;
    AtLeastWorkerHadStarted = 1;
    UNLOCK_MUTEX(lockActiveWorker);

    TaskQueue *sourceQueue = (TaskQueue *)arg;

    int exitResult = 0;
    while (processed_count < NUMBER_OF_ELEMENT && !terminate) // continuo a processare fino a NUMBER_OF_ELEMENT
    {
        char *filename = dequeueString(sourceQueue, &exitResult); // ritorna la stringa

        if (exitResult)
        {
            decreaseWorker(pthread_self()); // è arrivato un segnale per far terminare un thread
            break;
        }

        if (filename == NULL)
        {
            // Se il flag terminate è settato, il thread esce dopo aver inviato tutto
            if (terminate)
            {
                break;
            }
            continue; // Se la coda è vuota ma non è stato impostato terminate, continua a tentare
        }

        // Processa il file
        FILE *file = fopen(filename, "rb");
        if (file == NULL)
        {
            perror("Thread Worker: Errore apertura file");
            free(filename);
            continue;
        }

        // Legge il file e calcola la sommatoria
        if (fseek(file, 0, SEEK_END) != 0) // Controllo di errore su fseek
        {
            perror("Thread Worker: Errore fseek");
            fclose(file);
            free(filename);
            continue;
        }

        long file_size = ftell(file);
        if (file_size == -1L) // Controllo di errore su ftell
        {
            perror("Thread Worker: Errore ftell");
            fclose(file);
            free(filename);
            continue;
        }

        if (fseek(file, 0, SEEK_SET) != 0) // Controllo di errore su fseek per riportare il puntatore all'inizio
        {
            perror("Thread Worker: Errore fseek");
            fclose(file);
            free(filename);
            continue;
        }

        if (file_size % sizeof(long) != 0)
        {
            fprintf(stderr, "Thread Worker: Dimensione file non valida per %s\n", filename);
            fclose(file);
            free(filename);
            continue;
        }

        int count = file_size / sizeof(long);
        long *values = safeMalloc(count * sizeof(long));
        size_t read_size = fread(values, sizeof(long), count, file);
        if (read_size != count)
        {
            if (feof(file)) 
            {
                fprintf(stderr, "Thread Worker: Fine del file raggiunta inaspettatamente durante la lettura di %s\n", filename);
            }
            else if (ferror(file))
            {
                perror("Thread Worker: Errore lettura file");
            }
            free(values);
            fclose(file);
            free(filename);
            continue;
        }

        // Calcola la somma e invia al server
        long sum = 0;
        for (int i = 0; i < count; i++)
        {
            sum += i * values[i];
        }
        LOCK_MUTEX(lockSocket);
        sendToServer(sum, filename); // invio al server
        UNLOCK_MUTEX(lockSocket);

        LOCK_MUTEX(lockTermination);
        processed_count++;
        if (processed_count >= NUMBER_OF_ELEMENT)
        {
            // Se sono stati processati tutti gli elementi, settiamo terminate
            terminate = 1;
            pthread_cond_broadcast(&sourceQueue->notEmpty); // Sveglia tutti i thread se necessario
        }
        UNLOCK_MUTEX(lockTermination);

        free(values);
        fclose(file);
        free(filename);
    }

    LOCK_MUTEX(lockActiveWorker);
    nActiveWorker--; // decremento il numero di thread attivi
    if (nActiveWorker == 0)
    {
        pthread_cond_signal(&allWorkerEnds); // Sveglia il master thread
    }
    UNLOCK_MUTEX(lockActiveWorker);
    return NULL;
}
