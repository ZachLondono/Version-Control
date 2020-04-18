#ifndef NETWORKING
#define NETWORKING

#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "sharedfunctions.h"

// contains various functions which aid in sending/recieving messages which abide by the specified network protocol

#define CMND_NAME_MAX 9
typedef enum messagetype {checknet, createnet, destroynet, projectnet, rollbacknet, versionnet, filenet, responsenet, invalidnet} messagetype;
typedef struct NetworkCommand {
    messagetype type;
    int argc;
    char** argv;
    int* arglengths;
    int (*operation)(struct NetworkCommand*, int);
} NetworkCommand;

#define CMNDPREFIX '$'
#define ARGDELIM ':'

int connecttohost(char* remote, int port);

NetworkCommand* readMessage(int sockfd);
void freeCMND(NetworkCommand* command);
NetworkCommand* newFailureCMND(char* commandName, char* reason);
char* readSection(int fd, int upperbound, char** contbuffer, int* buffersize, int ignoredelim);
int sendNetworkCommand(NetworkCommand* command, int sockfd);

int _responsenet(NetworkCommand* command, int sockfd);
int _checknet(NetworkCommand* command, int sockfd);
int _createnet(NetworkCommand* command, int sockfd);
int _destroynet(NetworkCommand* command, int sockfd);
int _projectnet(NetworkCommand* command, int sockfd);
int _rollbacknet(NetworkCommand* command, int sockfd);
int _versionnet(NetworkCommand* command, int sockfd);
int _filenet(NetworkCommand* command, int sockfd);


#endif