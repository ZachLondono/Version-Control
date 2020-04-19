#include "clientcommands.h"

//invalid=0, configure=1, checkout=2, update=3, upgrade=4, commit=5, push=6, create=7, destroy=8, add=9, remove_cmnd=10, currentversion=11, history=12, rollback=13
int connectwithconfig() {	// load config file and connect to host with it
	Configuration* config = loadConfig();
	if (config == NULL) return -1;
	int sockfd = connecttohost(config->host, config->port);
	freeConfig(config);
	return sockfd;
}

NetworkCommand* newrequest(ClientCommand* command, int argc) {
    
    NetworkCommand* request = malloc(sizeof(NetworkCommand));
    request->argc = 1;
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

int _invalidcommand(ClientCommand* command) {
    //TODO maybe set type of invalid command in parse command?
    printf("Fatal Error: %s\n", command->args[0]);
    return 0;
}

int _configure(ClientCommand* command) {

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

    int fd_config = open("./.config", O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
    if (fd_config == -1) {
        printf("Error: couldn't create .config file\n");
        return -1;
    }

    int size = strlen(command->args[0]) + strlen(command->args[0]) + 11;
    char* filecontents = malloc(sizeof(char) * (size));
    memset(filecontents, '\0', size);
    snprintf(filecontents, size, "host:%s\nport:%s", command->args[0], command->args[1]);

    if (write(fd_config, filecontents, size) == -1) {
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
    
    NetworkCommand* request = newrequest(command, 1);
    request->type = createnet;

    int sockfd = connectwithconfig();
    if (sockfd < 0) return sockfd;

    sendNetworkCommand(request, sockfd);
    freeCMND(request);

    NetworkCommand* response = readMessage(sockfd);
    close(sockfd);
    
    if (response->argc != 3 || strcmp(response->argv[0], "create") != 0) {
        printf("Error: Malformed response from server\n");
        freeCMND(response);
        return -1;
    }

    if (strcmp(response->argv[1], "failure") == 0) {
        printf("Error: server failed to create the project: %s\n", response->argv[2]);
        freeCMND(response);
        return -1;
    }

    printf("Project %s has been created on the repository\n", command->args[0]);

    freeCMND(response);
    
    return 0;

}

int _destroy(ClientCommand* command) {
    printf("destroy not implimented\n");
    return -1;
}

int _add(ClientCommand* command) {
    printf("add not implimented\n");
    return -1;
}

int _remove(ClientCommand* command) {
    printf("remove not implimented\n");
    return -1;
}

int _currentversion(ClientCommand* command) {

    NetworkCommand* request = newrequest(command, 1);
    request->type = versionnet;

    int sockfd = connectwithconfig();
    if (sockfd < 0) return sockfd;

    sendNetworkCommand(request, sockfd);
    freeCMND(request);

    NetworkCommand* response = readMessage(sockfd);
    close(sockfd);
    
    if (response->argc != 3 || strcmp(response->argv[0], "version") != 0) {
        printf("Error: Malformed response from server\n");
        freeCMND(response);
        return -1;
    }

    if (strcmp(response->argv[1], "failure") == 0) {
        printf("Error: server failed to recieve current version: %s\n", response->argv[2]);
        freeCMND(response);
        return -1;
    }

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

    FileContents* file = readfile("./.config");
    char* buffer = file->content;
    if (buffer == NULL) {
        printf("Error: .config file does not exist or has incorrect permissions\n");
        return NULL;
    }
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











