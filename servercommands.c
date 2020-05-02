#include "servercommands.h"
#include <errno.h>

// This mutex and the following three "server" commands should be used when the server is accessing shared resouces as it 
// will lock access to the repo for other threads, to eliminate race conditions.
pthread_mutex_t repo_lock = PTHREAD_MUTEX_INITIALIZER;

int nextUID() {

	// check that curruid has not been already assigned to a client
	Node* currnode = getHead();
	currentuid++;
	while (currnode) {
		ProjectMeta* project = currnode->content;

		if (project->maxusers <= currentuid || project->activecommits[currentuid] != NULL) {
			currentuid++;
			currnode = getHead();
			continue;
		}

		currnode = currnode->next;
	}
	

	return currentuid;
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

ProjectMeta* getMetaData(char* project) {

	Node* node = getHead();
	while (node != NULL) {
		if (strcmp(((ProjectMeta*)node->content)->project, project) == 0) break;
		node = node->next;
	}

	if (node == NULL) {
		ProjectMeta* newproj = malloc(sizeof(ProjectMeta));

		int len = strlen(project) + 1;
		newproj->project = malloc(strlen(project) + 1);
		memset(newproj->project, '\0', len);
		memcpy(newproj->project, project, len);

		newproj->maxusers = 10;
		newproj->activecommits = malloc(newproj->maxusers * sizeof(Commit*));
		int i = 0;
		for (i = 0; i < newproj->maxusers; i++) newproj->activecommits[i] = NULL;
		insertNode(newproj);
		return newproj;
	} 

	return (ProjectMeta*)node->content;

}

int entercommit(int uid, char* projectName, char* commitdata, int length) {

	ProjectMeta* project = getMetaData(projectName);

	if (uid >= project->maxusers) { 
		Commit** upsized = realloc(project->activecommits, project->maxusers*2);
		if (!upsized) {
			printf("Error: couldn't malloc\n");
			return -1;
		}
		project->activecommits = upsized;
		int i = project->maxusers;
		for (i = project->maxusers; i < project->maxusers*2; i++) {
			project->activecommits[i] = NULL;
		}
		project->maxusers *= 2; 
	}

	Commit* commit = malloc(sizeof(Commit));
	commit->filecontent = commitdata;
	commit->filesize = length;
	commit->uid = uid;
	commit->entries = 0;
	
	int i = 0;
	for(i = 0; i < length; i++) {
		if (commitdata[i] == '\n') commit->entries += 1;
	}

	project->activecommits[uid] = commit;

	return 0;

}

int clientcommit(NetworkCommand* command, int sockfd) {

	command->type = filenet;
	command->argc = 3;			// Reformat command into a file request, so that it can be proccessed with _filenet function

	if (_filenet(command, sockfd) < 0) {
		printf("Failed to send server Manifest to client\n");
		return -1;
	}
	command->argc = 4;
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

	// printf("***********\nClient '%d' Commit\n%s***********\n", uid, commitfile->argv[1]);

	entercommit(uid, command->argv[0], commitfile->argv[1], commitfile->arglengths[1]);

	// Node* projdata = getHead();
	// printf("#####Active Commits#####\n");
	// while(projdata) {

	// 	ProjectMeta* meta = ((ProjectMeta*)projdata->content);

	// 	printf("%s ->\n", meta->project);
	// 	int i = 0;
	// 	for (i = 0; i < meta->maxusers; i++) {
	// 		if(meta->activecommits[i])
	// 			printf("	%d\n", i);
	// 	}

	// 	projdata = projdata->next;

	// }
	// printf("########################\n");

	// freeCMND(commitfile);

	char* reason = malloc(6);
	memset(reason,'\0', 6);
	sprintf(reason, "%d", uid);
	NetworkCommand* response = newSuccessCMND(name, reason);
	sendNetworkCommand(response, sockfd);
	freeCMND(response);

	return 0;

}

int clientpush(NetworkCommand* command, int sockfd) {

	// arg0 - proj name,  arg1 - uid,  arg2 - commitfilehash
	char* name = malloc(5);
	memset(name, '\0', 5);
	memcpy(name, "push", 5);

	if (!checkcommand(command, 3, name, sockfd, 1)) return -1;
	
	char* clienthash = command->argv[2];
	int clientuid = atoi(command->argv[1]);
	char* project = command->argv[0];

	Node* metaData = getHead();
	while(metaData) {
		if (strcmp(((ProjectMeta*)metaData->content)->project, project)==0) break;
		metaData = metaData->next;
	}

	if (metaData == NULL) {		// project does not have any commits
		// Send error message
		char* reason = malloc(28);
		memset(reason, '\0', 28);
		memcpy(reason, "There are no active commits", 28);
		NetworkCommand* nocommits = newFailureCMND(name, reason);
		sendNetworkCommand(nocommits, sockfd);
		freeCMND(nocommits);
		return -1;
	}

	Commit* commit = ((ProjectMeta*)metaData->content)->activecommits[clientuid];

	if (!commit) {				// client doesn't have any commits
		// Send error message
		char* reason = malloc(44);
		memset(reason, '\0', 44);
		memcpy(reason, "You have no active commits for this project", 44);
		NetworkCommand* nocommits = newFailureCMND(name, reason);
		sendNetworkCommand(nocommits, sockfd);
		freeCMND(nocommits);
		return -1;
	}

	unsigned char* hash = hashdata((unsigned char*)commit->filecontent, commit->filesize);
	char* encoded_hash = hashtohex(hash);

	int fd = open(".Commit", O_RDWR | O_CREAT, S_IRWXU);
	write(fd, commit->filecontent, commit->filesize);

	if (strcmp(clienthash, encoded_hash) != 0) {		// client's commit does not match
		// Send error message
		char* reason = malloc(24);
		memset(reason, '\0', 24);
		memcpy(reason, "Recieved invalid commit", 24);
		NetworkCommand* nocommits = newFailureCMND(name, reason);
		sendNetworkCommand(nocommits, sockfd);
		freeCMND(nocommits);
		return -1;
	}

	// Send success message to client
	char* reason = malloc(4);
	memset(reason, '\0', 4);
	memcpy(reason, "202", 4);
	NetworkCommand* requestfiles = newSuccessCMND(name, reason);
	sendNetworkCommand(requestfiles, sockfd);
	freeCMND(requestfiles);

	NetworkCommand* files = readMessage(sockfd);

	recreatefile("archive.tar.gz", files->argv[1], files->arglengths[1]);
	uncompressfile("archive.tar.gz");

	// Send success message to client
	reason = malloc(4);
	memcpy(reason, "202", 4);
	NetworkCommand* success = newSuccessCMND(name, reason);
	sendNetworkCommand(success, sockfd);
	freeCMND(success);

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
		case pushnet:
			return clientpush(command, sockfd);
			break;
		default:
			return -1;
			break;
	}

}
