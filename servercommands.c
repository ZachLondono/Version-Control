#include "servercommands.h"
#include <errno.h>

// This mutex and the following three "server" commands should be used when the server is accessing shared resouces as it 
// will lock access to the repo for other threads, to eliminate race conditions.
pthread_mutex_t repo_lock = PTHREAD_MUTEX_INITIALIZER;

int servercheckForLocalProj(char* projectname) {
	pthread_mutex_lock(&repo_lock);
	int ret = checkForLocalProj(projectname);
	pthread_mutex_unlock(&repo_lock);
	return ret;
}

int checkcommand(NetworkCommand* command, int expectedargs, char* name, int sockfd, int requireproj) {
	
	if (command->argc != expectedargs) {
		printf("Error: Malformed message from client\n");
		char* reason = malloc(18);
		memset(reason, '\0', 18);
		memcpy(reason, "Malformed message", 18);	
		NetworkCommand* response = newFailureCMND(name, reason);	
		sendNetworkCommand(response, sockfd);
		freeCMND(response);
		return 0;
	}

	if (servercheckForLocalProj(command->argv[0]) != requireproj) {
		printf("Error: Request for invalid project from client\n");
		char* reason;
		
		if (!requireproj) {
			reason = malloc(22);
			memset(reason, '\0', 22);
			memcpy(reason, "Project already exist", 22);
		} else {
			reason = malloc(24);
			memset(reason, '\0', 24);
			memcpy(reason, "Project does not exist", 24);	
		}

		NetworkCommand* response = newFailureCMND(name, reason);	
		sendNetworkCommand(response, sockfd);
		freeCMND(response);
		return 0;
	}

	return 1;

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

// creates a new project directory if it does not already exist, and initilizes a new .Manifest
int _createnet(NetworkCommand* command, int sockfd) {
	// version  arg0- project name
	char* name = malloc(9);
	memset(name, '\0', 9);
	memcpy(name, "create", 9);

	if (!checkcommand(command, 1, name, sockfd, 0)) return -1;

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
	// arg[0] = project name
	char* name = malloc(9);
	memcpy(name, "destroy", 9);

	if (!checkcommand(command, 1, name,sockfd,1)) return -1;

	char* removecmnd = malloc(26 + command->arglengths[0]);
	sprintf(removecmnd, "rm -r -f %s >/dev/null 2>&1", command->argv[0]);

	if (system(removecmnd) != 0) {
		char* reason = malloc(25);
		memset(reason, '\0',25);
		memcpy(reason, "couldn't destroy project", 25);
		NetworkCommand* response = newFailureCMND_B(name, reason, 25);
		sendNetworkCommand(response, sockfd);
		freeCMND(response);
		free(removecmnd);
		return -1;
	}
	free(removecmnd);

	char* reason = malloc(22);
	memset(reason, '\0', 22);
	memcpy(reason, "Project was destroyed", 22);
	NetworkCommand* response = newSuccessCMND_B(name, reason, 21);	
	sendNetworkCommand(response, sockfd);
	return 0;
}

int _projectnet(NetworkCommand* command, int sockfd) {
	// arg[0] = project name
	char* name = malloc(9);
	memcpy(name, "project", 9);

	if (!checkcommand(command, 1, name,sockfd,1)) return -1;

	int deflatedsize = 0;
	char* compressed = getcompressedfile(command->argv[0], &deflatedsize);

	NetworkCommand* response = newSuccessCMND_B(name,compressed, deflatedsize);	

	sendNetworkCommand(response, sockfd);
	freeCMND(response);
	return 0;
}

int _rollbacknet(NetworkCommand* command, int sockfd) {
	// arg[0] = project name, arg[1] = old version
	char* name = malloc(9);
	memcpy(name, "rollback", 9);
	char* reason = malloc(16);
	memcpy(reason, "not implimented", 16);
	NetworkCommand* response = newFailureCMND(name, reason);	
	sendNetworkCommand(response, sockfd);
	return 0;
}

// checks that a project exists and returns its .Manifest version
int _versionnet(NetworkCommand* command, int sockfd) {
	// version  arg0- project name
	char* name = malloc(9);
	memset(name, '\0', 9);
	memcpy(name, "version", 9);

	// Check project exists and request is formatted correctly
	if (!checkcommand(command, 1, name,sockfd,1)) return -1;
	
	// Get version from project's manifest
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
	// version  arg0- project name 		arg1- file name
	char* name = malloc(5);
	memset(name, '\0', 5);
	memcpy(name, "file", 5);

	if (command->argc != 2) {
		printf("Error: Malformed message from client\n");
		char* reason = malloc(18);
		memset(reason, '\0', 18);
		memcpy(reason, "Malformed message", 18);	
		NetworkCommand* response = newFailureCMND(name, reason);	
		sendNetworkCommand(response, sockfd);
		freeCMND(response);
		return -1;
	}

	if (!servercheckForLocalProj(command->argv[0])) {
		printf("Error: Request for invalid project from client\n");
		char* reason = malloc(24);
		memset(reason, '\0', 24);
		memcpy(reason, "Project does not exist", 24);	
		NetworkCommand* response = newFailureCMND(name, reason);	
		sendNetworkCommand(response, sockfd);
		freeCMND(response);
		return -1;
	}

	int filepath_size = command->arglengths[0] + command->arglengths[1] + 2;
	char* filepath = malloc(filepath_size);
	snprintf(filepath, filepath_size, "%s/%s", command->argv[0], command->argv[1]);

	int filesize = 0;
	char* filecontents = getcompressedfile(filepath, &filesize);
	free(filepath);

	NetworkCommand* response = newSuccessCMND_B(name, filecontents, filesize);

	sendNetworkCommand(response, sockfd);
	freeCMND(response);

	return 0;

}

int executecommand(NetworkCommand* command, int sockfd) {

	switch (command->type){
		case responsenet:
			return _responsenet(command, sockfd);
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
		case filenet:
			return _filenet(command, sockfd);
			break;
		default:
			return -1;
			break;
	}

}
