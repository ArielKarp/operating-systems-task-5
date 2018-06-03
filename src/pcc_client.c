/*
 * pcc_client.c
 *
 *  Created on: Jun 3, 2018
 *      Author: ariel
 */


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


int handle_error_exit(const char* error_msg) {
	int errsv = errno;
	printf("Error message: [%s] | ERRNO: [%s]\n", error_msg, strerror(errsv));
	if (errsv == 0) {
		// in case error did not change errno
		exit(EXIT_FAILURE);
	}
	exit(errsv);
}

int isIPAddr(const char* input_str) {
	struct sockaddr_in sa;
	memset(&sa, 0, sizeof(sa));
	int rc = inet_pton(AF_INET, input_str, &(sa.sin_addr));
	return rc != 0;
}

int main(int argc, char* argv[]) {
	// get input from user
	if (argc != 4) {
		handle_error_exit("Wrong number of input arguments");
	}

	int sockfd = -1;

	struct sockaddr_in serv_addr; // where we want to get to
	struct sockaddr_in my_addr;   // where we actually connected through
	struct sockaddr_in peer_addr; // where we actually connected to

	char* server_host = argv[1];
	int16_t server_port = atoi(argv[2]);
	unsigned int len_to_read = atoi(argv[3]);

	// get serv_addr ready
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(server_port); // Note: htons for endiannes

	// check if server host is a valid IP
	if (isIPAddr(server_host)) {
		inet_pton(AF_INET, input_str, &(serv_addr.sin_addr));
	} else {
		// this is a host name
		// make this pretty
		struct hostent *he;
		if ( (he = gethostname(server_host) ) == NULL ) {
			handle_error_exit("Hostname is invalid");
		}
		// copy address to server's struct
		memcpy(&serv_addr.sin_addr, he->h_addr_list[0], he->h_length);

	}

	// create client socket
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		handle_error_exit("Could not create socket");
	}

	// connet to server
	if( connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
	{
		handle_error_exit("Failed to connect to server");
	}

	return EXIT_SUCCESS;
}
