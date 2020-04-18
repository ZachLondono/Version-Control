#include "networking.h"
#include <errno.h>

// Connects a client to a remote computer on specified port
int connecttohost(char* remote, int port) {

	// create a socket file descriptor to read/write to
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) return -1;

	// get connection struct
	struct hostent* server = gethostbyname(remote);
	if (!server) return -1;

	// set up connection
	struct sockaddr_in serv_addr;
	bzero((char*)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);

	//Copy server's hostname address into the serv_addr struct
	bcopy((char *)server->h_addr, (char*)&serv_addr.sin_addr.s_addr, server->h_length);

	// connect to the server
	while (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		printf("\rAttempting to connect to host.  ");
		fflush(stdout);
		sleep(1);		
		printf("\rAttempting to connect to host.. ");
		fflush(stdout);
		sleep(1);		
		printf("\rAttempting to connect to host...");
		fflush(stdout);
		sleep(1);		
	}
	printf("\n");

	// return filedescriptor that can be written to/read from
	return sockfd;

}

NetworkCommand* readMessage(int sockfd) {

	if (sockfd < 0) {
		printf("Error: invalid socket file descriptor\n");
		return NULL;
	}

	int buffersize = 200;
	char* contbuffer = malloc(buffersize);
	memset(contbuffer, '\0', buffersize);
  

	char* commandname = readSection(sockfd, CMND_NAME_MAX, &contbuffer, &buffersize, 0);
	if (!commandname) {	// if command name was null, it was larger than max length
		commandname = malloc(CMND_NAME_MAX + 1);
		memset(commandname, '\0', CMND_NAME_MAX + 1);
		memcpy(commandname, "unknown", 7);
		char* reason = malloc(28);
		memcpy(reason, "Failed to read command name", 28);
		return newFailureCMND(commandname, reason);
	}
	char* argc_s = readSection(sockfd, 5, &contbuffer, &buffersize, 0);
	if (!argc_s) {	// if argc_s is null, the value overflowed the upperbounds of the buffer
		char* reason = malloc(30);
		memcpy(reason, "Failed to read argument count", 30);
		return newFailureCMND(commandname, reason);
	}
	
	int argc = atoi(argc_s);
	free(argc_s);
	char** argv = malloc(sizeof(char*) * argc);
	int* arglengths = malloc(sizeof(int) * argc);
	
	int i = 0;
	for(i = 0; i < argc; i++) {
		
		char* size_s = readSection(sockfd, 3, &contbuffer, &buffersize, 0);	// read in the length of the argument in bytes
		if (!size_s) {	// if size_s is null, the value is more digits than the upperbounds
			char* reason = malloc(29);
			memcpy(reason, "Failed to read argument size", 29);
			return newFailureCMND(commandname, reason);		
		}

		int size = atoi(size_s);
		arglengths[i] = size;
		free(size_s);

		char* buffer = readSection(sockfd, size, &contbuffer, &buffersize, 1);
		argv[i] = buffer;
	}
	
	free(contbuffer);

	NetworkCommand* command = malloc(sizeof(NetworkCommand));
	command->argc = argc;
	command->arglengths = arglengths;
	command->argv = argv;

	if (strcmp(commandname, "check") == 0) {
		command->type = checknet;
		command->operation = &_checknet;
	} else if (strcmp(commandname, "create") == 0) {
		command->type = createnet;
		command->operation = &_createnet;
	} else if (strcmp(commandname, "destroy") == 0) {
		command->type = destroynet;
		command->operation = &_destroynet;
	} else if (strcmp(commandname, "project") == 0) {
		command->type = projectnet;
		command->operation = &_projectnet;
	} else if (strcmp(commandname, "rollback") == 0) {
		command->type = rollbacknet;
		command->operation = &_rollbacknet;
	} else if (strcmp(commandname, "version") == 0) {
		command->type = versionnet;
		command->operation = &_versionnet;
	} else if (strcmp(commandname, "file") == 0) {
		command->type = filenet;
		command->operation = &_filenet;
	} else if (strcmp(commandname, "response") == 0) {
		command->type = responsenet;
		command->operation = &_responsenet;
	} else {
		char* name = malloc(CMND_NAME_MAX + 1);
		memcpy(name, commandname, CMND_NAME_MAX + 1);
		free(commandname);
		freeCMND(command);
		char* reason = malloc(28);
		memcpy(reason, "Failed to read command name", 28);
		return newFailureCMND(name, reason);
	}

	free(commandname);
	return command;

}

void freeCMND(NetworkCommand* command) {

	if (!command) return;

	if (command->arglengths != NULL) free(command->arglengths);
	if (command->argv != NULL) {
		int i = 0;
		for (i = 0; i < command->argc; i++) {
			if (command->argv[i] != NULL) free(command->argv[i]);
		}
		free(command->argv);
	}
	free(command);

}

NetworkCommand* newFailureCMND(char* commandName, char* reason) {
	NetworkCommand* command = malloc(sizeof(NetworkCommand));
	command->type = responsenet;
	command->argc = 3;
	command->argv = malloc(sizeof(char*) * 3);
	command->arglengths = malloc(sizeof(int) * 3);
	command->arglengths[0] = strlen(commandName);
	command->arglengths[1] = 7;
	command->arglengths[2] = strlen(reason);
	command->argv[0] = commandName;
	command->argv[1] = malloc(8);
	memcpy(command->argv[1], "failure", 8);
	command->argv[2] = reason;
	command->operation = &_responsenet;
	return command;
}

int _responsenet(NetworkCommand* command, int sockfd) {

	// response arg0- command responding to
	//			arg1- success or failure
	//			arg2- reason for failute (dne for success case)

	if (strcmp(command->argv[1], "success") == 0) {
		printf("Command %s was executed successfully\n", command->argv[0]);
		return 0;
	} else if (strcmp(command->argv[1], "failure") == 0) {
		if (command->argc == 3) printf("Command %s faild to execute: %s\n", command->argv[0], command->argv[2]);
		else printf("Command %s faild to execute\n", command->argv[0]);
		return 0;
	}
	
	return -1;

}

int _checknet(NetworkCommand* command, int sockfd) {
	char* name = malloc(9);
	memcpy(name, "check", 9);
	char* reason = malloc(16);
	memcpy(reason, "not implimented", 16);
	NetworkCommand* response = newFailureCMND(name, reason);	
	sendNetworkCommand(response, sockfd);
	return 0;
}

int _createnet(NetworkCommand* command, int sockfd) {
	char* name = malloc(9);
	memcpy(name, "create", 9);
	char* reason = malloc(16);
	memcpy(reason, "not implimented", 16);
	NetworkCommand* response = newFailureCMND(name, reason);	
	sendNetworkCommand(response, sockfd);
	return 0;
}

int _destroynet(NetworkCommand* command, int sockfd) {
	char* name = malloc(9);
	memcpy(name, "destroy", 9);
	char* reason = malloc(16);
	memcpy(reason, "not implimented", 16);
	NetworkCommand* response = newFailureCMND(name, reason);	
	sendNetworkCommand(response, sockfd);
	return 0;
}

int _projectnet(NetworkCommand* command, int sockfd) {
	char* name = malloc(9);
	memcpy(name, "project", 9);
	char* reason = malloc(16);
	memcpy(reason, "not implimented", 16);
	NetworkCommand* response = newFailureCMND(name, reason);	
	sendNetworkCommand(response, sockfd);
	return 0;
}

int _rollbacknet(NetworkCommand* command, int sockfd) {
	char* name = malloc(9);
	memcpy(name, "rollback", 9);
	char* reason = malloc(16);
	memcpy(reason, "not implimented", 16);
	NetworkCommand* response = newFailureCMND(name, reason);	
	sendNetworkCommand(response, sockfd);
	return 0;
}

int _versionnet(NetworkCommand* command, int sockfd) {
	char* name = malloc(9);
	memcpy(name, "version", 9);
	char* reason = malloc(16);
	memcpy(reason, "not implimented", 16);
	NetworkCommand* response = newFailureCMND(name, reason);	
	sendNetworkCommand(response, sockfd);
	return 0;
}

int _filenet(NetworkCommand* command, int sockfd) {
	char* name = malloc(9);
	memcpy(name, "file", 9);
	char* reason = malloc(16);
	memcpy(reason, "not implimented", 16);
	NetworkCommand* response = newFailureCMND(name, reason);	
	sendNetworkCommand(response, sockfd);
	return 0;
}

char* readSection(int fd, int upperbound, char** contbuffer, int* buffersize, int ignoreDelim) {   // buffersize will always be at least the size of buffersize after function execute

    if (!(*contbuffer)) {
		printf("Error: continuous buffer is null\n");
		return NULL;
	}
    size_t populatedbytes = strlen((*contbuffer));                          // number of bytes already read in
    if (populatedbytes <= upperbound) {                                     // check if enough bytes have been read already
        if (*buffersize <= upperbound) {                                    // if enough have not been read, make sure there is room in the buffer
            char* resized = realloc((*contbuffer), upperbound + 1);         // reallocate buffer to fit <= upperbound + 1 bytes
            if (!resized) {                                                 // check thatn buffer was able to be resized
                printf("Error: failed to allocate memory, buy some more ram you cheapskate, I only need %d more bytes...\n", upperbound + 1);
                return NULL;
            } 
            (*contbuffer) = resized;
            memset(&(*contbuffer)[*buffersize], '0', upperbound-*buffersize);   // null out new bytes
            *buffersize = upperbound + 1;
        }
        int status = 0;
        while (populatedbytes < upperbound && (status = read(fd, &(*contbuffer)[populatedbytes], upperbound - populatedbytes)) > 0) {
                populatedbytes += status;                                      // read in max bytes
		}
        (*contbuffer)[upperbound] = '\0';                                      //make sure buffer ends with null byte
    }

    char* ret = malloc(upperbound + 1);
    if (!ret) {
        printf("Error: failed to allocate memory, buy some more ram you cheapskate, I only need %d more bytes...\n",    upperbound + 1);
        return NULL;
    }
    memset(ret, '\0', upperbound + 1);

    if (ignoreDelim) {
        memcpy(ret, *contbuffer, upperbound);
		strshift((*contbuffer), *buffersize, upperbound); 	// skip over used bytes
        return ret;
    }

    size_t readindex = 0;
    while(readindex <= upperbound) {
        // parse out token
		if ((*contbuffer)[readindex] == ARGDELIM) {
            memcpy(ret, (*contbuffer), readindex);          // copy token from buffer
            break;
        }
		++readindex;
    }
	if (readindex > upperbound + 1) {
        free(ret);
        printf("Error: index out of bounds\n");
		return NULL;
	}

    strshift((*contbuffer), *buffersize, readindex + 1);    // readindex is the index of the last character before the delimiter, add 1 to skip delim

    return ret;

}

int sendNetworkCommand(NetworkCommand* command, int sockfd) {

	int messagelen = CMND_NAME_MAX + 1 + digitCount(command->argc);
	int i = 0;
	for (i = 0; i < command->argc; i++) messagelen += command->arglengths[i] + 1 + digitCount(command->arglengths[i]);

	char* message = malloc(messagelen);
	if (!message) return -1;
	memset(message,'\0', messagelen);
	switch (command->type) 	{
		case checknet:
			strcat(message,"check");
			break;
		case createnet:
			strcat(message,"create");
			break;
		case destroynet:
			strcat(message,"destroy");
			break;
		case projectnet:
			strcat(message,"project");
			break;
		case rollbacknet:
			strcat(message,"rollback");
			break;
		case versionnet:
			strcat(message,"version");
			break;
		case filenet:
			strcat(message,"file");
			break;
		case responsenet:
			strcat(message,"response");
			break;
		case invalidnet:
			strcat(message,"invalid");
			break;
		default:
			free(message);
			printf("Error: invalid message to send\n");
			return -1;
			break;
	}

	strcat(message,":");
	int l = digitCount(command->argc);
	char* num = malloc(l + 1);
	if (!num) return -1;
	snprintf (num, l+1, "%d",command->argc);
	strcat(message, num);
	free(num);
	strcat(message,":");
	
	for (i = 0; i < command->argc; i++) {
		
		l = digitCount(command->arglengths[i]);
		char* arg = malloc(l+1);
		if (!arg) return -1;
		snprintf (arg, l+1, "%d",command->arglengths[i]);
		strcat(message, arg);
		free(arg);
		strcat(message, ":");
		strcat(message,command->argv[i]);
	}

	return write(sockfd, message, messagelen);

}
