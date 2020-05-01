#include <errno.h>
#include "clientcommands.h"

//invalid=0, configure=1, checkout=2, update=3, upgrade=4, commit=5, push=6, create=7, destroy=8, add=9, remove_cmnd=10, currentversion=11, history=12, rollback=13

// Will load connection details from a .Config file on local path and attempt to connect with them, returns file descriptor of connection or -1 if it failed
int connectwithconfig() {	
	Configuration* config = loadConfig();
	if (config == NULL) return -1;
	int sockfd = connecttohost(config->host, config->port);
	freeConfig(config);
	return sockfd;
}

int getUID() {

    FileContents* config = readfile(".config");
    if (!config) {
        printf("Error: run config before attempting to commit\n");
        return -2;
    }
    char* content = malloc(config->size + 5);
    memset(content, '\0', config->size + 5);
    memcpy(content, config->content, config->size);
    char* uidline = strstr(config->content, "uid");
    if (!uidline) {
        memset(content, '\0', config->size + 5);
        memcpy(content, "uid:",5);
        write(config->fd, content, 4);
        free(content);
        freefile(config);
        return -1;
    }
    free(content);

    strtok(config->content, "\n");
    strtok(NULL, "\n");
    strtok(NULL, ":");
    char* uid = strtok(NULL, "\n");
    if (isNum(uid, strlen(uid))) {
        int ret = atoi(uid);
        freefile(config);
        return ret;
    } else return -1;

}

// Initialize and allocate a new NetworkCommand from a ClientCommand to be sent to the server to request some data
NetworkCommand* newrequest(ClientCommand* command, int argc) {
    
    NetworkCommand* request = malloc(sizeof(NetworkCommand));
    request->argc = argc;
    request->arglengths = malloc(sizeof(int) * argc);
    request->argv = malloc(sizeof(char*) * argc);

    int i = 0;
    for (i = 0; i < argc; i++) {
        request->arglengths[i] = strlen(command->args[i]);
        request->argv[i] = malloc(request->arglengths[i] + 1);
        memset(request->argv[i], '\0', request->arglengths[i] + 1);
        memcpy(request->argv[i], command->args[i], request->arglengths[i]);
    }

    return request;

}

NetworkCommand* newfilerequest(char* project, char** filepaths, int filecount) {

    NetworkCommand* request = malloc(sizeof(NetworkCommand));
    request->type = filenet;
    request->argc = 2 + filecount;
    request->arglengths = malloc(sizeof(int) * request->argc);
    request->argv = malloc(sizeof(char*) * request->argc);

    request->arglengths[0] = strlen(project);
    request->argv[0] = malloc(request->arglengths[0] + 1);
    memset(request->argv[0], '\0', request->arglengths[0]);
    memcpy(request->argv[0], project, request->arglengths[0]);

    int arg1len = digitCount(filecount) + 1;
    request->argv[1] = malloc(arg1len);
    sprintf(request->argv[1], "%d", filecount);
    request->arglengths[1] = arg1len - 1;

    int i = 0;
    for (i = 0; i < filecount; i++) {
        request->arglengths[i + 2] = strlen(filepaths[i]);
        request->argv[i+2] = malloc(request->arglengths[i+2] + 1);
        memset(request->argv[i + 2], '\0', request->arglengths[i+2] + 1);
        memcpy(request->argv[i + 2], filepaths[i], request->arglengths[i+2]);
    }

    return request;

}

NetworkCommand* newDataTransferCmnd(char* project, char* datacontent, int length) {

    NetworkCommand* toSend = malloc(sizeof(NetworkCommand));
    toSend->type = data;
    toSend->argc = 2;
    toSend->arglengths = malloc(sizeof(int) * 2);
    toSend->argv = malloc(sizeof(char*) * 2);

    toSend->arglengths[0] = strlen(project);
    toSend->argv[0] = malloc(toSend->arglengths[0] + 1);
    memset(toSend->argv[0], '\0', toSend->arglengths[0]);
    memcpy(toSend->argv[0], project, toSend->arglengths[0]);

    toSend->arglengths[1] = length;
    toSend->argv[1] = malloc(length + 1);
    memset(toSend->argv[1], '\0', toSend->arglengths[1]);
    memcpy(toSend->argv[1], datacontent, toSend->arglengths[1]);

    return toSend;

}

int checkresponse(char* command, NetworkCommand* response) {
    
    if (response->argc != 3 || strcmp(response->argv[0], command) != 0) { 	// valid responses to the create request will contain 3 arguments
        printf("Error: Malformed response from server\n");
        freeCMND(response);
        return -1;
    }

    if (strcmp(response->argv[1], "failure") == 0) {				// if the request failed, tell the user
        printf("Error: server failed to execute command: %s\n", response->argv[2]);	// argument 3 will contain the reason for the request failing
        freeCMND(response);
        return -1;
    }

    return 0;

}

// Print out reason that command was invalid
int _invalidcommand(ClientCommand* command) {
    printf("Fatal Error: %s\n", command->args[0]);
    return 0;
}

int _configure(ClientCommand* command) {

    // Takes in 2 argument <ip/host> and <port>
    // ip/host must be at most HOST_NAME_MAX bytes long
    // port must be a positive integer no longer than 16 bits, 65535
    // a .Config file will be created in the cwd and have both of these values written to it

    if (strlen(command->args[0]) > HOST_NAME_MAX + 1){
        printf("Error: Invalid host name length, max hostname on this system is %d\n", HOST_NAME_MAX);
        return -1;
    }

    if (strlen(command->args[1]) > 5){
        printf("Error: Invalid port length, max port number is 16 bit\n");
        return -1;
    }

    if (!isNum(command->args[1], strlen(command->args[1])))  {
        printf("Error: Invalid port, value must be a max 16 bit integer\n");
        return -1;    
    }

    remove("./.config");
    int fd_config = open("./.config", O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
    if (fd_config == -1) {
        printf("Error: couldn't create .config file\n");
        return -1;
    }

    int size = strlen(command->args[0]) + strlen(command->args[1]) + 13;
    char* filecontents = malloc(sizeof(char) * (size));
    memset(filecontents, '\0', size);
    snprintf(filecontents, size, "host:%s\nport:%s\n", command->args[0], command->args[1]);

    if (write(fd_config, filecontents, size - 1 ) == -1) {
        printf("Error: couldn't write to .config file\n");
        return -1;
    }

    free(filecontents);
    close(fd_config);

    printf("Successfully saved configuration:\n%s\n%s\n", command->args[0], command->args[1]);

    return 0;

}

int _checkout(ClientCommand* command) {

    char** files = malloc(sizeof(char*));
    files[0] = ".";
    NetworkCommand* request = newfilerequest(command->args[0], files, 1);

    int sockfd = connectwithconfig();
    if (sockfd < 0) {
        freeCMND(request);
        return sockfd;
    }

    sendNetworkCommand(request, sockfd);
    freeCMND(request);

    NetworkCommand* response = readMessage(sockfd);
    close(sockfd);

    if (checkresponse("file", response) < 0) return -1;     // check the response if valid

    recreatefile("archive.tar.gz", response->argv[2], response->arglengths[2]);
    uncompressfile("archive.tar.gz");    

    printf("%d compressed bytes recieved\nProject '%s' has been checked out\n", response->arglengths[2], command->args[0]);
    freeCMND(response);
    return -1;
}

int _update(ClientCommand* command) {
    printf("update not implimented\n");
    return -1;
}

int _upgrade(ClientCommand* command) {
    printf("upgrade not implimented\n");
    return -1;
}

int _commit(ClientCommand* command) {

    if (!checkForLocalProj(command->args[0])) {
        printf("Error: Local project does not exist. Checkout project from server or create a new one.\n");
        return -1;
    }

    int projlen = strlen(command->args[0]);

    char* conflictpath = malloc(projlen + 11);
    memset(conflictpath, '\0', projlen + 11);
    sprintf(conflictpath, "%s/.Conflict", command->args[0]);
    int fd = open(conflictpath, O_RDONLY);
    if (fd > 0) {
        printf("Error: Conflicts in project '%s' exist. Resolve conflict before attempting to commit.\n", command->args[0]);
        free(conflictpath);
        close(fd);
        return -1;
    }
    close(fd);
    free(conflictpath);

    char* updatepath = malloc(projlen + 9);
    memset(updatepath, '\0', projlen + 9);
    sprintf(updatepath, "%s/.Update", command->args[0]);
    fd = open(updatepath, O_RDONLY);
    if (fd > 0) {
        int size = lseek(fd, 0, SEEK_END);
        close(fd);
        if (size != 0) {
            printf("Error: Project '%s' has pending updates. Upgrade before attempting to commit.\n", command->args[0]);
            free(updatepath);
            return -1;
        } else remove(updatepath);
    }
    close(fd);
    free(updatepath);

    char** requestArgs = malloc(sizeof(char*) * 2);
    requestArgs[0] = malloc(10);
    memcpy(requestArgs[0], ".Manifest", 10);

    int uid = getUID();
    if (uid == -2) {
        free(requestArgs);
        return -1;
    }
    requestArgs[1] = uid == -1 ? malloc(3) : malloc(digitCount(uid) + 1);
    sprintf(requestArgs[1], "%d", uid);

    NetworkCommand* request = newfilerequest(command->args[0], requestArgs, 2);
    request->type = commitnet;                                                              // modifying a file request into a commit request
    memset(request->argv[1], '\0', request->arglengths[1]);
    memcpy(request->argv[1], "1", 2);

    free(requestArgs[0]);
    free(requestArgs[1]);
    free(requestArgs);

    int sockfd = connectwithconfig();
    if (sockfd < 0) return sockfd;

    sendNetworkCommand(request, sockfd);                                                    // TODO change for commit
    NetworkCommand* response = readMessage(sockfd);
    freeCMND(request);

    if (checkresponse("file", response) < 0) return -1;
    
    Commit* commit = createcommit(response->argv[2], response->arglengths[2], command->args[0], projlen);
    freeCMND(response);
    if(commit == NULL) {
        printf("Error: couldn't create commit\n");
        char* name = malloc(7);
        memset(name, '\0', 7);
        memcpy(name, "commit", 7);
        char* reason = malloc(23);
        memset(reason, '\0', 23);
        memcpy(reason, "couldn't create commit", 23);
        NetworkCommand* cancel = newFailureCMND(name, reason);
        sendNetworkCommand(cancel, sockfd);
        freeCMND(cancel);
        return -1;        
    }

    if (!commit) {
        char* name = malloc(7);
        memset(name, '\0', 7);
        memcpy(name, "commit", 7);
        char* reason = malloc(21);
        memset(reason, '\0', 21);
        memcpy(reason, "no changes to commit", 21);
        NetworkCommand* cancel = newFailureCMND(name, reason);
        sendNetworkCommand(cancel, sockfd);
        freeCMND(cancel);
        return -1;
    }

    NetworkCommand* toSend = newDataTransferCmnd(command->args[0], commit->filecontent, commit->filesize - 1);      // Send commit file to server
    sendNetworkCommand(toSend, sockfd);
    freeCMND(toSend);
    

    NetworkCommand* finalresponse = readMessage(sockfd);                         // check that server recieved commit successfully
    if (checkresponse("data", finalresponse) != 0)  {
        printf("Error: server couldn't read commit file\n");
        freeCommit(commit);
        return -1;
    }

    if (uid != atoi(finalresponse->argv[2])) {                                   // if the server sent a different uid than the one save, replace local
        FileContents* config = readfile(".config");
        write(config->fd, finalresponse->argv[2], strlen(finalresponse->argv[2]));
        freefile(config);
    }

    printf("Successfully commit %d change(s) -> uid: %s\n", commit->entries, finalresponse->argv[2]);
    freeCommit(commit);
    freeCMND(finalresponse);

    return 0;

}

Commit* createcommit(char* remoteManifest, int remotelen, char* project, int projlen) {

    int checkfolder = 0;
    DIR* dir = NULL;
    if (!(dir = opendir(".tempfiles"))) checkfolder = mkdir(".tempfiles", 0700);
    else closedir(dir);

    if (checkfolder != 0) return NULL;

    chdir(".tempfiles");
    recreatefile("archive.tar.gz", remoteManifest, remotelen);
    uncompressfile("archive.tar.gz");
    chdir("..");

    char* servermanifestpath = malloc(23 + projlen);
    memset(servermanifestpath, '\0', 23 + projlen);
    sprintf(servermanifestpath, ".tempfiles/%s/.Manifest", project);
    FileContents* servermanifest = readfile(servermanifestpath);
    free(servermanifestpath);

    close(servermanifest->fd);
    system("rm -rf .tempfiles");

    char* localmanifestpath = malloc(11 + projlen);
    memset(localmanifestpath, '\0', 11 + projlen);
    sprintf(localmanifestpath, "%s/.Manifest", project);
    FileContents* localmanifestfile = readfile(localmanifestpath);
    free(localmanifestpath);

    if (getManifestVersion(servermanifest) != getManifestVersion(localmanifestfile)) {
        printf("Error: Remote project is ahead of local project. Update before attempting to commit.");
        freefile(localmanifestfile);
        freefile(servermanifest);
        return NULL;
    }

    Manifest* localmanifest = parseManifest(localmanifestfile);
    char** files = getManifestFiles(localmanifest);
    char** hashcodes = getManifestHashcodes(localmanifest);
    int* fileversions = getManifestFileVersion(localmanifest);

    Manifest* remotemanifest = parseManifest(servermanifest);
    char** remotefiles = getManifestFiles(remotemanifest);
    char** remotehashcodes = getManifestHashcodes(remotemanifest);

    if (localmanifest->entrycount == 0 && remotemanifest->entrycount == 0) {
        printf("There are no changes to commit\n");
        freeManifest(localmanifest);
        freeManifest(remotemanifest);
        freefile(localmanifestfile);
        freefile(servermanifest);
        return NULL;
    }

    Commit* commit = malloc(sizeof(Commit));
    commit->entries = 0;
    commit->uid = -1;

    commit->filecontent = malloc(localmanifestfile->size);
    memset(commit->filecontent, '\0', localmanifestfile->size);
    commit->filesize = 0;

    freefile(localmanifestfile);
    freefile(servermanifest);

    /* 
        Modify
        1) livehash != localhash
        2) localhash == serverhash

        Add
        1) file is in localmanifest 
        2) file is not in remotemanifest

        Remove
        1) file is in remotemanifest
        2) file is not in localmanifest
    */

    int i = 0;
    for(i = 0; i < localmanifest->entrycount; i++) {                    // Check if client added/modified any files

        int j = 0;
        int new = 1;
        for (j = 0; j < remotemanifest->entrycount; j++) {
            if (strcmp(files[i], remotefiles[j]) != 0) continue;        // file is included in remote manifest
            new = 0;
            break;
        }

        if (new) {
            // Add condition
            char* commitentry = malloc(strlen(files[i]) + digitCount(fileversions[i]) + 47);
            memset(commitentry, '\0', strlen(files[i]) + digitCount(fileversions[i]) + 47);
            sprintf(commitentry, "A %s", files[i]); 
            printf("%s\n", commitentry);
            sprintf(commitentry, "A %d %s %s\n",fileversions[i] + 1, files[i], hashcodes[i]);
            strcat(commit->filecontent, commitentry);
            free(commitentry);
            commit->filesize += strlen(files[i]) + digitCount(fileversions[i]) + 47;
            commit->entries += 1;
        } else {
            // file is already in remote, check that is up to date with live file
            FileContents* livecontent = readfile(files[i]);
            unsigned char* livehashcode = hashdata((unsigned char*) livecontent->content, livecontent->size);

            if (strcmp((const char*)livehashcode, (const char*) hashcodes[i]) != 0) {
                if (strcmp((const char*)hashcodes[i],(const char*) remotehashcodes[j]) != 0) {    // check that local manifest hash matches the server manifest hash
                    printf("Error: There is an inconsistency between the remote project and the local project, skiping file '%s'\n", files[i]);
                    freefile(livecontent);
                    free(livehashcode);
                    continue;
                }

                // Modify condition
                char* commitentry = malloc(strlen(files[i]) + digitCount(fileversions[i]) + 47);
                memset(commitentry, '\0', strlen(files[i]) + digitCount(fileversions[i]) + 47);
                sprintf(commitentry, "M %s", files[i]); 
                printf("%s\n", commitentry);
                sprintf(commitentry, "M %d %s %s\n", fileversions[i], files[i], hashcodes[i]); 
                strcat(commit->filecontent, commitentry);
                free(commitentry);
                commit->filesize  += strlen(files[i]) + digitCount(fileversions[i]) + 47;
                commit->entries += 1;
            }
            freefile(livecontent);
        }

    }

    for(i = 0; i < remotemanifest->entrycount; i++) {               // check if client deleted any files
        
        int j = 0;
        int removed = 1;
        for (j = 0; j < localmanifest->entrycount; j++) {
            if (strcmp(files[j], remotefiles[i]) != 0) continue;
            removed = 0;
            break;
        }
        
        if (removed) {
            // Delete condition
            char* commitentry = malloc(strlen(remotefiles[i]) + 48);
            memset(commitentry, '\0', strlen(remotefiles[i]) + 48);
            sprintf(commitentry, "D %s", remotefiles[i]); 
            printf("%s\n", commitentry);
            sprintf(commitentry, "D 0 %s %s\n", remotefiles[i], remotehashcodes[i]); 
            strcat(commit->filecontent, commitentry);
            free(commitentry);
            commit->filesize += strlen(remotefiles[i]) + 45;
            commit->entries += 1;
        }

    }            

    for (i = 0; i < localmanifest->entrycount; i++) {
        free(files[i]);
        free(hashcodes[i]);
    }
    for (i = 0; i < remotemanifest->entrycount; i++) {
        free(remotefiles[i]);
        free(remotehashcodes[i]);
    }
    free(files);
    free(hashcodes);
    free(remotefiles);
    free(remotehashcodes);

    freeManifest(remotemanifest);
    freeManifest(localmanifest);

    char* commitpath = malloc(9 + projlen);
    memset(commitpath, '\0', 9 + projlen);
    sprintf(commitpath, "%s/.Commit", project);
    int fd = open(commitpath, O_RDWR | O_CREAT, S_IRWXU);
    if (fd < 0) {
        printf("Error: Could not open or create commit file. Make sure file permissions have not been changed\n");
        close(fd);
        free(commitpath);
        return NULL; 
    }

    free(commitpath);

    if (commit->filesize == '\0') {
        printf("There are no changes to commit. The local files are all up to date.\n");
        close(fd);
        free(commit);
        return NULL;
    }

    write(fd, commit->filecontent, commit->filesize - 2);      // TODO MAKE SURE ALL BYTES ARE WRITTEN TO FILE
    close(fd);

    return commit;

}

int _push(ClientCommand* command) {
    printf("push not implimented\n");
    return -1;
}

int _create(ClientCommand* command) {
    
    // Will send a request to the server to create a new project within the repository
    // then wait for a response to see if it was done successfully
    // command takes one argument <project name>

    NetworkCommand* request = newrequest(command, 1);
    request->type = createnet;

    int sockfd = connectwithconfig();
    if (sockfd < 0) return sockfd;

    sendNetworkCommand(request, sockfd);
    freeCMND(request);

    NetworkCommand* response = readMessage(sockfd);
    close(sockfd);
    
    if (checkresponse("create", response) < 0) return -1;

    printf("Project '%s' has been created in the repository\n", command->args[0]);

    freeCMND(response);

    if (mkdir(command->args[0], 0700)) {
		printf("Error: failed to create new project directory: %s\n", strerror(errno));
	}
    char* newmanifest = malloc( + 11);
	memset(newmanifest, '\0', strlen(command->args[0]) + 11);
	strcat(newmanifest, command->args[0]);
	strcat(newmanifest, "/.Manifest");
	int fd = open(newmanifest, O_WRONLY | O_CREAT, S_IRWXU);
	if (fd < 0) {
		printf("Error: failed to create new manifest is project: %s\n", strerror(errno));
	} else { 
		write(fd, "1\n", 2);
		close(fd);
		free(newmanifest);
	}

    printf("Project '%s' has been created locally\n", command->args[0]);
    
    return 0;

}

int _destroy(ClientCommand* command) {
    NetworkCommand* request = newrequest(command, 1);
    request->type = destroynet;

    int sockfd = connectwithconfig();
    if (sockfd < 0) {
        freeCMND(request);
        return sockfd;
    }

    sendNetworkCommand(request, sockfd);
    freeCMND(request);

    NetworkCommand* response = readMessage(sockfd);
    close(sockfd);
    
    if (checkresponse("destroy", response) < 0) return -1;     // check the response if valid

    freeCMND(response);

    printf("Project '%s' has been removed from the server\n", command->args[0]);

    return 0;

}

int _add(ClientCommand* command) {

    char* proj = command->args[0];
    char* file = command->args[1];

    if(!checkForLocalProj(command->args[0])) {      // Check project exists
        printf("Error: project does not exist locally, checkout the project or create it if it does not exist\n");
        return -1;
    }

    int projlen = strlen(command->args[0]);
    int filelen = strlen(command->args[1]);
    char* fullpath = malloc(projlen + 2 + filelen);
    memset(fullpath, '\0', projlen + filelen + 2);
    sprintf(fullpath, "%s/%s", proj, file);

    FileContents* filecontents = readfile(fullpath);       // Check file exists & load it
    if (filecontents == NULL) {
        printf("Error: file does not exist or cannot be accessed\n");
        free(fullpath);
        return -1;
    }

    char* manifestpath = malloc(projlen + 11);
    memset(manifestpath, '\0', projlen + 11);              // Check for & load project local .Manifest
    snprintf(manifestpath, projlen + 11, "%s/.Manifest", proj);
    FileContents* manifest = readfile(manifestpath);
    if (manifest == NULL) {
        printf("Error: project has not been initalized or cannot be accessed\n");
        free(fullpath);
        free(manifestpath);
        freefile(manifest);
        freefile(filecontents);
        return -1;
    }

    if(strstr(manifest->content, fullpath) != NULL) {      // Check if file already has an entry in the .Manifest
        printf("Error: file has already been added\n");
        free(fullpath);
        free(manifestpath);
        freefile(manifest);
        freefile(filecontents);
        return -1;    
    }

    int len =  manifest->size + filelen + projlen + SHA_DIGEST_LENGTH*2 + 6;
    char* newmanifest = malloc(len);
    memset(newmanifest, '\0', len);

    unsigned char* filehash = hashdata((unsigned char*) filecontents->content, filecontents->size);
    char* encodedhash = hashtohex(filehash);                // Has the file content and represent the hash with hex
    free(filehash);

    snprintf(newmanifest, len, "%s1 %s %s\n", manifest->content, fullpath, encodedhash);
    free(fullpath);
    free(encodedhash);
    freefile(filecontents);
    freefile(manifest);

    char* tempmanifest = malloc(projlen + 16);              // Create a new .Manifest with new enty
    memset(tempmanifest, '\0', projlen + 16);
    snprintf(tempmanifest, projlen + 16, "%s.temp", manifestpath);
    int fd = open(tempmanifest, O_RDWR | O_CREAT, S_IRWXU);
    if (fd < 0) {
        printf("Error: failed to add file\n");
        free(newmanifest);
        close(fd);
        return -1;
    }

    int status = 0;         
    int written = 0;
    while((status = write(fd, &newmanifest[written], len - 1 - written)) > 0) written += status;
    
    free(newmanifest);
    close(fd);
    
    if (status == -1) {
        printf("Error: failed to add file\n");
        return -1;
    } 

    remove(manifestpath);                                   // relace loacl .Manifest with new one
    rename(tempmanifest, manifestpath);
    free(tempmanifest);
    free(manifestpath);

    printf("File '%s' has been added\n", file);

    return 0;
    
}

int _remove(ClientCommand* command) {

    char* proj = command->args[0];
    char* file = command->args[1];

    if(!checkForLocalProj(command->args[0])) {      // Check project exists
        printf("Error: project does not exist locally, checkout the project or create it if it does not exist\n");
        return -1;
    }

    int projlen = strlen(command->args[0]);
    int filelen = strlen(command->args[1]);
    char* fullpath = malloc(projlen + 2 + filelen);
    memset(fullpath, '\0', projlen + filelen + 2);
    sprintf(fullpath, "%s/%s", proj, file);

    char* manifestpath = malloc(projlen + 11);
    memset(manifestpath, '\0', projlen + 11);              // Check for & load project local .Manifest
    snprintf(manifestpath, projlen + 11, "%s/.Manifest", proj);
    FileContents* manifest = readfile(manifestpath);
    if (manifest == NULL) {
        printf("Error: project has not been initalized or cannot be accessed\n");
        free(fullpath);
        free(manifestpath);
        freefile(manifest);
        return -1;
    }

    if(strstr(manifest->content, fullpath) == NULL) {      // Check if file doesn't have an entry in the .Manifest
        printf("Error: file has not yet been added\n");
        free(fullpath);
        free(manifestpath);
        freefile(manifest);
        return -1;    
    }

    char* manifestcontent = malloc(manifest->size + 1);
    memset(manifestcontent, '\0', manifest->size + 1);
    memcpy(manifestcontent, manifest->content, manifest->size);

    strtok(manifestcontent, "\n");                        // skip first line (manifest ver)

    char* token;        // find the line in the manifest which contains this file
    while ((token = strtok(NULL, "\n"), fullpath) != NULL) {
        if (strstr(token, fullpath) == NULL) continue;
        break;
    }

    int tokenlen = strlen(token);
    char* fulltoken = malloc(tokenlen + 2);
    memset(fulltoken, '\0', tokenlen + 2);
    snprintf(fulltoken,tokenlen+2, "%s\n", token);
    char* newcontent = strrmove(manifest->content, fulltoken);


    char* tempmanifest = malloc(projlen + 16);              // Create a new .Manifest with new enty
    memset(tempmanifest, '\0', projlen + 16);
    snprintf(tempmanifest, projlen + 16, "%s.temp", manifestpath);
    int fd = open(tempmanifest, O_RDWR | O_CREAT, S_IRWXU);
    if (fd < 0) {
        printf("Error: failed to add file\n");
        close(fd);
        return -1;
    }

    int status = 0;         
    int written = 0;
    while((status = write(fd, &newcontent[written], manifest->size - tokenlen - 1 - written)) > 0) written += status;
    freefile(manifest);
    
    
    if (status == -1) {
        printf("Error: failed to remove file\n");
        return -1;
    } 

    remove(manifestpath);                                   // relace loacl .Manifest with new one
    rename(tempmanifest, manifestpath);
    
    close(fd);
    free(tempmanifest);
    free(fullpath);
    free(manifestpath);
    free(manifestcontent);
    free(fulltoken);

    printf("File '%s' has been removed\n", file);

    return -1;
}

int _currentversion(ClientCommand* command) {

    NetworkCommand* request = newrequest(command, 1);
    request->type = versionnet;

    int sockfd = connectwithconfig();
    if (sockfd < 0) {
        freeCMND(request);
        return sockfd;
    }

    sendNetworkCommand(request, sockfd);
    freeCMND(request);

    NetworkCommand* response = readMessage(sockfd);
    close(sockfd);
    
    if (checkresponse("version", response) < 0) return -1;     // check the response if valid

    printf("Project version: %s\n", response->argv[2]); 
    
    freeCMND(response);

    return 0;
}

int _history(ClientCommand* command) {
    printf("history not implimented\n");
    return -1;
}

int _rollback(ClientCommand* command) {
    printf("rollback not implimented\n");
    return -1;
}

Configuration* loadConfig() {

    // Will check that the configuration file (.config) exists and is formatted correctly and contains a valid configuration
    // if it is not/does not it will warn the user of the error and requuire that the user run the configuration command again.

    FileContents* file = readfile("./.config");
    if (file == NULL) {
        printf("Error: .config file does not exist or has incorrect permissions\n");
        return NULL;
    }
    char* buffer = file->content;
    size_t size = strlen(buffer);

    // file is blank or jus has empty lines host:\nport:\n
    if (size <= 12) {
        printf("Error: .config file was found, but is blank, run config command\n");
        return NULL;
    }

    char* host = malloc(HOST_NAME_MAX + 1);
    memset(host, '\0', HOST_NAME_MAX + 1);
    char* port = malloc(5 + 1); // ports are a 16 bit number, so 5 chars + 1 null
    memset(port, '\0', 6);

    int host_len = 0;
    while (host_len < (size - 10)) {
        if ((&buffer[5])[host_len] == '\n') {
            memcpy(host, &buffer[5], host_len);   // copy hostname into heap
            break;
        }
        if (host_len == HOST_NAME_MAX) {
            printf("Error: Invalid host name/ip, max hostname length on this system is %d\n", HOST_NAME_MAX);
            free(host);
            free(port);
            free(buffer);
            return NULL;
        }
        host_len++;
    }

    int port_len = 0;
    int max_port_len = strlen(&buffer[11 + host_len]);
    while (port_len <= (size - 10 - host_len - 1)) {
        if ((&buffer[11 + host_len])[port_len] == '\n' || port_len == max_port_len) {
            memcpy(port, &buffer[11 + host_len], port_len);
            if (!isNum(port, port_len)) {
                printf("Error: Invalid port number, value must be a number\n");
                free(host);
                free(port);
                free(buffer);
                return NULL;
            }
            break;
        }
        if (port_len == 5) {
            printf("Error: Invalid port number, ports can only at most a 16 bit integer\n");
            free(host);
            free(port);
            free(buffer);
            return NULL;
        }
        port_len++;
    }


    if (host_len == 0 || port_len == 0) {
        printf("Error: .config file was found, but is blank, run config command\n");
        free(host);
        free(port);
        free(buffer);
        return NULL;
    }

    freefile(file);
    Configuration* config = malloc(sizeof(Configuration));
    config->host = host;
    config->port = atoi(port);
    free(port);

    return config;

}

void freeConfig(Configuration* config) {
    if(config == NULL) return;
    if (config->host != NULL) free(config->host);
    free(config);
}