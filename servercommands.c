#include "servercommands.h"
#include <errno.h>

pthread_mutex_t repo_lock = PTHREAD_MUTEX_INITIALIZER;

int servercheckForLocalProj(char* projectname) {
	pthread_mutex_lock(&repo_lock);
	int ret = checkForLocalProj(projectname);
	pthread_mutex_unlock(&repo_lock);
	return ret;
}

FileContents* serverreadfile(char* name) {
	pthread_mutex_lock(&repo_lock);
	FileContents* content = readfile(name);
	pthread_mutex_unlock(&repo_lock);
	return content;
}

ssize_t serverwrite(int fd, char* buff, int count) {
	pthread_mutex_lock(&repo_lock);
	ssize_t ret = write(fd, buff, count);
	pthread_mutex_unlock(&repo_lock);
	return ret;
}

int _responsenet(NetworkCommand* command, int sockfd) {

	// response arg0- command responding to
	//			arg1- success or failure
	//			arg2- response data / reason for failure

	if (command->argc != 3) {
		printf("Error: Recieved malformed message \n");
		return -1;
	}

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
	memset(name, '\0', 9);
	memcpy(name, "check", 9);
	char* reason = malloc(16);
	memset(reason, '\0', 16);
	memcpy(reason, "not implimented", 16);
	NetworkCommand* response = newFailureCMND(name, reason);	
	sendNetworkCommand(response, sockfd);
	return 0;
}

int _createnet(NetworkCommand* command, int sockfd) {
		// version  arg0- project name
	char* name = malloc(9);
	memset(name, '\0', 9);
	memcpy(name, "create", 9);

	if (command->argc != 1) {
		printf("Error: Malformed message from client\n");
		char* reason = malloc(18);
		memcpy(reason, "Malformed message", 18);	
		NetworkCommand* response = newFailureCMND(name, reason);	
		sendNetworkCommand(response, sockfd);
		freeCMND(response);
		return -1;
	}

	if (servercheckForLocalProj(command->argv[0])) {
		printf("Error: Request for invalid project from client\n");
		char* reason = malloc(22);
		memcpy(reason, "Project already exist", 22);	
		NetworkCommand* response = newFailureCMND(name, reason);	
		sendNetworkCommand(response, sockfd);
		freeCMND(response);
		return -1;
	}

	if (mkdir(command->argv[0], 0700)) {
		printf("Error: failed to create new project directory: %s\n", strerror(errno));
	}
	char* newmanifest = malloc(command->arglengths[0] + 11);
	memset(newmanifest, '\0', command->arglengths[0] + 11);
	strcat(newmanifest, command->argv[0]);
	strcat(newmanifest, "/.Manifest");
	int fd = open(newmanifest, O_WRONLY | O_CREAT, S_IRWXU);
	if (fd < 0) {
		printf("Error: failed to create new manifest is project: %s\n", strerror(errno));
	} else { 
		serverwrite(fd, "1\n", 2);
		close(fd);
		free(newmanifest);
	}
	printf("New project created '%s'\n",command->argv[0]);

	char* reason = malloc(command->arglengths[0] + 1);
	memset(reason, '\0', command->arglengths[0] + 1);
	memcpy(reason, command->argv[0], command->arglengths[0]);
	NetworkCommand* response = newSuccessCMND(name, reason);
	sendNetworkCommand(response, sockfd);
	freeCMND(response);

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
	// version  arg0- project name
	char* name = malloc(9);
	memset(name, '\0', 9);
	memcpy(name, "version", 9);

	if (command->argc != 1) {
		printf("Error: Malformed message from client\n");
		char* reason = malloc(18);
		memcpy(reason, "Malformed message", 18);	
		NetworkCommand* response = newFailureCMND(name, reason);	
		sendNetworkCommand(response, sockfd);
		freeCMND(response);
		return -1;
	}

	if (!servercheckForLocalProj(command->argv[0])) {
		printf("Error: Request for invalid project from client\n");
		char* reason = malloc(24);
		memcpy(reason, "Project does not exist", 24);	
		NetworkCommand* response = newFailureCMND(name, reason);	
		sendNetworkCommand(response, sockfd);
		freeCMND(response);
		return -1;
	}
	
	int version = projectVersion(command->argv[0], &serverreadfile);
	char* reason = malloc(digitCount(version) + 1);
	memset(reason, '\0', digitCount(version) + 1);
	snprintf(reason, digitCount(version)+1, "%d", version);
	NetworkCommand* response = newSuccessCMND(name, reason);
	sendNetworkCommand(response, sockfd);
	freeCMND(response);
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

int executecommand(NetworkCommand* command, int sockfd) {

	switch (command->type){
		case responsenet:
			return _responsenet(command, sockfd);
			break;
		case checknet:
			return _checknet(command, sockfd);
			break;
		case createnet:
			return _createnet(command, sockfd);
			break;
		case destroynet:
			return _destroynet(command, sockfd);
			break;
		case projectnet:
			return _projectnet(command, sockfd);
			break;
		case rollbacknet:
			return _rollbacknet(command, sockfd);
			break;
		case versionnet:
			return _versionnet(command, sockfd);
			break;
		default:
			return -1;
			break;
	}

}
