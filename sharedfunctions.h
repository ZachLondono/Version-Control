#ifndef SHAREDFUNC
#define SHAREDFUNC

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <openssl/sha.h>
#include <string.h>

int checkForLocalProj(char* projname);
void hashtohexprint(char* hash);
char* hashdata(char* data);
char* readfile(char* name);

#endif