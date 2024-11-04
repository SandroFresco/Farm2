#ifndef THREADPOOLWORKER_H
#define THREADPOOLWORKER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "../header/TaskQueue.h"

//--------------------STRUCT-------------------------------

typedef struct
{
    int *nThread;
    pthread_t *threads; // array di worker
    TaskQueue *fileQueue;
} ThreadPool;

void threadPoolInitWorker(TaskQueue *fileQueue, int *nThread, int queueLength);
void threadPoolStartWorker();
void threadPoolDestroyWorker();
void decreaseWorker(pthread_t thread);
void addTask(int argc, char *argv[], int optind, char **folderArray, int nFolder, int folderFlag);

#endif