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
#include <unistd.h>

int checkForLocalProj(char* projname);
void hashtohexprint(unsigned char* hash);
unsigned char* hashdata(unsigned char* data, size_t datalen);
char* readfile(char* name);

#endif