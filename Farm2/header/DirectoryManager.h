#ifndef DIRECTORYMANAGER_H
#define DIRECTORYMANAGER_H

#include "../header/TaskQueue.h"// Include queue.h perché utilizza la struttura Queue




//-----------------------------DICHIARAZIONE FUNZIONI-------------------------------------
int countFiles(const char *path);
void process_file(const char *path, TaskQueue *q);
void explore_directory(const char *path, TaskQueue *q);

#endif 
