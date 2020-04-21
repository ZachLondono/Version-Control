#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include "networking.h"
#include "sharedfunctions.h"
#include "servercommands.h"

void* threadentry(void* value) {

	int clisockfd = *(int*)value;

	NetworkCommand* message =  readMessage(clisockfd);

	printf("LOG: Message recieved from client -> %d\n", message->type);
	executecommand(message, clisockfd);
	freeCMND(message);

	return NULL;
}

int main(int argc, char** argv) {

	if (argc != 2) {
		printf("Error: Invalid argument count\n");
		return -1;
	}

	int portlen = strlen(argv[1]);
	if (!isNum(argv[1], portlen) || portlen > 5) {
		printf("Error: Invalid port. Port musht be an integer between 0 an 65535\n");
		return -1;
	}

	int port = atoi(argv[1]);
	
	int serverfd = socket(AF_INET, SOCK_STREAM, 0);
	if (serverfd < 0) {
		printf("Error: couldn't open server socket\n");
		return -1;
	}

	struct sockaddr_in serv_addr;
	bzero((char*)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);

	if (bind(serverfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		printf("Error: couldn't bind to port\n");
		return -1;
	}

	listen(serverfd, 5);

	struct sockaddr_in cli_addr;
	socklen_t clilen = sizeof(cli_addr);

	int clisockfd = 0;
	while (1) {
		
		clisockfd = accept(serverfd, (struct sockaddr*)&cli_addr, &clilen);
		
		if (clisockfd < 0) {
			printf("Error: couldn't open client socket\n");
			continue;
		}

		printf("Successful connection from: %s\n", inet_ntoa((&cli_addr)->sin_addr));

		pthread_t thread;
		pthread_create(&thread, NULL, threadentry, &clisockfd);
		pthread_join(thread, NULL);

	}

	printf("An error occured while accepting new connections: %s\n", strerror(errno));

	// TODO: close all threads

	return 0;

}
