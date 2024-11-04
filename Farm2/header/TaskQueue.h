#ifndef TASKQUEUE_H
#define TASKQUEUE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

//--------------------STRUCT-------------------------------

// Coda di stringhe
typedef struct
{
    char **data;
    int capacity;
    int size;
    pthread_mutex_t lock;
    pthread_cond_t notFull;
    pthread_cond_t notEmpty;
} TaskQueue;

//-----------------------------DICHIARAZIONE FUNZIONI-------------------------------------
void queueInit(TaskQueue *q, int capacity);
void queueDestroy(TaskQueue *q);
void enqueueString(TaskQueue *q, char *element);
char *dequeueString(TaskQueue *q, int* exit_result);

#endif 
