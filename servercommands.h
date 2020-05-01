#ifndef SERVERCMNDS
#define SERVERCMNDS
#include <pthread.h>
#include "networking.h"

Commit** activecommits;
int currentuid;
int maxusers;

int servercheckForLocalProj(char* projectname);
FileContents* serverreadfile(char* name);
ssize_t serverwrite(int fd, char* buff, size_t count);
ssize_t serverread(int fd, char* buff, size_t count);
int serveropen(const char* pathname, int flags);

int _responsenet(NetworkCommand* command, int sockfd);
int _createnet(NetworkCommand* command, int sockfd);
int _destroynet(NetworkCommand* command, int sockfd);
int _projectnet(NetworkCommand* command, int sockfd);
int _rollbacknet(NetworkCommand* command, int sockfd);
int _versionnet(NetworkCommand* command, int sockfd);
int _filenet(NetworkCommand* command, int sockfd);

int executecommand(NetworkCommand* command, int sockfd);

#endif
