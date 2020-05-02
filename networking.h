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
typedef enum messagetype {createnet, destroynet, projectnet, rollbacknet, versionnet, filenet, commitnet, pushnet, data, responsenet, invalidnet} messagetype;
typedef struct NetworkCommand {
    messagetype type;
    int argc;
    char** argv;
    int* arglengths;
} NetworkCommand;

#define ARGDELIM ':'

int connecttohost(char* remote, int port);

NetworkCommand* readMessage(int sockfd);
void freeCMND(NetworkCommand* command);
char* readSection(int fd, int upperbound, char** contbuffer, int* buffersize, int* populatedbytes, int ignoredelim);
int sendNetworkCommand(NetworkCommand* command, int sockfd);

NetworkCommand* newFailureCMND_B(char* commandName, char* reason, int reseon_len);
NetworkCommand* newSuccessCMND_B(char* commandName, char* reason, int reseon_len);
NetworkCommand* newFailureCMND(char* commandName, char* reason);
NetworkCommand* newSuccessCMND(char* commandName, char* reason);

#endif
