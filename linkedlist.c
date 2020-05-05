#include "linkedlist.h"

Node* LLhead;
int size = 0;

Node* getHead() {
    return LLhead;
}

int insertNode(void* content) {

    if (LLhead == NULL) {
        LLhead = malloc(sizeof(Node));
        LLhead->content = content;
        LLhead->next = NULL;
        size = 1;
        return 0;
    } else {
        Node* currNode = LLhead;
        while (currNode->next != NULL) currNode = currNode->next;
        currNode->next = malloc(sizeof(Node));
        currNode->next->content = content;
        currNode->next->next = NULL;
        ++size;
        return size - 1;
    }

    return -1;

}

void freeLL() {
    Node* temp = getHead();
    while(temp != NULL) {
        Node* tempB = temp->next;
        free(temp->content);
        free(temp);
        temp = tempB;
    }
}