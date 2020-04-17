#include "sharedfunctions.h"

int checkForLocalProj(char* projname) {
	DIR *dir;
	// check if there is a directory with this name
	if (!(dir = opendir(projname))) return 0;
	closedir(dir);
	return 1;
}

void hashtohexprint(unsigned char* hash) {
	int i = 0;
	for (i = 0; i < SHA_DIGEST_LENGTH; i++) {
		printf(" %2x", (unsigned char) hash[i]);
	}
	printf("\n");
}

unsigned char* hashdata(unsigned char* data, size_t datalen) {
	unsigned char* hash = malloc(SHA_DIGEST_LENGTH);
	SHA1(data, datalen, hash);
	return hash;
}

char* readfile(char* name) {
    int fd = open(name, O_RDONLY);
    if (fd < 0) return NULL;

    struct stat filestat;
    stat(name, &filestat);
    size_t size = filestat.st_size;

    char* buffer = malloc(size);

    int status = 0;
    int bytesread = 0;
    while ((status = read(fd, buffer, size - bytesread)) != 0) bytesread += status;
    close(fd);

    return buffer;
}