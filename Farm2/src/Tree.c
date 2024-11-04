#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#include "../header/Tree.h"
#include "../header/Utils.h"

extern pthread_mutex_t mutex;
extern volatile sig_atomic_t stopThread;

// inserimento in un albero
TreeNode *insertTree(TreeNode *root, long num, char *filename)
{
    if (root == NULL)
    {
        TreeNode *new_node = safeMalloc(sizeof(TreeNode));
        new_node->num = num;
        new_node->filename = filename;
        new_node->left = new_node->right = NULL;
        return new_node;
    }

    if (num < root->num)
    {
        root->left = insertTree(root->left, num, filename);
    }
    else
    {
        root->right = insertTree(root->right, num, filename);
    }

    return root;
}

// funzione effettiva di stampa
void printTree(TreeNode *root)
{
    if (root != NULL)
    {
        printTree(root->left);
        printf("%ld %s\n", root->num, root->filename);
        printTree(root->right);
    }
}

// funzione eseguita dal thread per stampare
void *printTreeThread(void *arg)
{
    TreeNode **rootPtr = (TreeNode **)arg;

    while (!stopThread)
    {
        sleep(1); // Pausa prima di controllare nuovamente
        LOCK_MUTEX(mutex);
        if (!stopThread)
        {
                printTree(*rootPtr); // Passa l'albero corrente da stampare
        }
        UNLOCK_MUTEX(mutex);
    }

    return NULL; // Termina il thread quando il flag è impostato a 1
}

void freeTree(TreeNode *root)
{
    if (root != NULL)
    {
        freeTree(root->left);
        freeTree(root->right);
        free(root->filename);
        free(root);
    }
}