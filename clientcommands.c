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
    char* content = malloc(config->size + 8);
    memset(content, '\0', config->size + 8);
    memcpy(content, config->content, config->size);
    char* uidline = strstr(config->content, "uid");
    if (!uidline) {
        memset(content, '\0', config->size + 8);
        memcpy(content, "uid:-1\n",8);
        write(config->fd, content, 6);
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

    if (!checkForLocalProj(command->args[0])) {
        printf("Error: Local project does not exist. Checkout project from server or create a new one.\n");
        return -1;
    }

    int projlen = strlen(command->args[0]);

    char* localmanifestpath = malloc(11 + projlen);
    memset(localmanifestpath, '\0', 11 + projlen);
    sprintf(localmanifestpath, "%s/.Manifest", command->args[0]);
    int fd = open(localmanifestpath, O_RDONLY);
    if (fd < 0) {
        printf("Error: Project does not contain a Manifest\n");
        free(localmanifestpath);
        return -1;
    }
    free(localmanifestpath);
    close(fd);

    // char** requestArgs = malloc(sizeof(char*) * 2);
    // requestArgs[0] = malloc(10);
    // memcpy(requestArgs[0], ".Manifest", 10);

    char* file = ".Manifest";
    NetworkCommand* request = newfilerequest(command->args[0], &file, 1);
    request->type = updatenet;                                                             

    int sockfd = connectwithconfig();
    if (sockfd < 0) return sockfd;

    sendNetworkCommand(request, sockfd);                                                   
    NetworkCommand* response = readMessage(sockfd);
    freeCMND(request);

    if (checkresponse("file", response) < 0) return -1;
    
    Update* update = createupdate(response->argv[2], response->arglengths[2], command->args[0], projlen);       // Returns NULL if there was a conflict 
    freeCMND(response);

    if (!update) {
        printf("Error: couldn't prepare update, conflicts exist. resolve the conflicts and try again.\n");
        return -1;
    }

    printf("Successfuly prepared update with '%d' changes\n", update->entries);

    return 0;

}

int _upgrade(ClientCommand* command) {
    
    // proj dont exist
    // server no contact
    // yes .Conflict
    // no .Commit

    char* project = command->args[0];
    int projlen = strlen(project);

    if(!checkForLocalProj(project)) return -1;

    char* conflictpath = malloc(projlen + 11);
    memset(conflictpath, '\0', projlen + 11);
    sprintf(conflictpath, "%s/.Conflict", project);
    
    char* updatepath = malloc(projlen + 9);
    memset(updatepath, '\0', projlen + 9);
    sprintf(updatepath, "%s/.Update", project);    

    int fd = open(conflictpath, O_RDONLY);
    if (fd > 0) {
        printf("Error: Conflicts in project '%s' exist. Resolve conflict before attempting to upgrade.\n", command->args[0]);
        free(conflictpath);
        close(fd);
        return -1;
    }
    close(fd);
    free(conflictpath);

    fd = open(updatepath, O_RDONLY);
    if (fd < 0) {
        printf("Error: Project '%s' has no pending updates. Update before attempting to upgrade.\n", command->args[0]);
        free(updatepath);
        return -1;
    }
    close(fd);


    FileContents* file = readfile(updatepath);
    Commit* update = parseCommit(file);
    freefile(file);
    char** files = getCommitFilePaths(update);
    char** hashes = getCommitHashes(update);
    int* versions = getCommitVersions(update);
    ModTag* tags = getModificationTags(update);

    int sockfd = connectwithconfig();
    
    NetworkCommand* requestversion = malloc(sizeof(NetworkCommand));
    if (!requestversion) {
        printf("no alloc %s", strerror(errno));
        return -1;
    }
    requestversion->type = upgradenet;
    requestversion->argc = 1;
    requestversion->argv = malloc(sizeof(char*));
    requestversion->arglengths = malloc(sizeof(int));
    requestversion->arglengths[0] = strlen(project);
    requestversion->argv[0] = malloc(requestversion->arglengths[0] + 1);
    memset(requestversion->argv[0], '\0', requestversion->arglengths[0] + 1);
    memcpy(requestversion->argv[0], project, requestversion->arglengths[0]);

    sendNetworkCommand(requestversion, sockfd);

    // Recieve files from server
    NetworkCommand* responseversion = readMessage(sockfd);

    if(checkresponse("upgrade", responseversion) < 0) return -1;

    char* manifestpath = malloc(projlen + 11);
    memset(manifestpath, '\0', projlen + 11);
    sprintf(manifestpath, "%s/.Manifest", project);    

    FileContents* manifestfile = readfile(manifestpath);
    Manifest* manifest = parseManifest(manifestfile);
    freefile(manifestfile);
    char** fileslocal = getManifestFiles(manifest);

    char* tempmanifestpath = malloc(projlen + 16);
    memset(tempmanifestpath, '\0', projlen + 16);
    sprintf(tempmanifestpath, "%s/.temp.Manifest", project);    

    fd = open(tempmanifestpath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

    write(fd, responseversion->argv[2], responseversion->arglengths[2]);
    write(fd, "\n", 1);

    int i = 0;
    int j = 0;
    for (i = 0; i < manifest->entrycount; i++) {

        int checked = 0;

        for (j = 0; j < update->entries; j++) {
            if (strcmp(fileslocal[i], files[j]) == 0) {
                checked = 1;
                if (tags[j] == Delete) {
                    break;                  // skip, do not add to new .Manifest
                }                                        
                // Else its modify, replace manifest entry with update entry

                int entrylen = digitCount(versions[j]) + 1 + strlen(files[j]) + 1 + 40 + 2;
                char entry[entrylen];
                snprintf(entry,  entrylen, "%d %s %s\n", versions[j], files[j], hashes[j]);
                
                write(fd, entry, entrylen - 1);

            }
        }

        if (!checked) {
            // file exists in manifest, not in .Update (local add)
            write(fd, manifest->entries[i], strlen(manifest->entries[i]) - 1);
            write(fd, "\n", 1);
        }

    }

    for (i = 0; i < update->entries; i++) {
        if (tags[i] != Add) continue;

        int entrylen = digitCount(versions[i]) + 1 + strlen(files[i]) + 1 + 40 + 2;
        char entry[entrylen];
        snprintf(entry,  entrylen, "%d %s %s\n", versions[i], files[i], hashes[i]);
        
        write(fd, entry, entrylen - 1);

    }

    close(fd);
   
    int filelen = 1;
    for(i = 0; i < update->entries; i++) {
        filelen += strlen(files[i]) + 1;
    }
    int notdeleted = 0;
    char** stripedfiles = malloc(filelen * sizeof(char*));
    for(i = 0; i < update->entries; i++) {
        if (tags[i] == Delete) continue;

        strtok(files[i], "/");
        char* entry = strtok(NULL, "\0");

        stripedfiles[notdeleted] = malloc(strlen(entry) + 1);
        memset(stripedfiles[notdeleted], '\0', strlen(entry) + 1);
        memcpy(stripedfiles[notdeleted], entry, strlen(entry));

        notdeleted++;

    }

    // Replace files in local repo
    if (notdeleted != 0) {
        NetworkCommand* request = newfilerequest(project, stripedfiles, notdeleted);
        request->type = upgradenet;
        freeCommit(update);

        sendNetworkCommand(request, sockfd);

        // Recieve files from server
        NetworkCommand* response = readMessage(sockfd);

        if(checkresponse("file", response) < 0) return -1;

        recreatefile("archive.tar.gz", response->argv[2], response->arglengths[2]);
        uncompressfile("archive.tar.gz");

    } else {
        NetworkCommand* cancel = newFailureCMND("upgrade", "no files");
        sendNetworkCommand(cancel, sockfd);
    }

    remove(manifestpath);
    rename(tempmanifestpath, manifestpath);
    remove(updatepath);

    printf("Local repo upgraded successfully\n");
    return -1;
}

Update* createupdate(char* remoteManifest, int remotelen, char* project, int projlen) {

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

    if (getManifestVersion(servermanifest) == getManifestVersion(localmanifestfile)) {
        printf("Local project is up to date!\n");
        freefile(localmanifestfile);
        freefile(servermanifest);
        
        Update* update = malloc(sizeof(Update));
        update->uptodate = 1;

        return update;
    }

    Manifest* localmanifest = parseManifest(localmanifestfile);
    char** files = getManifestFiles(localmanifest);
    char** hashcodes = getManifestHashcodes(localmanifest);
    int* fileversions = getManifestFileVersion(localmanifest);

    Manifest* remotemanifest = parseManifest(servermanifest);
    char** remotefiles = getManifestFiles(remotemanifest);
    char** remotehashcodes = getManifestHashcodes(remotemanifest);
    int* remotefileversions = getManifestFileVersion(remotemanifest);

    if (localmanifest->entrycount == 0 && remotemanifest->entrycount == 0) {
        printf("Local project is up to date!\n");
        freeManifest(localmanifest);
        freeManifest(remotemanifest);
        freefile(localmanifestfile);
        freefile(servermanifest);
        return NULL;
    }

    Update* update = malloc(sizeof(Update));
    update->uptodate = 0;
    update->entries = 0;
    update->uid = -1;

    update->filecontent = malloc(localmanifestfile->size + servermanifest->size + 1);
    memset(update->filecontent, '\0', localmanifestfile->size + servermanifest->size+ 1);
    update->filesize = 0;

    freefile(localmanifestfile);
    freefile(servermanifest);

    /*

        Modify Case:
            client hash != remote hash
            client hash == live hash
        
        Add Case:
            exists on remote
            doesn't exist on local

        Delete Case:
            exists on local
            doesn't exist on remote

    */

    char* updatepath = malloc(11 + projlen);
    memset(updatepath, '\0', 11 + projlen);
    sprintf(updatepath, "%s/.Update", project);

    char* conflictpath = malloc(11 + projlen);
    memset(conflictpath, '\0', 11 + projlen);
    sprintf(conflictpath, "%s/.Conflict", project);
    
    int update_fd = open(updatepath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    int conflict_fd = open(conflictpath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

    int conflict = 0;

    int i = 0,j = 0;
    for ( i = 0; i < localmanifest->entrycount; i++) {

        int check = 0;

        for (j = 0; j < remotemanifest->entrycount; j++) {
            
            if (strcmp(files[i], remotefiles[j]) != 0) continue; 
            
            check = 1;
            
            if (strcmp(hashcodes[i], remotehashcodes[j]) != 0) {
                
                FileContents* livecontent = readfile(files[i]);
                unsigned char* livehashcode = hashdata((unsigned char*) livecontent->content, livecontent->size);

                if (strcmp((const char*)hashcodes[i], (const char*)livehashcode) == 0) {

                    // if (conflict) break;

                    // Modify Case
                    int entrylen = 2 + digitCount(fileversions[i]) + 1 + strlen(files[i]) + 1 + 40 + 2;
                    char entry[entrylen];

                    sprintf(entry, "M %d %s %s\n", fileversions[i], files[i], hashcodes[i]);                    
                    write(update_fd, entry, entrylen - 1);

                    printf("M %s\n", files[i]);

                    update->entries++;
                    update->filesize += entrylen - 1;

                } else {

                    // Conflict Case
                    conflict = 1;

                    int entrylen = 2 + digitCount(fileversions[i]) + 1 + strlen(files[i]) + 1 + 40 + 2;
                    char entry[entrylen];

                    sprintf(entry, "C %d %s %s\n", fileversions[i], files[i], hashcodes[i]);                    
                    write(conflict_fd, entry, entrylen - 1);

                    printf("C %s\n", files[i]);

                }

            }

            break;

        }

        if (!check) {

            // Delete Case

            int entrylen = 2 + digitCount(fileversions[i]) + 1 + strlen(files[i]) + 1 + 40 + 2;
            char entry[entrylen];

            sprintf(entry, "D %d %s %s\n", fileversions[i], files[i], hashcodes[i]);                    
            write(update_fd, entry, entrylen - 1);

            printf("D %s\n", files[i]);

            update->entries++;
            update->filesize += entrylen - 1;

        }

    }

    for ( i = 0; i < remotemanifest->entrycount; i++) {
        // if (conflict) break;
        int check = 0;
        for (j = 0; j < localmanifest->entrycount; j++) {
            if (strcmp(files[j], remotefiles[i]) != 0) continue;
            check = 1;
            break;
        }

        if (!check) {

            // Add Case
            int entrylen = 2 + digitCount(remotefileversions[i]) + 1 + strlen(remotefiles[i]) + 1 + 40 + 2;
            char entry[entrylen];

            sprintf(entry, "A %d %s %s\n", remotefileversions[i], remotefiles[i], remotehashcodes[i]);                    
            write(update_fd, entry, entrylen - 1);

            printf("A %s\n", remotefiles[i]);

            update->entries++;
            update->filesize += entrylen - 1;

        }
    }

    update->filecontent = malloc(update->filesize);
    read(update_fd, update->filecontent, update->filesize);

    if (conflict) remove(updatepath);
    else remove(conflictpath);

    if (update->entries == 0) remove(updatepath);

    if (conflict) return NULL;
    return update;

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

    char* localmanifestpath = malloc(11 + projlen);
    memset(localmanifestpath, '\0', 11 + projlen);
    sprintf(localmanifestpath, "%s/.Manifest", command->args[0]);
    fd = open(localmanifestpath, O_RDONLY);
    if (fd < 0) {
        printf("Error: Project does not contain a Manifest\n");
        free(localmanifestpath);
        return -1;
    }
    free(localmanifestpath);
    close(fd);

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

    NetworkCommand* toSend = newDataTransferCmnd(command->args[0], commit->filecontent, strlen(commit->filecontent));      // Send commit file to server
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

        char newconfig[config->size + finalresponse->arglengths[2]];
        memset(newconfig, '\0', config->size + finalresponse->arglengths[2]);
        strcat(newconfig, strtok(config->content, "\n"));
        strcat(newconfig, "\n");
        strcat(newconfig, strtok(NULL, "\n"));
        strcat(newconfig, "\n");
        strcat(newconfig, strtok(NULL, ":"));
        strcat(newconfig, ":");
        strcat(newconfig, finalresponse->argv[2]);
        strcat(newconfig, "\n");
        
        remove(".config");

        int newfd = open(".config", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        write(newfd, newconfig, config->size + finalresponse->arglengths[2] - 1);

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

    commit->filecontent = malloc(localmanifestfile->size + servermanifest->size + 2);
    memset(commit->filecontent, '\0', localmanifestfile->size + servermanifest->size+ 2);
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
            char* commitentry = malloc(strlen(files[i]) + digitCount(fileversions[i]) + 40 + 6);
            memset(commitentry, '\0', strlen(files[i]) + digitCount(fileversions[i]) + 40 + 6);
            sprintf(commitentry, "A %s", files[i]); 
            printf("%s\n", commitentry);
            sprintf(commitentry, "A %d %s %s\n",fileversions[i] + 1, files[i], hashcodes[i]);
            strcat(commit->filecontent, commitentry);
            free(commitentry);
            commit->filesize += strlen(files[i]) + digitCount(fileversions[i]) + 45;
            commit->entries += 1;
        } else {
            // file is already in remote, check that is up to date with live file
            FileContents* livecontent = readfile(files[i]);
            unsigned char* livehashcode = hashdata((unsigned char*) livecontent->content, livecontent->size);
            char* encodedlivehash = hashtohex(livehashcode);
            free(livehashcode);

            if (strcmp(encodedlivehash, hashcodes[i]) != 0) {
               
                if (strcmp((const char*)hashcodes[i],(const char*) remotehashcodes[j]) != 0) {    // check that local manifest hash matches the server manifest hash
                    printf("Error: There is an inconsistency between the remote project and the local project, skiping file '%s'\n", files[i]);
                    freefile(livecontent);
                    free(encodedlivehash);
                    continue;
                }

                // Modify condition
                char* commitentry = malloc(strlen(files[i]) + digitCount(fileversions[i]) + 46);
                memset(commitentry, '\0', strlen(files[i]) + digitCount(fileversions[i]) + 46);
 
                printf("M %s\n", files[i]);
                sprintf(commitentry, "M %d %s %s\n", fileversions[i], files[i], hashcodes[i]); 

                strcat(commit->filecontent, commitentry);
                free(commitentry);
                commit->filesize  += strlen(files[i]) + digitCount(fileversions[i]) + 45;
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
            char* commitentry = malloc(strlen(remotefiles[i]) + 40 + 2 + 4 + 1);
            memset(commitentry, '\0', strlen(remotefiles[i]) + 40 + 2 + 4 + 1);
            sprintf(commitentry, "D %s", remotefiles[i]); 
            printf("%s\n", commitentry);
            sprintf(commitentry, "D 0 %s %s\n", remotefiles[i], remotehashcodes[i]); 
            strcat(commit->filecontent, commitentry);
            free(commitentry);
            commit->filesize += strlen(remotefiles[i]) + 40 + 2 + 4;
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
    remove(commitpath);
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

    // strcat(commit->filecontent, "\n");
    write(fd, commit->filecontent, commit->filesize);      // TODO MAKE SURE ALL BYTES ARE WRITTEN TO FILE
    close(fd);

    return commit;

}

int _push(ClientCommand* command) {

    // check that a .Commit exists
    if (!checkForLocalProj(command->args[0])) {
        printf("Error: Local project does not exist. Checkout project from server or create a new one.\n");
        return -1;
    }

    char* commitpath = malloc(strlen(command->args[0]) + 9);
    sprintf(commitpath, "%s/.Commit",command->args[0]);
    FileContents* commit_file = readfile(commitpath);

    if (!commit_file) {
        printf("Error: You have no changes commited, run ./WTF commit then try push again\n");
        return -1;
    }

    Commit* commit = parseCommit(commit_file);

    char** paths = getCommitFilePaths(commit);
    ModTag* tags = getModificationTags(commit);

    printf("Pushing Changes:\n");
    int i = 0;
    for(i = 0; i < commit->entries; i++) {
        printf("%s\t%s\n", tags[i] == Add ? "Add" : tags[i] == Modify ? "Modifiy" : "Delete", paths[i]);
    }

    unsigned char* localhash = hashdata((unsigned char*) commit->filecontent, commit->filesize);
    char* localencodedhash = hashtohex(localhash);
    free(localhash);

    NetworkCommand* request = malloc(sizeof(NetworkCommand));
    request->type = pushnet;
    request->argc = 3;
    request->arglengths = malloc(3 * sizeof(int));
    request->argv = malloc(3 * sizeof(char*));

    request->arglengths[0] = strlen(command->args[0]);     // project name
    request->argv[0] = malloc(request->arglengths[0]+1);
    memset(request->argv[0], '\0', request->arglengths[0]);
    memcpy(request->argv[0], command->args[0], request->arglengths[0]);

    int uid = getUID();
    request->arglengths[1] = digitCount(uid);     // uid
    request->argv[1] = malloc(request->arglengths[1] + 1);
    memset(request->argv[1], '\0', request->arglengths[1]);
    sprintf(request->argv[1], "%d", uid);
    
    request->arglengths[2] = SHA_DIGEST_LENGTH * 2;     // commit hash
    request->argv[2] = malloc(SHA_DIGEST_LENGTH * 2 + 1);
    memset(request->argv[2], '\0', request->arglengths[2]);
    memcpy(request->argv[2], localencodedhash, request->arglengths[2]);    

    int sockfd = connectwithconfig();

    sendNetworkCommand(request, sockfd);

    NetworkCommand* responseA = readMessage(sockfd);

    // check response is success
    if(checkresponse("push", responseA) < 0) return -1;

    int len = 1;
    for (i = 0; i < commit->entries; i++) if (tags[i] != Delete) len += strlen(paths[i]) + 1;  

    char* tarfiles = malloc(len);
    memset(tarfiles,'\0',len + 1);
    for (i = 0; i < commit->entries; i++) {
        if (tags[i] != Delete) {
            strcat(tarfiles, paths[i]);  
            strcat(tarfiles, " ");
        }
    } 

    int filesize = 0;
    char* files = getcompressedfile(tarfiles, &filesize, NULL, NULL);

    NetworkCommand* toSend = newDataTransferCmnd(command->args[0], files, filesize);      // Send commit file to server
    sendNetworkCommand(toSend, sockfd);
    freeCMND(toSend);

    NetworkCommand* responseB = readMessage(sockfd);

    if(checkresponse("push", responseB) == 0) printf("Commits have been successfully pushed to remote\n");
    


    // Update manifest versions

    int projlen = strlen(command->args[0]);
    char manifestpath[projlen + 11];
    sprintf(manifestpath, "%s/.Manifest", command->args[0]);

    FileContents* manifestcontent = readfile(manifestpath);
    if (!manifestcontent) {
        printf("Error: Manifest does not exist or cannot be accessed\n");
        return -1;
    }

    Manifest* manifest = parseManifest(manifestcontent);    // needs to be freed
    char** manifestfiles = getManifestFiles(manifest);              // needs to be freed
    char** hashcodes = getManifestHashcodes(manifest);

    int* versions = getCommitVersions(commit);

    char new_manifestpath[projlen + 16];
    sprintf(new_manifestpath, "%s/.temp.Manifest", command->args[0]);

    int new_fd = open(new_manifestpath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

    if (new_fd < 0) {
        printf("Error: couldn't update Manifest: %s\n", strerror(errno));
        freefile(manifestcontent);
        return -1;
    }

    int ver = getManifestVersion(manifestcontent);
    char ver_s[digitCount(ver + 1) + 2];
    sprintf(ver_s, "%d\n", ver + 1);
    write(new_fd, ver_s, digitCount(ver + 1) + 1);

    int j = 0;
    for (i = 0; i < manifest->entrycount; i++) {
        int check = 0;
        for (j = 0; j < commit->entries; j++) {
            if (strcmp(manifestfiles[i], paths[j]) != 0) continue;
            check = 1;
            // update file version in manifest
            int entrylen = strlen(manifest->entries[i]) + 2;
            char entry[entrylen];
            memset(entry, '\0', entrylen);
            sprintf(entry, "%d %s %s\n", versions[j], paths[j], hashcodes[i]);
            write(new_fd, entry, entrylen - 1);
        }
        if (!check) {
            write(new_fd, manifest->entries[i], strlen(manifest->entries[i]));
            write(new_fd, "\n", 1);
        }
    }

    rename(new_manifestpath, manifestpath);

    remove(commitpath);
    
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

    char* project = command->args[0];
    int projlen = strlen(project);
    char* file = command->args[1];
    int filelen = strlen(file);

    if(!checkForLocalProj(command->args[0])) {      // Check project exists
        printf("Error: project does not exist locally, checkout the project or create it if it does not exist\n");
        return -1;
    }

    char fullpath[projlen + filelen + 2];
    sprintf(fullpath, "%s/%s", project, file);

    char manifestpath[projlen + 11];
    sprintf(manifestpath, "%s/.Manifest", project);

    FileContents* filecontents = readfile(fullpath);
    if (!filecontents) {
        printf("Error: file does not exist or cannot be accessed\n");
        return -1;
    }

    FileContents* manifestcontent = readfile(manifestpath);
    if (!manifestcontent) {
        printf("Error: Manifest does not exist or cannot be accessed\n");
        freefile(filecontents);
        return -1;
    }

    Manifest* manifest = parseManifest(manifestcontent);    // needs to be freed
    char** files = getManifestFiles(manifest);              // needs to be freed
    int i = 0;
    for (i = 0; i < manifest->entrycount; i++) {
        if (strcmp(files[i], fullpath) != 0) continue;
        printf("Error: this file has already been added to the manifest\n");
        freefile(filecontents);
        freefile(manifestcontent);
        return -1;
    }

    unsigned char* hash = hashdata((unsigned char*) filecontents->content, filecontents->size);
    char* encoded_hash = hashtohex(hash);
    free(hash);
   
    char newentry[projlen + filelen + 46];
    sprintf(newentry, "0 %s %s\n", fullpath, encoded_hash);

    freefile(filecontents);
    free(encoded_hash);

    char new_manifestpath[projlen + 16];
    sprintf(new_manifestpath, "%s/.temp.Manifest", project);

    int new_fd = open(new_manifestpath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

    if (new_fd < 0) {
        printf("Error: couldn't update Manifest: %s\n", strerror(errno));
        freefile(manifestcontent);
        return -1;
    }

    write(new_fd, manifestcontent->content, manifestcontent->size);
    write(new_fd, newentry, projlen + filelen + 45);
    freefile(manifestcontent);

    close(new_fd);

    rename(new_manifestpath, manifestpath);

    printf("File '%s' has been successfully added\n", file);
    
    return 0;

}

int _remove(ClientCommand* command) {

    char* project = command->args[0];
    int projlen = strlen(project);
    char* file = command->args[1];
    int filelen = strlen(file);

    if(!checkForLocalProj(command->args[0])) {      // Check project exists
        printf("Error: project does not exist locally, checkout the project or create it if it does not exist\n");
        return -1;
    }

    char fullpath[projlen + filelen + 2];
    sprintf(fullpath, "%s/%s", project, file);

    char manifestpath[projlen + 11];
    sprintf(manifestpath, "%s/.Manifest", project);

    char new_manifestpath[projlen + 16];
    sprintf(new_manifestpath, "%s/.temp.Manifest", project);

    FileContents* manifestcontent = readfile(manifestpath);
    if (!manifestcontent) {
        printf("Error: Manifest does not exist or cannot be accessed\n");
        return -1;
    }

    int new_fd = open(new_manifestpath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (new_fd < 0) {
        printf("Error: couldn't update Manifest: %s\n", strerror(errno));
        freefile(manifestcontent);
        return -1;
    }

    Manifest* manifest = parseManifest(manifestcontent);    // needs to be freed
    char** files = getManifestFiles(manifest);              // needs to be freed

    int ver = getManifestVersion(manifestcontent);
    char ver_s[digitCount(ver) + 2];
    sprintf(ver_s, "%d\n", ver);
    write(new_fd, ver_s, digitCount(ver) + 1);

    int i = 0;
    int check = 0;
    for (i = 0; i < manifest->entrycount; i++) {
        if (strcmp(files[i], fullpath) == 0) check = 1;
        else {
            int len = strlen(manifest->entries[i]) + 2;
            char entry[len];
            sprintf(entry, "%s\n", manifest->entries[i]);
            write(new_fd, entry, len - 1);
        }
    }

    if (!check) {
        printf("Error: this file has not been added to the manifest\n");
        remove(new_manifestpath);
        freefile(manifestcontent);
        freeManifest(manifest);
        return -1;
    }

    close(new_fd);
    rename(new_manifestpath, manifestpath);

    printf("File '%s' successfully removed from the project\n", file);

    return 0;

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

    int sockfd = connectwithconfig();

    char* file = "History";
    NetworkCommand* historyrequest = newfilerequest(command->args[0], &file, 1);
    sendNetworkCommand(historyrequest, sockfd);
    freeCMND(historyrequest);

    NetworkCommand* response = readMessage(sockfd);

    if (checkresponse("history", response) < 0) return -1;

    printf("Project '%s' History:\n%s", command->args[0], response->argv[2]);

    return 0;

}

int _rollback(ClientCommand* command) {

    NetworkCommand* request = malloc(sizeof(NetworkCommand));
    request->type = rollbacknet;
    request->argc = 2;
    request->argv = malloc(sizeof(char*) * 2);
    request->arglengths = malloc(sizeof(int) * 2);

    request->arglengths[0] = strlen(command->args[0]);
    request->argv[0] = malloc(request->arglengths[0] + 1);
    sprintf(request->argv[0], "%s", command->args[0]);

    request->arglengths[1] = strlen(command->args[1]);
    request->argv[1] = malloc(request->arglengths[1] + 1);
    sprintf(request->argv[1], "%s", command->args[1]);

    int sockfd = connectwithconfig();

    sendNetworkCommand(request, sockfd);

    NetworkCommand* response = readMessage(sockfd);

    checkresponse("rollback", response);

    return 0;
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