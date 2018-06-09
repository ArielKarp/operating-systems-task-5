/*
 * pcc_server.c
 *
 *  Created on: Jun 7, 2018
 *      Author: ariel
 */


#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <limits.h>
#include <pthread.h>

typedef struct {
	int connfd;
} threadInfo;


#define LOWER_BOUND				32
#define UPPER_BOUND				126
#define NUMBER_OF_LISTENERS		100
#define SIZE_OF_ARR				((UPPER_BOUND - LOWER_BOUND) + 1)

// global variables
int pcc_count[SIZE_OF_ARR];
pthread_mutex_t mutex;
int nThreads;
int doLoop = 1;
int isShuttingDown = 0;
pthread_t* threads = NULL;


int handle_error_exit(const char* error_msg) {
	int errsv = errno;
	printf("Error message: [%s] | ERRNO: [%s]\n", error_msg, strerror(errsv));
	if (errsv == 0) {
		// in case error did not change errno
		exit(EXIT_FAILURE);
	}
	exit(errsv);
}

unsigned int convertCharArrayToUsingedInt(char* input_arr, int size_of_arr, int* success) {
	for(int i = 0; i < size_of_arr; ++i) {
		if(!isdigit(input_arr[i])) {
			*success = 0;
			return 0;
		}
	}
	*success = 1;
	return (strtoul(input_arr, NULL, 10));
}

void* pcc_client_thread(void* arg) {
	int connfd = ((threadInfo*) arg)->connfd;

	// read len from client
	// read message from client
	// count number of valid instances
	// update array
	// exit gracefully
}

int main(int argc, char* argv[]) {
	// get input from user
	if (argc != 2) {
		handle_error_exit("Wrong number of input arguments");
	}

	int sockfd		= -1;
	int rc 			= 0;
	int listenfd 	= -1;
	int connfd		= -1;
	struct sockaddr_in serv_addr;
	socklen_t addrsize = sizeof(struct sockaddr_in);
	pthread_attr_t attr;

	int16_t server_port = convertCharArrayToUsingedInt(argv[2], strlen(argv[2]), &rc);
	if (!rc) {
		handle_error_exit("Wrong port number");
	}

	if ((listenfd = socket( AF_INET, SOCK_STREAM, 0)) < 0) {
		handle_error_exit("Could not create socket to listen");
	}

	// get serv_addr ready
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(server_port); // Note: htons for endiannes
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// bind to socket
	if( bind(listenfd, (struct sockaddr*) &serv_addr, addrsize) != 0)
	{
		handle_error_exit("Failed to bind server's socket");
	}

	// start listening
	if ( listen(listenfd, NUMBER_OF_LISTENERS) != 0) {
		handle_error_exit("Failed to starting listening");
	}

	// init mutex and attr
	if (pthread_mutex_init(&mutex, NULL) != 0) {
		handle_error_exit("Failed to init mutex");
	}
	if (pthread_attr_init(&attr) != 0) {
		handle_error_exit("Failed to init attr");
	}
	if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) != 0) {
		handle_error_exit("Failed to set detach state");
	}

	while (doLoop) {
		connfd = accept(listenfd, NULL, NULL);
		if (connfd < 0) {
			handle_error_exit("Failed to accept connection");
		}
		if (isShuttingDown) {  // shutting down, finish looping..
			doLoop = 0;
			break;
		}
		// create a new thread
		// realloc safely
		pthread_t new_threads = realloc(threads, (++nThreads)*sizeof(pthread_t));
		if (new_threads == NULL) {
			handle_error_exit("Failed to realloc");
		}
		threads = new_threads;
		threadInfo* pcc_info = malloc(sizeof(threadInfo));
		if (pcc_info == NULL) {
			handle_error_exit("Failed to allocate threadInfo struct");
		}
		pcc_info->connfd = connfd;
		if (pthread_create(&threads[nThreads-1], &attr, pcc_client_thread, (void*) pcc_info) != 0) {
			handle_error_exit("Failed creating a thread");
		}
	}

	// TODO: sould not get here
	return EXIT_SUCCESS;
}
