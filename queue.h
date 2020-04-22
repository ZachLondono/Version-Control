#ifndef QUEUE
#define QUEUE

#include <stdlib.h>

typedef struct node {
    struct node* next;
    int* socket;
} QueueNode;

void enqueue(int* socket);
int* dequeue();

#endif