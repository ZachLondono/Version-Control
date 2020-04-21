#include "networking.h"
#include <errno.h>

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
	int newline = 0;
	while (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		newline = 1;
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
	if (newline) printf("\n");
	
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
	} else if (strcmp(commandname, "create") == 0) {
		command->type = createnet;
	} else if (strcmp(commandname, "destroy") == 0) {
		command->type = destroynet;
	} else if (strcmp(commandname, "project") == 0) {
		command->type = projectnet;
	} else if (strcmp(commandname, "rollback") == 0) {
		command->type = rollbacknet;
	} else if (strcmp(commandname, "version") == 0) {
		command->type = versionnet;
	} else if (strcmp(commandname, "file") == 0) {
		command->type = filenet;
	} else if (strcmp(commandname, "response") == 0) {
		command->type = responsenet;
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

// Utilizies a continuous buffer shared between function calls to ensure no data is lost while minimizing read calls to increase efficiency
// of reading an unknown number of arbitrarily long sections which are either deliminated or of known length
char* readSection(int fd, int upperbound, char** contbuffer, int* buffersize, int ignoreDelim) {   // buffersize will always be at least the size of buffersize after function execute

    if (!(*contbuffer)) {
		printf("Error: continuous buffer is null\n");
		return NULL;
    }

    // The following code maintains the buffer and changes it's size/contents if neccessary
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
		if (status < 0 ) {
			printf("couldnt read message: %s\n", strerror(errno));
		}
		(*contbuffer)[upperbound] = '\0';                                      //make sure buffer ends with null byte
    }

    // The following code will parse out the desired section of data, and discard the deliminers if not ignored, or include them if ignored
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

// Convert the NetworkCommand struct into a char* which matches the network protocoll specifications
int sendNetworkCommand(NetworkCommand* command, int sockfd) {

	// Allocate enough memory to store the entire message
	int messagelen = CMND_NAME_MAX + 2 + digitCount(command->argc);
	int i = 0;
	for (i = 0; i < command->argc; i++) messagelen += command->arglengths[i] + 1 + digitCount(command->arglengths[i]);

	// Append the correct request type to the message
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

	
	// network protocoll structure: <command>:<argument count>:<argument 1 length>:<argument 1><argument 2 length>:<argument 3> ...
	// note: there is no deliminer between an argument's content and it's following argument's length
	int ret = write(sockfd, message, messagelen);

	free(message);

	return ret;

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
	memcpy(command->argv[1], "failure", 7);
	command->argv[2] = reason;
	return command;
}

NetworkCommand* newSuccessCMND(char* commandName, char* reason) {
	NetworkCommand* command = newFailureCMND(commandName, reason);
	memcpy(command->argv[1], "success", 7);
	return command;
}

void freeCMND(NetworkCommand* command) {

	if (!command) {
		printf("attempting to free null network command\n");
		return;
	} 

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

