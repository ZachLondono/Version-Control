#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include "clientcommands.h"

int main(int argc, char** argv) {

	executeCommand(argc, argv);

	// Example data transfer
	// char buffer[256];

	// int sockfd = connecttohost("localhost", 5555);
	// if (sockfd < 0) return -1; 

	// printf("Enter message: ");
	// bzero(buffer, 256);
	// fgets(buffer, 255, stdin);

	// int n = write(sockfd, buffer, 255);

	// if (n < 0) {
	// 	perror("Error writing\n");
	// 	return 0;
	// }

	// bzero(buffer, 256);
	// n = read(sockfd, buffer, 255);

	// if (n < 0) {
	// 	perror("error reading\n");
	// 	return 0;
	// }

	// printf("%s\n", buffer);
	// return 0;

}

