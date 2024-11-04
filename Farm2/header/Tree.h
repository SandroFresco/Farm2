#ifndef TREE_H
#define TREE_H

typedef struct TreeNode
{ // albero per memorizzare i file
    long num;
    char *filename;
    struct TreeNode *left;
    struct TreeNode *right;
} TreeNode;

//-----------------------------DICHIARAZIONE FUNZIONI-------------------------------------
TreeNode *insertTree(TreeNode *root, long num, char *filename);
void printTree(TreeNode *root);
void *printTreeThread(void *arg);
void freeTree(TreeNode *root);
#endif 
