#include "networking.h"

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
	int a = 0;
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
/*
Message* readMessage(int sockfd) {


	Message* message = malloc(sizeof(Message));

	char buffer[5000];

	// Read total 5000 bytes (over max command size)
	int read_in = 0;
	int status = 0;
	int size = 5000;
	while (size != 0 && (status = read(sockfd, &buffer[read_in], size-read_in)) != 0) size -= status;

	// check the message begins with the command type
	if (buffer[0] != CMNDPREFIX || buffer[1] <= 0 || buffer[1] >=14) {
		printf("Error trying to parse an invalid message\n");
		return NULL;
	}

	message->type = buffer[1];
	messagetype t = message->type;
	int argc = 1;
	if (t == configure || t == add || t == remove_cmnd || t == rollback) {
		//these commands are the only ones which have 2 arguments
		message->args = malloc(sizeof(char*) * 2);		
		argc = 2;
	} else message->args = malloc(sizeof(char*) * 1);

	int read_index = 3; // 3 to skip prefix, cmnd type, delim
	int i = 0;
	for(i = 0; i < argc; i++) {
		int arg_len = getArgLen(buffer, read_index);
		int digs = digitCount(arg_len);
		char* arg = malloc(arg_len + 1);
		memset(arg, '\0', arg_len + 1);
		memcpy(arg, &buffer[read_index + digs + 1], arg_len);
		message->args[i] = arg;
		read_index += arg_len + digs + 2;
	}

	return message;

}

int digitCount(int num) {
	int size = 1;
	while ((num /= 10) > 0) ++size;
	return size;
}

int getArgLen(char* buffer, int start_index) {
	int  curr_index = start_index;
	while (buffer[curr_index] != ARGDELIM) ++curr_index;

	char* lenstr = malloc(curr_index - start_index);
	memset(len, '\0', curr_index - start_index);
	memcpy(len, &buffer[start_index], curr_index - start_index);
	int len = atoi(len);
	free(lenstr);
	return len;
}
*/
