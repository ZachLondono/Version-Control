#ifndef CLIENTCMNDS
#define CLIENTCMNDS

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include "sharedfunctions.h"
#include "networking.h"

typedef enum _commandtype {invalid=0, configure=1, checkout=2, update=3, upgrade=4, commit=5, push=6, create=7, destroy=8,
			add=9, remove_cmnd=10, currentversion=11, history=12, rollback=13} commandtype;

typedef struct ClientCommand {
    commandtype type;
    char** args;
} ClientCommand;

typedef struct Configuration {
    char* host;
    int port;
} Configuration;

Configuration* loadConfig();
void freeConfig(Configuration* config);
int connectwithconfig();
Commit* createcommit(char* remoteManifest, int remotelen, char* project, int projlen);
Update* createupdate(char* remoteManifest, int remotelen, char* project, int projlen);


int _invalidcommand(ClientCommand* command);
int _configure(ClientCommand* command);
int _checkout(ClientCommand* command);
int _update(ClientCommand* command);
int _upgrade(ClientCommand* command);
int _commit(ClientCommand* command);
int _push(ClientCommand* command);
int _create(ClientCommand* command);
int _destroy(ClientCommand* command);
int _add(ClientCommand* command);
int _remove(ClientCommand* command);
int _currentversion(ClientCommand* command);
int _history(ClientCommand* command);
int _rollback(ClientCommand* command);
#endif