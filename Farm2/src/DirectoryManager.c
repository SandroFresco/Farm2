#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <signal.h>

#include "../header/DirectoryManager.h"
#include "../header/TaskQueue.h"

#define PATH_MAX 4096

extern int terminate;

int check_file(const char *arg)
{
    struct stat info;

    if (stat(arg, &info) == -1)
        return -1; // errore
    if (S_ISREG(info.st_mode))
        return 1; // file
    if (S_ISDIR(info.st_mode))
        return 2; // cartella

    return -1;
}

void process_file(const char *path, TaskQueue *q)
{
    // Verifica se l'input è effettivamente un file
    if (check_file(path) != 1)
    {
        fprintf(stderr, "Errore: %s non è un file valido.\n", path);
        return;
    }
    enqueueString(q, (char *)path);
}

void explore_directory(const char *path, TaskQueue *q)
{
    // Verifica se l'input è effettivamente una cartella
    if (check_file(path) != 2)
    {
        fprintf(stderr, "Errore: %s non è una cartella valida.\n", path);
        return;
    }

    DIR *dir;
    struct dirent *entry;
    char fullpath[PATH_MAX];

    if ((dir = opendir(path)) == NULL)
    {
        perror("Errore nell'apertura della directory");
        return;
    }

    // Itera sui file nella directory
    while ((entry = readdir(dir)) != NULL && !terminate)
    {
        // Ignora i riferimenti alla directory corrente e quella parent (., ..)
        if (strncmp(entry->d_name, ".", 1) == 0 || strncmp(entry->d_name, "..", 2) == 0)
            continue;

        // Costruisci il percorso completo del file o della directory
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

        // Verifica se è un file o una sottocartella
        int check_result = check_file(fullpath);
        if (check_result == 1) // file
        {
            // Processa il file chiamando la nuova funzione
            process_file(fullpath, q);
            if (terminate)
            {
                if (closedir(dir) == -1)
                {
                    perror("Errore durante la chiusura della directory");
                    return;
                }
                return;
            }
        }
        else if (check_result == 2) // cartella
        {
            // Esplora ricorsivamente la sottocartella
            explore_directory(fullpath, q);
        }
        else
        {
            perror("Errore nel controllo del file");
        }
    }
    if (closedir(dir) == -1)
    {
        perror("Errore durante la chiusura della directory");
        return;
    }
}

int countFiles(const char *path)
{
    DIR *dir;
    struct dirent *entry;
    char fullpath[PATH_MAX];
    int file_count = 0;

    // Verifica se il percorso è una directory
    if ((dir = opendir(path)) == NULL)
    {
        perror("Errore nell'apertura della directory");
        return -1; // Errore nell'apertura della directory
    }

    // Itera sui file nella directory
    while ((entry = readdir(dir)) != NULL)
    {
        // Ignora i riferimenti alla directory corrente e quella parent (., ..)
        if (strncmp(entry->d_name, ".", 1) == 0 || strncmp(entry->d_name, "..", 2) == 0)
            continue;

        // Costruisci il percorso completo del file o della directory
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

        // Verifica se è un file o una sottocartella
        int check_result = check_file(fullpath);
        if (check_result == 1) // file
        {
            file_count++; // Incrementa il conteggio dei file
        }
        else if (check_result == 2) // cartella
        {
            // Esplora ricorsivamente la sottocartella
            int subfolder_count = countFiles(fullpath);
            if (subfolder_count >= 0) // Se non c'è stato un errore
            {
                file_count += subfolder_count; // Aggiungi il conteggio della sottocartella
            }
        }
        else
        {
            perror("Errore nel controllo del file");
        }
    }

    if (closedir(dir) == -1)
    {
        perror("Errore durante la chiusura della directory");
        return -1;
    }
    return file_count; // Restituisci il conteggio totale dei file
}
