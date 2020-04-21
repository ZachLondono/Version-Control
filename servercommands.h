#ifndef SERVERCMNDS
#define SERVERCMNDS
#include <pthread.h>
#include "networking.h"

int servercheckForLocalProj(char* projectname);
FileContents* serverreadfile(char* name);
ssize_t serverwrite(int fd, char* buff, int count);

int _responsenet(NetworkCommand* command, int sockfd);
int _checknet(NetworkCommand* command, int sockfd);
int _createnet(NetworkCommand* command, int sockfd);
int _destroynet(NetworkCommand* command, int sockfd);
int _projectnet(NetworkCommand* command, int sockfd);
int _rollbacknet(NetworkCommand* command, int sockfd);
int _versionnet(NetworkCommand* command, int sockfd);
int _filenet(NetworkCommand* command, int sockfd);

int executecommand(NetworkCommand* command, int sockfd);

#endif