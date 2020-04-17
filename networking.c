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

int bindtoport(int sockfd, int port) {

	struct sockaddr_in serv_addr;

	bzero((char*)&serv_addr, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);

	return bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

}

NetworkCommand* readMessage(int sockfd) {

	if (sockfd < 0) {
		printf("Error: invalid socket file descriptor\n");
		return NULL;
	}

	char* commandname = readSection(sockfd, CMND_NAME_MAX);
	if (!commandname) {	// if command name was null, it was larger than max length
		commandname = malloc(CMND_NAME_MAX + 1);
		memset(commandname, '\0', CMND_NAME_MAX + 1);
		memcpy(commandname, "unknown", 7);
		char* reason = malloc(28);
		memcpy(reason, "Failed to read command name", 28);
		return newFailureCMND(commandname, reason);
	}
	char* argc_s = readSection(sockfd, 5);
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
		
		char* size_s = readSection(sockfd, 100);	// read in the length of the argument in bytes

		if (!size_s) {	// if size_s is null, the value is more digits than the upperbounds
			char* reason = malloc(29);
			memcpy(reason, "Failed to read argument size", 29);
			return newFailureCMND(commandname, reason);		
		}

		int size = atoi(size_s);
		arglengths[i] = size;
		free(size_s);

		char* buffer = malloc(size + 1);
		memset(buffer, '\0', size + 1);
		int bytesread = 0;
		int status = 0;
		while((status = read(sockfd, &buffer[bytesread], size - bytesread)) > 0) {
			bytesread += status;				// read the bytes to the allocate memory
			if (bytesread > size) break;
		}
		argv[i] = buffer;

	}
	
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
	command->type = invalidnet;
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

char* readSection(int fd, int upperbound) {

	char* buffer = malloc(upperbound + 2);	// +2 for null byte and delimiter
	memset(buffer, '\0', upperbound + 2);
	size_t bytesread = 0;
	int status = 0;
	while((status = read(fd, &buffer[bytesread], (upperbound+1) - bytesread)) > 0) { 
		bytesread += status;
		if (bytesread > upperbound) break;
	}
	size_t commandlength = 0;
	while(commandlength < upperbound + 1) {
		if (buffer[commandlength] == ARGDELIM) {
			memset(&buffer[commandlength], '\0', bytesread - commandlength + 1);	// Null out the rest of the buffer, after the name
			long offset = -(bytesread - 1 - commandlength);
			int a = lseek(fd,offset,1);									// Reset the fd pointer to just after the deliminator 
			if (a == -1) printf("error with lseek %s\n", strerror(errno));
			break;
		}
		++commandlength;
		if (commandlength > upperbound + 1) {
			return NULL;
		}
	}

	return buffer;	// can imporve memory usage by instead returning a char* of the exact size needed when section size is bellow upperbound

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

int digitCount(int num) {
	int count = 0;
	do {
    	count++;
		num /= 10;
    } while(num != 0);
	return count;
}