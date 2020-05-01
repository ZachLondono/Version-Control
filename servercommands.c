#include "servercommands.h"
#include <errno.h>

// This mutex and the following three "server" commands should be used when the server is accessing shared resouces as it 
// will lock access to the repo for other threads, to eliminate race conditions.
pthread_mutex_t repo_lock = PTHREAD_MUTEX_INITIALIZER;

Commit** activecommits = NULL;
int currentuid = 0;
int maxusers = 0;

int checkActiveCommitsSize(int uid) {	// will make sure that a given uid exists in the active commits, and that the active commits has been initialized
	if (uid >= maxusers || activecommits == NULL) {			
		maxusers = uid + 10;
		if (activecommits != NULL) {
			Commit** upsized = realloc(activecommits, maxusers * sizeof(Commit*));	// will resize active commits if neccessary
			if (!upsized) {
				printf("Error: couldn't allocate\n");
				return -1;
			}
			activecommits = upsized;
		} else activecommits = malloc(maxusers * sizeof(Commit*));

		if (!activecommits) return -1;

		int i = 0;
		for (i = 0; i < maxusers; i++) activecommits[i] = NULL;
		return 0;
	}
	return 0;
}

int nextUID() {
	while(1) {
		if (checkActiveCommitsSize(++currentuid) < 0) return -1;
		if (activecommits[currentuid] == NULL) break;
	}
	int uid = currentuid;
	return uid;
}

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

		char* name_cpy = malloc(strlen(name) + 1);
		memset(name_cpy, '\0', strlen(name) + 1);
		memcpy(name_cpy, name, strlen(name) + 1);

		NetworkCommand* response = newFailureCMND(name_cpy, reason);	
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

		char* name_cpy = malloc(strlen(name) + 1);
		memset(name_cpy, '\0', strlen(name) + 1);
		memcpy(name_cpy, name, strlen(name) + 1);

		NetworkCommand* response = newFailureCMND(name_cpy, reason);	
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

ssize_t serverwrite(int fd, char* buff, size_t count) {
	pthread_mutex_lock(&repo_lock);
	ssize_t ret = write(fd, buff, count);
	pthread_mutex_unlock(&repo_lock);
	return ret;
}

ssize_t serverread(int fd, char* buff, size_t count) {
	pthread_mutex_lock(&repo_lock);
	ssize_t ret = read(fd, buff, count);
	pthread_mutex_unlock(&repo_lock);
	return ret;
}

int serveropen(const char* pathname, int flags) {
	pthread_mutex_lock(&repo_lock);
	int ret = open(pathname, flags);
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

	pthread_mutex_lock(&repo_lock);
	if (system(removecmnd) != 0) {
		pthread_mutex_unlock(&repo_lock);
		char* reason = malloc(25);
		memset(reason, '\0',25);
		memcpy(reason, "couldn't destroy project", 25);
		NetworkCommand* response = newFailureCMND_B(name, reason, 25);
		sendNetworkCommand(response, sockfd);
		freeCMND(response);
		free(removecmnd);
		return -1;
	}
	pthread_mutex_unlock(&repo_lock);
	free(removecmnd);

	char* reason = malloc(22);
	memset(reason, '\0', 22);
	memcpy(reason, "Project was destroyed", 22);
	NetworkCommand* response = newSuccessCMND_B(name, reason, 21);	
	sendNetworkCommand(response, sockfd);
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
	// version  arg0- project name 		arg1- filecount   arg2- file1 name  arg3- file2 name ...
	char* name = malloc(5);
	memset(name, '\0', 5);
	memcpy(name, "file", 5);

	int filecount = atoi(command->argv[1]);

	if (!checkcommand(command, filecount + 2, name, sockfd,1)) return -1;

	int totallen = 1;
	char** fullpaths = malloc(filecount * sizeof(char*));	// full paths of all files 
	int i = 0;
	for (i = 0; i < filecount; i++) {
		
		int pathlen = command->arglengths[0] + command->arglengths[i+2] + 2;
		fullpaths[i] = malloc(pathlen);
		memset(fullpaths[i], '\0', pathlen);
		sprintf(fullpaths[i], "%s/%s", command->argv[0], command->argv[i+2]);
		
		totallen += pathlen + 1;

		int fd = serveropen(fullpaths[i], O_RDONLY);
		
		// Check that all files exist, if not cancel request
		if (fd < 0) {
			
			printf("Error: client requested file which doesn't exist or cant be opened\n");
			char* reason = malloc(56);
			memset(reason, '\0', 56);
			memcpy(reason, "Some files requested do not exist or cannot be accessed", 56);
			
			NetworkCommand* response = newFailureCMND_B(name, reason, 56);
			sendNetworkCommand(response, sockfd);
			free(response);
			int j = 0;
			for (j = 0; j <= i; j++) {
				free(fullpaths[j]);
			}
			free(fullpaths);

			return -1;

		}

		close(fd);

	}

	char* allpaths = malloc(totallen);
	memset(allpaths, '\0', totallen);
	for (i = 0; i < filecount; i++) {
		if (i != 0) strcat(allpaths, fullpaths[i]);
		strcat(allpaths, fullpaths[i]);
	}

	char* filecontents = NULL;
	int filesize = 0;
	if ((filecontents = getcompressedfile(allpaths, &filesize, serveropen, serverread)) == NULL) {
		printf("Error: failed to compress files\n");
		char* reason = malloc(23);
		memset(reason, '\0', 23);
		memcpy(reason, "Couldn't archive files", 23);
		
		NetworkCommand* response = newFailureCMND_B(name, reason, 23);
		sendNetworkCommand(response, sockfd);
		free(response);
		for (i = 0; i <= filecount; i++) {
			free(fullpaths[i]);
		}
		free(fullpaths);
		free(allpaths);
		return -1;
	}

	for (i = 0; i < filecount; i++) {
		free(fullpaths[i]);
	}
	free(fullpaths);
	free(allpaths);

	// int filepath_size = command->arglengths[0] + command->arglengths[1] + 2;
	// char* filepath = malloc(filepath_size);
	// snprintf(filepath, filepath_size, "%s/%s", command->argv[0], command->argv[1]);

	// int filesize = 0;
	// char* filecontents = getcompressedfile(filepath, &filesize);
	// free(filepath);

	NetworkCommand* response = newSuccessCMND_B(name, filecontents, filesize);

	sendNetworkCommand(response, sockfd);
	freeCMND(response);

	return 0;

}

int clientcommit(NetworkCommand* command, int sockfd) {

	command->type = filenet;
	command->argc = 3;			// Reformat command into a file request, so that it can be proccessed with _filenet function

	if (_filenet(command, sockfd) < 0) {
		printf("Failed to send server Manifest to client\n");
		return -1;
	}

	NetworkCommand* commitfile = readMessage(sockfd);

	if (commitfile->type == responsenet) {
		int ret = _responsenet(commitfile, sockfd);
		freeCMND(commitfile);
		return ret;
	} 

	int uid = atoi(command->argv[3]);	// User id is passed as third argument, will be -1 if client does not have one
	if (uid == -1) uid = nextUID();
	if (uid == -1) {
		printf("Error: failed to allocate memory for new commit\n");
		return -1;
	}
	
	char* name = malloc(5);
	memset(name,'\0', 5);
	memcpy(name, "data", 5);

	if(!checkcommand(commitfile, 2, name, sockfd, 1)) {
		printf("Failed to recieve client's changes\n");
		return -1;
	}

	printf("***********\nClient '%d' Commit\n%s***********\n", uid, commitfile->argv[1]);

	if (checkActiveCommitsSize(uid) < 0) return -1;
	if (activecommits[uid] != NULL) {
		printf("Overwriting client '%d's previous commit\n", uid);
		free(activecommits[uid]);
	}
	
	Commit* commit = malloc(sizeof(Commit*));
	commit->uid = uid;
	commit->filecontent = commitfile->argv[1];
	commit->filesize = commitfile->arglengths[1];
	activecommits[uid] = commit;

	freeCMND(commitfile);

	char* reason = malloc(6);
	memset(reason,'\0', 6);
	sprintf(reason, "%d", uid);
	NetworkCommand* response = newSuccessCMND(name, reason);
	sendNetworkCommand(response, sockfd);
	freeCMND(response);

	return 0;

}

// int _data(NetworkCommand* command, int sockfd) {

// 	char* name = malloc(5);
// 	memset(name, '\0', 5);
// 	memcpy(name, "data", 5);

// 	if (!checkcommand(command, 2, name, sockfd, 1)) return -1;

// 	//command->argv[1] contains a tar.gz with file to be copied into the server

// }

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
		case rollbacknet:
			return _rollbacknet(command, sockfd);
			break;
		case versionnet:
			return _versionnet(command, sockfd);
			break;
		case filenet:
			return _filenet(command, sockfd);
			break;
		case commitnet:
			return clientcommit(command, sockfd);
			break;
		// case data:
		// 	return _data(command, sockfd);
		// 	break;
		default:
			return -1;
			break;
	}

}
