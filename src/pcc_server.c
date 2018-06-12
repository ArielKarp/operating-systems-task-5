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
#include <signal.h>
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
unsigned int pcc_count[SIZE_OF_ARR];
pthread_mutex_t pcc_mutex;
int nThreads = 0;
int isShuttingDown = 0;
pthread_t* threads = NULL;
pthread_attr_t attr;
int listenfd 	= -1;


int handle_error_exit(const char* error_msg) {
	int errsv = errno;
	printf("Error message: [%s] | ERRNO: [%s]\n", error_msg, strerror(errsv));
	if (errsv == 0) {
		// in case error did not change errno
		exit(EXIT_FAILURE);
	}
	exit(errsv);
}

unsigned int convertCharArrayToUsingedInt(char* input_arr, unsigned int size_of_arr, int* success) {
	for(unsigned int i = 0; i < size_of_arr; ++i) {
		if(!isdigit(input_arr[i])) {
			*success = 0;
			return 0;
		}
	}
	*success = 1;
	return (strtoul(input_arr, NULL, 10));
}

unsigned int countArrIns(char* in_stream, unsigned int size_of_arr, unsigned int* out_arr) {
	// count and returns the number of printable chars of in_stream
	// update out_arr
	char curr_char;
	unsigned int count = 0;
	for (unsigned int i = 0; i < size_of_arr; ++i) {
		curr_char = in_stream[i];
		if (curr_char >= LOWER_BOUND && curr_char <= UPPER_BOUND) {
			// printable char
			count++;
			out_arr[curr_char - LOWER_BOUND]++;
		}
	}
	return count;
}

void updatePccCount(unsigned int* in_arr) {
	// in_arr and pcc_count have the same size
	for (int i = 0; i < SIZE_OF_ARR; ++i) {
		pcc_count[i] = pcc_count[i] +  in_arr[i];
	}
}

void* pcc_client_thread(void* arg) {
	int connfd = ((threadInfo*) arg)->connfd;

	// read len from client
	uint32_t in_val;
	char* in_stream = (char*)&in_val;
	int size_of_val = sizeof(in_val);
	int read_bytes = 0;
	int done_read = 0;
	while (read_bytes < size_of_val) {
		done_read = read(connfd, in_stream + read_bytes, size_of_val - read_bytes);
		if (done_read == -1) {
			handle_error_exit("Failed to read from client");
		}
		read_bytes += done_read;
	}

	// set message to read
	unsigned int size_to_read = ntohl(in_val);
	char* message_to_read = malloc(size_to_read * sizeof(char));
	if (message_to_read == NULL) {
		handle_error_exit("Failed to allocate memory");
	}

	// read message from client
	read_bytes = 0;
	done_read = 0;
	while (read_bytes < size_to_read) {
		done_read = read(connfd, message_to_read + read_bytes, size_to_read - read_bytes);
		if (done_read == -1) {
			handle_error_exit("Failed to read from client");
		}
		read_bytes += done_read;
	}

	// count number of valid instances
	unsigned int* local_pcc = calloc(SIZE_OF_ARR, sizeof(unsigned int));
	if (local_pcc == NULL) {
		handle_error_exit("Failed to allocate memory");
	}

	unsigned int r_value = countArrIns(message_to_read, size_to_read, local_pcc);

	// return value to client
	uint32_t out_val = htonl(r_value);
	char* out_stream = (char*)&out_val;
	size_of_val = sizeof(out_val);
	int wrote_bytes = 0;
	int bytes_wrote = 0;
	while (wrote_bytes < size_of_val) {
		bytes_wrote = write(connfd, out_stream + wrote_bytes, size_of_val - wrote_bytes);
		if (bytes_wrote == -1) {
			handle_error_exit("Failed to write to client");
		}
		wrote_bytes += bytes_wrote;
	}

	// lock mutex
	if (pthread_mutex_lock(&pcc_mutex) != 0) {
		// lock mutex
		handle_error_exit("Failed to lock the mutex");
	}
	// mutex is locked
	// update array
	updatePccCount(local_pcc);

	// unlock mutex
	if (pthread_mutex_unlock(&pcc_mutex) != 0) {
		handle_error_exit("Failed to unlock the mutex");
	}

	// exit gracefully
	close(connfd);
	free(local_pcc);
	free(message_to_read);
	free((threadInfo*) arg);
	pthread_exit(NULL);
}

void prettyPrintPccArr() {
	printf("\n");  // flush stdout
	for (int i = 0; i < SIZE_OF_ARR; ++i) {
		printf("char '%c' : %u times\n", i + LOWER_BOUND, pcc_count[i]);
	}
}

void handle_resources() {
	// stop accepting connections
	isShuttingDown = 1;
	close(listenfd);

	// join all threads
	for (int i = 0; i < nThreads; ++i) {
		if (pthread_join(threads[i], NULL) != 0) {
			handle_error_exit("Failed to join a thread");
		}
	}

	// print pcc counters
	prettyPrintPccArr();

	// exit gracefully
	if (pthread_attr_destroy(&attr) != 0) {
		handle_error_exit("Failed to destroy attr");
	}
	if (pthread_mutex_destroy(&pcc_mutex) != 0) {
		handle_error_exit("Failed to destroy mutex");
	}

	free(threads);
}

void signal_int_handler(int signum) {
	handle_resources();
	exit(EXIT_SUCCESS);
}

int register_signal_int_handling() {
	struct sigaction new_int_action;
	memset(&new_int_action, 0, sizeof(new_int_action));
	new_int_action.sa_handler = signal_int_handler;
	return sigaction(SIGINT, &new_int_action, NULL);
}

int main(int argc, char* argv[]) {
	// get input from user
	if (argc != 2) {
		handle_error_exit("Wrong number of input arguments");
	}

	if (register_signal_int_handling() != 0) {
		handle_error_exit("Failed to register sigint handler");
	}

	int rc 			= 0;
	int connfd		= -1;
	struct sockaddr_in serv_addr;
	socklen_t addrsize = sizeof(struct sockaddr_in);

	// clear pcc_count
	memset(pcc_count, 0, SIZE_OF_ARR * sizeof(unsigned int));

	uint16_t server_port = convertCharArrayToUsingedInt(argv[1], strlen(argv[1]), &rc);
	if (!rc) {
		handle_error_exit("Wrong port number");
	}

	if ((listenfd = socket( AF_INET, SOCK_STREAM, 0)) < 0) {
		handle_error_exit("Could not create socket to listen");
	}


	// allow reuse of socket
	int reuse = 1;
	if ( setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0) {
		handle_error_exit("Failed to set socket reuseadd");
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
	if (listen(listenfd, NUMBER_OF_LISTENERS) != 0) {
		handle_error_exit("Failed to starting listening");
	}

	// init mutex and attr
	if (pthread_mutex_init(&pcc_mutex, NULL) != 0) {
		handle_error_exit("Failed to init mutex");
	}
	if (pthread_attr_init(&attr) != 0) {
		handle_error_exit("Failed to init attr");
	}
	if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) != 0) {
		handle_error_exit("Failed to set detach state");
	}
	while (1) {
		connfd = accept(listenfd, NULL, NULL);
		if (connfd < 0) {
			handle_error_exit("Failed to accept connection");
		}

		if (!isShuttingDown) {  // not shutting down
			// create a new thread
			// realloc safely
			pthread_t* new_threads = realloc(threads, (++nThreads)*sizeof(pthread_t));
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
		} else {
			close(connfd);
		}
	}

	handle_resources();
	return EXIT_SUCCESS;
}
