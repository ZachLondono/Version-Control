#ifndef LINKEDLIST
#define LINKEDLIST
#include <stdlib.h>
typedef struct _Node {
    void* content;
    struct _Node* next;
} Node;
int insertNode(void* content);
void freeLL();
Node* getHead();
#endif