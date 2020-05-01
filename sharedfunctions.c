#include "sharedfunctions.h"

int checkForLocalProj(char* projname) {
	DIR *dir;
	// check if there is a directory with this name
	if (!(dir = opendir(projname))) return 0;
	closedir(dir);
	return 1;
}

char* hashtohex(unsigned char* hash) {
    char* hex = malloc(SHA_DIGEST_LENGTH*2 + 1);
    memset(hex, '\0', SHA_DIGEST_LENGTH*2 + 1);
    char* sub = malloc(4);
    memset(sub, '\0', 4);
    int i = 0;
    for (i = 0; i < SHA_DIGEST_LENGTH; i++) {
        memset(sub, '\0', 4);
        sprintf(sub, "%02x", (unsigned char) hash[i]);
        strcat(hex, sub);
    }
    free(sub);
    return hex;
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

    char* buffer = malloc(size + 1);
    memset(buffer, '\0', size + 1);

    int status = 0;
    int bytesread = 0;
    while ((status = read(fd, &buffer[bytesread], size - bytesread)) != 0) bytesread += status;
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

int strshift(char* word, int populated, size_t buffsize, int offset) {
    if (offset > buffsize) return -1;
    if (!word) return -1;
    size_t usedbytes = populated;
    if (usedbytes == 0) return -1;
    int i = 0;
    for(i = 0; i + offset < buffsize; i++) 
        word[i] = word[i + offset];
    int index = usedbytes - offset;
    if (index < 0) index = 0;
    if (index >= buffsize) index = buffsize - 1; 
    memset(&word[index], '\0', buffsize-i);
    return 0;
}

int incrimentManifest(char* project, FileContents* (*readfilepntr)(char*), ssize_t (*writepntr)(int fd, char* buff, int count)) {

    int path_size = strlen(project) + 13;
    char* path = malloc(path_size);
    memset(path, '\0', path_size);
    strcat(path,"./");
    strcat(path,project);
    strcat(path,"/.Manifest");

    FileContents* manifest = (*readfilepntr)(path);
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

    (*writepntr)(tmpfd, version_s, digc);
    (*writepntr)(tmpfd, &manifest->content[digc], manifest->size-digc);

    free(version_s);
    freefile(manifest);

    remove(path);

    rename("./.Manifest.tmp", path);
    free(path);
    close(tmpfd);

    return 0;

}

int projectVersion(char* project, FileContents* (*readfilepntr)(char*)) {

    int path_size = strlen(project) + 13;
    char* path = malloc(path_size);
    memset(path, '\0', path_size);
    strcat(path,"./");
    strcat(path,project);
    strcat(path,"/.Manifest");

    FileContents* manifest = readfilepntr(path);
    free(path);
    int ret = getManifestVersion(manifest);
    freefile(manifest);
    return ret;

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

    int ret = atoi(version_s);
    free(version_s);
    return ret;

}

int createcompressedarchive(char* filepath, int pathlen) {
	int cmndlen = 26 + pathlen;
    char* tarcmnd = malloc(cmndlen);
	memset(tarcmnd,'\0', cmndlen);
	snprintf(tarcmnd, cmndlen, "tar -czf archive.tar.gz %s", filepath);
	if (system(tarcmnd) != 0) {
        printf("tar command failed\n");
        return -1;
    }
	free(tarcmnd);
    return 0;
}

char* getcompressedfile(char* filepath, int* deflated_size, int (*opencmnd)(const char*, int), ssize_t (*readcmnd)(int, char*, size_t)) {

    int pathlen = strlen(filepath);
    if (createcompressedarchive(filepath, pathlen) < 0) return NULL;

	int fd = opencmnd("archive.tar.gz", O_RDWR);
	if (fd < 0) {
		printf("Error: couldn't read compressed data\n");
		return NULL;
	}

    struct stat filestat;
    stat("archive.tar.gz", &filestat);
    size_t size = filestat.st_size;
	*deflated_size = size;

	char* deflated_content = malloc(size);
	int readin = 0, status = 0;
	while ((status = readcmnd(fd, &deflated_content[readin], size - readin)) > 0) readin += status;

	close(fd);

	if (remove("archive.tar.gz") != 0) printf("Error: couldn't remove temporary files\n");

	return deflated_content;
	
}

int recreatefile(char* filepath, char* contents, int size) {
	int fd = open(filepath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd < 0) printf("Error: failed to recreate file\n");
	else write(fd, contents, size);                             //TODO MAKE SURE ALL DATA WAS WRITTEN
	close(fd);
    return fd >= 0 ? 0 : -1;
}

int uncompressfile(char* compressedpath) {

	int pathlen = strlen(compressedpath);
	int cmndlen = 10 + pathlen;

	char* tarcmnd = malloc(cmndlen);
	memset(tarcmnd,'\0', cmndlen);
	snprintf(tarcmnd, cmndlen, "tar -xf %s", compressedpath);
	int ret = system(tarcmnd);
	free(tarcmnd);

	if(remove(compressedpath) != 0) {
        printf("Error: couldn't remove temporary files\n");
    }
    return ret;

}

char* strrmove(char* str, const char* sub) {
    char *p, *q, *r;
    if ((q = r = strstr(str, sub)) != NULL) {
        size_t len = strlen(sub);
        while ((r = strstr(p = r + len, sub)) != NULL) {
            while (p < r) *q++ = *p++;
        }
        while((*q++ = *p++) != '\0') continue;
    }
    return str;
}

Manifest* parseManifest(FileContents* filecontent) {

    int entrycount = -1;
    int i = 0;
    for(i = 0; i < filecontent->size; i++) if (filecontent->content[i] == '\n') entrycount++;
    if (entrycount < 0) return NULL;

    Manifest* manifest = malloc(sizeof(Manifest));

    manifest->entrycount = entrycount;
    
    manifest->entries = malloc(entrycount * sizeof(char*)); 
    char* version = strtok(filecontent->content, "\n");

    if (!isNum(version, strlen(version))) {
        printf("Error: malformed Manifest file\n");
        free(manifest);
        return NULL;
    }

    manifest->version = atoi(version);

    for(i = 0; i < entrycount; i++) {
        char* entry = strtok(NULL, "\n");
        manifest->entries[i] = malloc(strlen(entry) + 1);
        memset(manifest->entries[i], '\0', strlen(entry) + 1);
        memcpy(manifest->entries[i], entry, strlen(entry) + 1);
    }

    return manifest;

}

char** getManifestFiles(Manifest* manifest) {

    if (manifest->entrycount == 0) return NULL;

    char** filepaths = malloc(sizeof(char*) * manifest->entrycount);

    int i = 0;
    for (i = 0; i < manifest->entrycount; i++) {

        char* entry = malloc(strlen(manifest->entries[i]) + 1);
        memset(entry, '\0', strlen(manifest->entries[i]) + 1);
        memcpy(entry, manifest->entries[i], strlen(manifest->entries[i]) + 1);

        strtok(entry, " ");
        char* filepath = strtok(NULL, " ");
        int len = strlen(filepath);
        filepaths[i] = malloc(len + 1);
        memset(filepaths[i], '\0', len + 1);
        memcpy(filepaths[i], filepath, len);

        free(entry);

    }

    return filepaths;

}

char** getManifestHashcodes(Manifest* manifest) {

    if (manifest->entrycount == 0) return NULL;

    char** hashcodes = malloc(sizeof(char*) * manifest->entrycount);

    int i = 0;
    for (i = 0; i < manifest->entrycount; i++) {
        
        char* entry = malloc(strlen(manifest->entries[i]) + 1);
        memset(entry, '\0', strlen(manifest->entries[i]) + 1);
        memcpy(entry, manifest->entries[i], strlen(manifest->entries[i]) + 1);


        strtok(entry, " ");
        strtok(NULL, " ");
        char* hashcode = strtok(NULL, "\n");
        int len = strlen(hashcode);
        hashcodes[i] = malloc(len + 1);
        memset(hashcodes[i], '\0', len + 1);
        memcpy(hashcodes[i], hashcode, len);

        free(entry);

    }

    return hashcodes;

}

int* getManifestFileVersion(Manifest* manifest) {

    if (manifest->entrycount == 0) return NULL;

    int* fileversions = malloc(sizeof(int) * manifest->entrycount);

    int i = 0;
    for (i = 0; i < manifest->entrycount; i++) {

        char* entry = malloc(strlen(manifest->entries[i]) + 1);
        memset(entry, '\0', strlen(manifest->entries[i] + 1));
        memcpy(entry, manifest->entries[i], strlen(manifest->entries[i]) + 1);

        char* version = strtok(entry, " ");
        fileversions[i] = atoi(version);

        free(entry);

    }

    return fileversions;

}

void freeManifest(Manifest* manifest) {

    int i = 0;
    for(i = 0; i < manifest->entrycount; i++) {
        free(manifest->entries[i]);
    }
    free(manifest->entries);
    free(manifest);

}

void freeCommit(Commit* commit) {
    free(commit->filecontent);
    free(commit);
}