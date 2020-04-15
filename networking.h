#ifndef NETWORKING
#define NETWORKING

#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


#define CMNDPREFIX '$'
#define ARGDELIM ':'

int connecttohost(char* remote, int port);
int bindtoport(int fd, int port);
// Message* readMessage(int sockfd);
// int digitCount(int num);
// int getArgLen(char* buffer, int start_index);

#endif
