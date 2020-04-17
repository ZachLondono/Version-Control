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
#include <ctype.h>

// Contains various utility functions that both client and server may want/need to use

int checkForLocalProj(char* projname);
void hashtohexprint(unsigned char* hash);
unsigned char* hashdata(unsigned char* data, size_t datalen);
char* readfile(char* name);
int isNum(char* value, int len);
int strshift(char* word, size_t buffsize, int offset);

#endif