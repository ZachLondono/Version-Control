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

	int checkfolder = 0;
	char archivedirectory[10] = ".archive";
    DIR* dir = NULL;
    if ((dir = opendir(archivedirectory))) {
		closedir(dir);
		char backupdirectory[strlen(command->argv[0]) + 10];
		sprintf(backupdirectory, ".archive/%s", command->argv[0]);
		dir = NULL;
		if (!(dir = opendir(backupdirectory))) checkfolder = -1; 
		closedir(dir);
	}

	char* removecmnd = malloc(26 + command->arglengths[0]);
	sprintf(removecmnd, "rm -r -f %s >/dev/null 2>&1", command->argv[0]);
	
	char* removecmnd2 = malloc(26 + command->arglengths[0] + 9);
	sprintf(removecmnd2, "rm -r -f .archive/%s >/dev/null 2>&1", command->argv[0]);

	int failed = 0;

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
		failed = 1;
	}

	if (checkfolder == 0) {
		if (system(removecmnd2) != 0) {
			pthread_mutex_unlock(&repo_lock);
			char* reason = malloc(25);
			memset(reason, '\0',25);
			memcpy(reason, "couldn't destroy archive", 25);
			NetworkCommand* response = newFailureCMND_B(name, reason, 25);
			sendNetworkCommand(response, sockfd);
			freeCMND(response);
			free(removecmnd);
			failed = 1;
		}
	}

	Node* head = getHead();
	while(head != NULL) {
		if (strcmp(((ProjectMeta*) head->content)->project, command->argv[0]) == 0) break;
		head = head->next;
	}

	if (head != NULL) {
		ProjectMeta* projmeta = (ProjectMeta*) head->content;
		int i = 0;
		for (i = 0; i < projmeta->maxusers; i++) {
			if (projmeta->activecommits[i]) {
				printf("Expiring commit for user '%d' in '%s'\n", i, command->argv[0]);
				free(projmeta->activecommits[i]);
				projmeta->activecommits[i] = NULL;
			}
		}
	}

	pthread_mutex_unlock(&repo_lock);
	free(removecmnd);
	if (failed) return -1;

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
	memset(name, '\0', 9);
	memcpy(name, "rollback", 9);

	if (!checkcommand(command, 2, name, sockfd, 1)) return -1;

	int newversion = atoi(command->argv[1]);

	char archivepath[command->arglengths[0] + 10];
	memset(archivepath, '\0', command->arglengths[0] + 10);
	sprintf(archivepath, ".archive/%s", command->argv[0]);

	DIR* archvedir = opendir(archivepath);
	if (archvedir == NULL) {
		printf("Error: Client requested a version of project '%s' that does not exist\n", command->argv[0]);
		char* reason = malloc(50);
		memset(reason, '\0', 50);
		memcpy(reason, "Requested version for project does not exist", 50);
		
		NetworkCommand* response = newFailureCMND_B(name, reason, 50);
		sendNetworkCommand(response, sockfd);
		free(response);
		return -1;
	}

	char* directoriestoremove[100];
	int dircount = 0;
	char* rollbacktar = NULL;

	struct dirent* entry;
	while((entry = readdir(archvedir))) {
		printf("%s\n", entry->d_name);	
		if (entry->d_type == DT_REG) {
			if (strcmp(entry->d_name, "History") == 0 || strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
			char* name = malloc(strlen(entry->d_name));
			memset(name, '\0', strlen(entry->d_name));
			memcpy(name, entry->d_name, strlen(entry->d_name));
			char* version = strtok(name, "-");
			if (!isNum(version, strlen(version))) continue;
			if (atoi(version) > newversion) {
				directoriestoremove[dircount] = malloc(strlen(entry->d_name) + 1);
				memset(directoriestoremove[dircount], '\0',strlen(entry->d_name) + 1);
				memcpy(directoriestoremove[dircount], entry->d_name,strlen(entry->d_name) + 1);
				++dircount;
			} else if (atoi(version) == newversion) {
				printf("**%s\n", entry->d_name);
				rollbacktar = malloc(strlen(entry->d_name) + 1);
				memset(rollbacktar, '\0', strlen(entry->d_name));
				memcpy(rollbacktar, entry->d_name, strlen(entry->d_name));
			}
			free(name);
		}
	}

	if (!rollbacktar) {
		printf("Error: Client requested a version of project '%s' that does not exist\n", command->argv[0]);
		char* reason = malloc(50);
		memset(reason, '\0', 50);
		memcpy(reason, "Requested version for project does not exist", 50);
		
		NetworkCommand* response = newFailureCMND_B(name, reason, 50);
		sendNetworkCommand(response, sockfd);
		free(response);
		return -1;
	}	

	char cmnd[strlen(command->argv[0]) + 9];
	memset(cmnd, '\0', strlen(command->argv[0]) + 9);
	// memcpy(olddir, command->argv[0], strlen(command->argv[0]) + 1);
	sprintf(cmnd, "rm -rf %s", command->argv[0]);
	
	if (system(cmnd) < 0) {
		printf("Error: There was an error removing old files in project %s, skipping... %s\n", command->argv[0], strerror(errno));
	}

	char* fullpath = malloc(strlen(archivepath) + strlen(rollbacktar) + 2);
	memset(fullpath, '\0', strlen(archivepath) + strlen(rollbacktar) + 2);
	sprintf(fullpath, "%s/%s", archivepath, rollbacktar);
	rename(fullpath, "rollback");

	uncompressfile("rollback");
	char* reason = malloc(4);
	memset(reason, '\0', 4);
	memcpy(reason, "202", 4);

	int i = 0;
	for (i = 0; i < dircount; i++) {
		char* full = malloc(strlen(archivepath) + strlen(directoriestoremove[i]) + 2);
		memset(full, '\0', strlen(archivepath) + strlen(directoriestoremove[i]) + 2);
		sprintf(full, "%s/%s", archivepath, directoriestoremove[i]);
		remove(full);
	}
	

	char historypath[strlen(archivepath) + 9];
	sprintf(historypath, "%s/History", archivepath);

	int fd = open(historypath, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd > 0) {
		char entry[18 + digitCount(newversion)];
		sprintf(entry, "rollback to ver %d\n", newversion);
		write(fd, entry, 17 + digitCount(newversion));
	} else printf("Error: couldn't update history, skipping... %s\n", strerror(errno));
	

	NetworkCommand* response = newSuccessCMND_B(name, reason, 4);
	sendNetworkCommand(response, sockfd);
	free(response);

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

	int pathlen = strlen(command->argv[0]) + 11;
	char manifestpath[pathlen];
	sprintf(manifestpath, "%s/.Manifest", command->argv[0]);

	FileContents* manifestcontent = readfile(manifestpath);
	Manifest* manifest = parseManifest(manifestcontent);

	char** filepats = getManifestFiles(manifest);
	int* versions = getManifestFileVersion(manifest);

	char* reason = malloc(manifestcontent->size);	
	memset(reason, '\0', manifestcontent->size);
	freefile(manifestcontent);


	char ver[digitCount(manifest->version) + 2];
	sprintf(ver, "%d\n", manifest->version);
	strcat(reason, ver);

	int i = 0;
	for (i = 0; i < manifest->entrycount; i++) {
		char entry[digitCount(versions[i]) + 1 + strlen(filepats[i]) + 2];
		sprintf(entry, "%d %s\n", versions[i], filepats[i]);
		strcat(reason, entry);
	}

	NetworkCommand* response = newSuccessCMND(name, reason);
	sendNetworkCommand(response, sockfd);
	freeCMND(response);
	return 0;
	
}

int clienthistory(char* project, int sockfd) {

	char* name = malloc(10);
	memset(name, '\0', 10);
	memcpy(name, "history", 7);
	

	if (!checkForLocalProj(project)) {
		printf("Error: client requested project which doesn't exist or cant be opened\n");
		char* reason = malloc(23);
		memset(reason, '\0', 23);
		memcpy(reason, "Project does not exist", 23);
		
		NetworkCommand* response = newFailureCMND_B(name, reason, 23);
		sendNetworkCommand(response, sockfd);
		free(response);
		return -1;
	}

	char filepath[strlen(project) + 19];
	memset(filepath, '\0', 19);
	sprintf(filepath, ".archive/%s/History", project);

	FileContents* history = readfile(filepath);
	if (!history) {
		char* reason = malloc(39);
		memset(reason, '\0', 39);
		memcpy(reason, "Projetc '%s' has had no files commited", 39);
		
		NetworkCommand* response = newFailureCMND_B(name, reason, 39);
		sendNetworkCommand(response, sockfd);
		free(response);
		return -1;
	}

	NetworkCommand* response = newSuccessCMND_B(name, history->content, history->size);

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

	if(filecount == 1 && strcmp(command->argv[2], "History") == 0) {
		return clienthistory(command->argv[0], sockfd);
	}
	
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
		if (i != 0) strcat(allpaths, " ");
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
	commit->filecontent = malloc(length + 1);
	memset(commit->filecontent, '\0', length + 1);
	memcpy(commit->filecontent, commitdata, length);
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

	freeCMND(commitfile);		//IF THIS LINE IS UNCOMMENTED, COMMIT IF INVALID

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

	name = malloc(5);
	memset(name, '\0', 5);
	memcpy(name, "push", 5);
	NetworkCommand* files = readMessage(sockfd);

	pthread_mutex_lock(&repo_lock);

	char manifestpath[strlen(project) + 11];
	memset(manifestpath, '\0', strlen(project) + 11);
	sprintf(manifestpath, "%s/.Manifest", project);
	FileContents* manifestFile = readfile(manifestpath);
	int version = getManifestVersion(manifestFile);


	char archivedirectory[10] = ".archive";
	int checkfolder = 0;
    DIR* dir = NULL;
    if (!(dir = opendir(archivedirectory))) checkfolder = mkdir(archivedirectory, 0700);
    else closedir(dir);

	char backupdirectory[strlen(project) + 10];
	sprintf(backupdirectory, ".archive/%s", project);
	int checkfolder2 = 0;
    dir = NULL;
    if (!(dir = opendir(backupdirectory))) checkfolder2 = mkdir(backupdirectory, 0700);
    else closedir(dir);


	if (checkfolder == 0 && checkfolder2 == 0) {
		int archivenamelen = digitCount(version) + 2*strlen(project) + 12;
		char* archivename = malloc(archivenamelen);
		sprintf(archivename, "%s/%d-%s", backupdirectory, version, project);
		createcompressedfile(project, strlen(project), archivename, archivenamelen);

		char historypath[strlen(project) + 7];
		sprintf(historypath, ".archive/%s/History", project);
		int fd = open(historypath, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
		if (fd > 0) {
			write(fd, commit->filecontent, commit->filesize);
		} else printf("Error: couldn't update history, skipping... %s\n", strerror(errno));
		
	} else printf("Error: couldn't backup current version, skipping...\n");

	// Copy all clients files into server repo
	recreatefile("archive.tar.gz", files->argv[1], files->arglengths[1]);
	uncompressfile("archive.tar.gz");

	// Delete all files deleted on client side
	char** paths = getCommitFilePaths(commit);
	char** hashes = getCommitHashes(commit);
	ModTag* tags = getModificationTags(commit);
	int* versions = getCommitVersions(commit);

	int i = 0;
	int addedcount = 0;
	char* added[commit->entries];
	// int removedcount = 0;
	// char* removed[commit->entries];
	int addedlen = 0; 
	for (i = 0; i < commit->entries; i++) {
		if (tags[i] == Add || tags[i] == Modify) {
			int len = strlen(paths[i]) + digitCount(versions[i]) + 43;
			addedlen += len;
			added[addedcount] = malloc(len + 1);
			sprintf(added[addedcount], "%d %s %s\n", versions[i], paths[i], hashes[i]);
			++addedcount;
		}
	}

	// create a char* with enough space for all current entries and new entries - deleted entries
	char newmanifest[manifestFile->size + addedlen];
	memset(newmanifest, '\0', manifestFile->size + addedlen);

	char verstr[digitCount(version + 1) + 2];
	sprintf(verstr, "%d\n", version + 1);
	strcat(newmanifest, verstr);

	for (i = 0; i < addedcount; i++) {
		strcat(newmanifest, added[i]);
	}

	char tempmanifestpath[strlen(project) + 16];
	memset(tempmanifestpath, '\0', strlen(project) + 16);
	sprintf(tempmanifestpath, "%s/.temp.Manifest", project);
	int fd = open(tempmanifestpath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	write(fd, newmanifest, addedlen + strlen(verstr));

	remove(manifestpath);

	rename(tempmanifestpath, manifestpath);

	pthread_mutex_unlock(&repo_lock);

	// Send success message to client
	reason = malloc(4);
	memcpy(reason, "202", 4);
	NetworkCommand* success = newSuccessCMND(name, reason);
	sendNetworkCommand(success, sockfd);
	freeCMND(success);

	Node* head = getHead();
	while(head) {
		if (strcmp(((ProjectMeta*) head->content)->project, project) == 0) break;
		head = head->next;
	}

	if (head == NULL) return 0;

	ProjectMeta* projmeta = (ProjectMeta*) head->content;
	i = 0;
	for (i = 0; i < projmeta->maxusers; i++) {
		if (projmeta->activecommits[i]) {
			printf("Expiring commit for user '%d' in '%s'\n", i, project);
			free(projmeta->activecommits[i]);
			projmeta->activecommits[i] = NULL;
		}
	}

	return 0;

}

int clientupgrade(NetworkCommand* command, int sockfd) {

	if (!checkcommand(command, 1, "upgrade", sockfd, 1)) return -1;

	char* manpath = malloc(command->arglengths[0] + 11);
	sprintf(manpath, "%s/.Manifest", command->argv[0]);

	FileContents* file = readfile(manpath);
	int version = getManifestVersion(file);
	freefile(file);
	free(manpath);

	char* ver_s = malloc(digitCount(version) + 1);
	sprintf(ver_s, "%d", version);
	NetworkCommand* response = newSuccessCMND("upgrade", ver_s);
	sendNetworkCommand(response, sockfd);

	NetworkCommand* filereq = readMessage(sockfd);
	filereq->type = filenet;
	if (_filenet(filereq, sockfd) < 0) {
		printf("Failed to send server files to client\n");
		return -1;
	}

	return 0;
}

int clientupdate(NetworkCommand* command, int sockfd) {
	command->type = filenet;
	if (_filenet(command, sockfd) < 0) {
		printf("Failed to send server Manifest to client\n");
		return -1;
	}
	return -1;
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
		case updatenet:
			return clientupdate(command, sockfd);
			break;
		case upgradenet:
			return clientupgrade(command, sockfd);
			break;
		default:
			return -1;
			break;
	}

}
