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

	int clisockfd = accept(serverfd, (struct sockaddr*)&cli_addr, &clilen);

	if (clisockfd < 0) {
		printf("Error: couldn't open client socket\n");
		return -1;
	}

	printf("Successful connection from: %s\n", inet_ntoa((&cli_addr)->sin_addr));

	NetworkCommand* message =  readMessage(clisockfd);
	
	printf("Message recieved from client\n");
	message->operation(message, clisockfd);
	printf("Recieved message from server\n");
    printf("argv[0] %s\n", message->argv[0]);


	// int serverfd = socket(AF_INET, SOCK_STREAM,0);

	// if (serverfd < 0) {
	// 	printf("Error: server failed to open socket\n");
	// 	return -1;
	// }

	// if (bindtoport(serverfd, port) < 0) {
	// 	printf("Error: failed to bind socket to port\n");
	// 	return -1;
	// }
	
	// if (listen(serverfd, 5) < 0) {
	// 	printf("Error: unable to listen on port\n%s\n", strerror(errno));
	// }


	// struct sockaddr_in cli_addr;
	// int clilen = sizeof(cli_addr);
	// int newsockfd = accept(serverfd, (struct sockaddr *)&cli_addr, &clilen);
	// if (newsockfd < 0) {
	// 	printf("Error: couldn't accept client connection\n%s\n", strerror(errno));
	// 	return -1;
	// } else {
	// 	printf("Client successfully connected: \n");
	// 	printf(": %s\n", inet_ntoa((&cli_addr)->sin_addr));
	// }

	// char buffer[256];
	// bzero(buffer, 256);
	
	// int n = read(newsockfd, buffer, 255);

	// if (n < 0) {
	// 	perror("error reading");
	// 	return 0;
	// }

	// printf("Message from client: %s\n", buffer);

	// n = write(newsockfd, "message recieved", 18);

	// if (n < 0) {
	// 	perror("Error writing");
	// 	return 0;
	// }

	return 0;

}

// int executeServerCommand(NetworkCommand command) {

// }