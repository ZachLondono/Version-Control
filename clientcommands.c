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
    printf("checkout not implimented\n");
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
    printf("commit not implimented\n");
    return -1;
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
    printf("destroy not implimented\n");
    return -1;
}

int _add(ClientCommand* command) {

    char* proj = command->args[0];
    char* file = command->args[1];

    if(!checkForLocalProj(command->args[0])) {
        printf("Error: project does not exist locally, checkout the project or create it if it does not exist\n");
        return -1;
    }

    int projlen = strlen(command->args[0]);
    int filelen = strlen(command->args[1]);
    char* fullpath = malloc(projlen + 2 + filelen);
    memset(fullpath, '\0', projlen + filelen + 2);
    sprintf(fullpath, "%s/%s", proj, file);

    FileContents* filecontents = readfile(fullpath);
    if (filecontents == NULL) {
        printf("Error: file does not exist or cannot be accessed\n");
        free(fullpath);
        return -1;
    }

    char* manifestpath = malloc(projlen + 11);
    memset(manifestpath, '\0', projlen + 11);
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

    if(strstr(manifest->content, fullpath) != NULL) {
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
    char* encodedhash = hashtohex(filehash);
    free(filehash);

    snprintf(newmanifest, len, "%s1 %s %s\n", manifest->content, fullpath, encodedhash);
    free(fullpath);
    free(encodedhash);
    freefile(filecontents);
    freefile(manifest);

    char* tempmanifest = malloc(projlen + 16);
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

    remove(manifestpath);
    rename(tempmanifest, manifestpath);
    free(tempmanifest);
    free(manifestpath);

    printf("File '%s' has been added\n", file);

    return 0;
    
}

int _remove(ClientCommand* command) {
    printf("remove not implimented\n");
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








