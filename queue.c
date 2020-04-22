#include "queue.h"

QueueNode* head = NULL;
QueueNode* tail = NULL;

void enqueue(int *socket)  {
    QueueNode* newnode = malloc(sizeof(QueueNode));
    newnode->socket = socket;
    newnode->next = NULL;
    if (tail == NULL) head = newnode;
    else tail->next = newnode;
    tail = newnode;
}

int* dequeue() {
    if (head == NULL) {
        return NULL;
    } else {
        int *ret = head->socket;
        QueueNode *temp = head;
        head = head->next;
        if (head == NULL) tail = NULL;
        free(temp);
        return ret;
    }
}
