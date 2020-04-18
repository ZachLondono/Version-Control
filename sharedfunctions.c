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

FileContents* readfile(char* name) {
    int fd = open(name, O_RDWR);
    if (fd < 0) return NULL;

    struct stat filestat;
    stat(name, &filestat);
    size_t size = filestat.st_size;

    char* buffer = malloc(size);

    int status = 0;
    int bytesread = 0;
    while ((status = read(fd, buffer, size - bytesread)) != 0) bytesread += status;
    // close(fd);

    FileContents* file = malloc(sizeof(FileContents));
    file->content = buffer;
    file->size = size;
    file->fd = fd;
    return file;
}

void freefile(FileContents* file) {
    close(file->fd);
    free(file->content);
    free(file);
}

int isNum(char* value, int len) {
    int i = 0;
    for(i = 0; i < len; i++) if (!isdigit(value[i])) return 0;
    return 1;
}

int digitCount(int num) {
	int count = 0;
	do {
    	count++;
		num /= 10;
    } while(num != 0);
	return count;
}

int strshift(char* word, size_t buffsize, int offset) {
    if (offset > buffsize) return -1;
    if (!word) return -1;
    size_t usedbytes = strlen(word);
    if (usedbytes == 0) return -1;
    int i = 0;
    for(i = 0; i + offset < buffsize; i++) 
        word[i] = word[i + offset];
    memset(&word[usedbytes - offset], '\0', buffsize-i);
    return 0;
}

int incrimentManifest(char* project) {

    int path_size = strlen(project) + 13;
    char* path = malloc(path_size);
    memset(path, '\0', path_size);
    strcat(path,"./");
    strcat(path,project);
    strcat(path,"/.Manifest");

    FileContents* manifest = readfile(path);
    if (!manifest) return -1;
    int version = getManifestVersion(manifest);
    if (version < 0) {
        printf("Error: failed to open .Manifest");
        return version;
    }
    
    int digc = digitCount(version);
    char* version_s = malloc(digc + 1);
    memset(version_s, '\0', digc + 1);
    snprintf(version_s, digc+2, "%d", ++version);
    printf("New .Manifest version: %d=%s digits = %d\n", version, version_s, digitCount(version));

    int tmpfd = open("./.Manifest.tmp", O_RDWR | O_CREAT, S_IRWXU);

    write(tmpfd, version_s, digc);
    write(tmpfd, &manifest->content[digc], manifest->size-digc);

    free(version_s);
    freefile(manifest);

    remove(path);

    rename("./.Manifest.tmp", path);
    free(path);
    close(tmpfd);

    return 0;

}

int projectVersion(char* project) {

    int path_size = strlen(project) + 13;
    char* path = malloc(path_size);
    memset(path, '\0', path_size);
    strcat(path,"./");
    strcat(path,project);
    strcat(path,"/.Manifest");

    FileContents* manifest = readfile(path);
    free(path);
    return getManifestVersion(manifest);

}

int getManifestVersion(FileContents* manifest) {

    if (manifest == NULL) {
        printf("Error: project does not exist or does not contain .Manifest\n");
        return -1;
    }

    int size = manifest->size;

    char* version_s = NULL;
    int i = 0;
    for (i = 0; i < size; i++) {
        if (manifest->content[i] != '\n') continue;
        version_s = malloc(i + 1 + 1);      // +1 for NULL +1 incase incrimenting it leads to an additional digit
        memset(version_s, '\0', i + 1 + 1);
        memcpy(version_s, manifest->content, i);
        break;
    }

    if (version_s == NULL || !isNum(version_s, i)) {
        printf("Error: Malformed .Manifest file, unable to incriment version\n");
        return -1;
    }

    return atoi(version_s);

}