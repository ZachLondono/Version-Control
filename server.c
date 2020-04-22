#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include <signal.h>
#include "networking.h"
#include "sharedfunctions.h"
#include "servercommands.h"
#include "queue.h"

#define THREAD_POOL_SIZE 10
pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t new_command = PTHREAD_COND_INITIALIZER;
pthread_t thread_pool[THREAD_POOL_SIZE];
int serveractive = 1;

void* executecommandthread(int* value) {

	int clisockfd = *(value);
    free(value);

	NetworkCommand* message =  readMessage(clisockfd);
	printf("LOG: Message recieved from client -> %d\n", message->type);
	executecommand(message, clisockfd);
	freeCMND(message);

	return NULL;

}

void* threadentry(void* value) {
	while (serveractive) {
        pthread_mutex_lock(&queue_lock);
        int* node = dequeue();
        if (node == NULL) {
            pthread_cond_wait(&new_command, &queue_lock);
            node = dequeue();
        }
        pthread_mutex_unlock(&queue_lock);
		if (node == NULL) continue;
		executecommandthread(node);
	}
    printf("exiting thread\n");
    return NULL;
}

void inturupthandler() {
    serveractive = 0;
    int i = 0;
    for ( i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_cond_signal(&new_command);
    }
    for ( i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_join(thread_pool[i], NULL);
    }
    exit(0);
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

	int i = 0;
	for(i = 0; i < THREAD_POOL_SIZE; i++) {
		pthread_create(&thread_pool[i], NULL, threadentry, NULL);
	}
    if(signal(SIGINT, inturupthandler) == SIG_ERR) {
        printf("Error: couldn't set handler for inturupt signal,\n");
        return -1;
    }

	int clisockfd = 0;
	while (1) {
		
		clisockfd = accept(serverfd, (struct sockaddr*)&cli_addr, &clilen);
		
		if (clisockfd < 0) {
			printf("Error: couldn't open client socket\n");
			continue;
		}

		printf("Successful connection from: %s\n", inet_ntoa((&cli_addr)->sin_addr));

		int* fd = malloc(sizeof(int));
		*fd = clisockfd;
        pthread_mutex_lock(&queue_lock);
        pthread_cond_signal(&new_command);
		enqueue(fd);	
        pthread_mutex_unlock(&queue_lock);

	}

	printf("An error occured while accepting new connections: %s\n", strerror(errno));

	return 0;

}
