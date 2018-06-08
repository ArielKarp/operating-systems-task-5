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

int handle_error_exit(const char* error_msg) {
	int errsv = errno;
	printf("Error message: [%s] | ERRNO: [%s]\n", error_msg, strerror(errsv));
	if (errsv == 0) {
		// in case error did not change errno
		exit(EXIT_FAILURE);
	}
	exit(errsv);
}

int main(int argc, char* argv[]) {
	// get input from user
	if (argc != 2) {
		handle_error_exit("Wrong number of input arguments");
	}

	int sockfd = -1;
	int rc = 0;
	struct sockaddr_in serv_addr; // where we want to get to

	char* server_host = argv[1];
	int16_t server_port = convertCharArrayToUsingedInt(argv[2], strlen(argv[2]), &rc);
	if (!rc) {
		handle_error_exit("Wrong port number");
	}
	rc = 0;
	unsigned int len_to_read = convertCharArrayToUsingedInt(argv[3], strlen(argv[3]), &rc);
	if (!rc) {
		handle_error_exit("Wrong numer of bytes to read");
	}
}
