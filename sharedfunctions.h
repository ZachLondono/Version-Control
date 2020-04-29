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
typedef struct FileContents{
    char* content;
    size_t size;
    int fd;
} FileContents;

typedef struct _Manifest {
    int version;
    char** entries;
    int entrycount;
} Manifest;

typedef struct _Commit {
    int filesize;
    int entries;
    char* filecontent;
    int uid;
} Commit;

int checkForLocalProj(char* projname);
char* bin2hex(const unsigned char *bin, size_t len);
int hexchr2bin(const char hex, char* out);
char* hashtohex(unsigned char* hash);
unsigned char* hashdata(unsigned char* data, size_t datalen);
FileContents* readfile(char* name);
void freefile(FileContents* file);
char* strrmove(char* str, const char* sub);
int isNum(char* value, int len);
int digitCount(int num);
int strshift(char* word, int populatedbytes, size_t buffsize, int offset);
int incrimentManifest(char* project, FileContents* (*readfile)(char*), ssize_t (*write)(int fd, char* buff, int count));
int projectVersion(char* project, FileContents* (*readfile)(char*));
int createcompressedarchive(char* filepath, int pathlen);
int getManifestVersion(FileContents* manifest);
char* getcompressedfile(char* filepath, int* deflated_size, int (*opencmnd)(const char*, int), ssize_t (*readcmnd)(int, char*, size_t));
int recreatefile(char* filepath, char* contents, int size);
int uncompressfile(char* compressedpath);
Manifest* parseManifest(FileContents* filecontent);
char** getManifestFiles(Manifest* manifest);
char** getManifestHashcodes(Manifest* manifest);
void freeManifest(Manifest* manifest);

#endif