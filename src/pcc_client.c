/*
 * pcc_client.c
 *
 *  Created on: Jun 3, 2018
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


#define MESSAGE_BLOCK 1024


int handle_error_exit(const char* error_msg) {
	int errsv = errno;
	printf("Error message: [%s] | ERRNO: [%s]\n", error_msg, strerror(errsv));
	if (errsv == 0) {
		// in case error did not change errno
		exit(EXIT_FAILURE);
	}
	exit(errsv);
}

int my_ceil(float num) {
	int integet_num = (int) num;
	if (num == (float) integet_num) {
		return integet_num;
	}
	return integet_num + 1;
}


int isIPAddr(const char* input_str) {
	struct sockaddr_in sa;
	memset(&sa, 0, sizeof(sa));
	int rc = inet_pton(AF_INET, input_str, &(sa.sin_addr));
	return rc != 0;
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

void fromHostNameToIPAddr(char* hostname, struct sockaddr_in* out_addr) {
	struct addrinfo hints;
	struct addrinfo *servinfo;
	struct addrinfo *pointer;
	struct sockaddr_in *h;
	int rc;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if ( (rc = getaddrinfo(hostname, NULL, &hints, &servinfo)) != 0)
	{
		handle_error_exit("Failed to getaddrinfo");
	}

	// loop through all the results and connect to the first we can
	for(pointer = servinfo; pointer != NULL; pointer = pointer->ai_next)
	{
		// get address
		h = (struct sockaddr_in *) pointer->ai_addr;
		// copy to out_addr
		out_addr->sin_addr = h->sin_addr;
	}
	// free addrinfo
	freeaddrinfo(servinfo);
}

int main(int argc, char* argv[]) {
	// get input from user
	if (argc != 4) {
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

	// get serv_addr ready
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(server_port); // Note: htons for endiannes

	// check if server host is a valid IP
	if (isIPAddr(server_host)) {
		if (inet_pton(AF_INET, server_host, &(serv_addr.sin_addr)) != 1) { // TODO: check error
			handle_error_exit("Error in inet_pton");
		}
	} else {
		// this is a host name
		fromHostNameToIPAddr(server_host, &serv_addr);
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

	// open /dev/urandom
	int urand_fd = open("/dev/urandom", O_RDONLY);
	if (urand_fd < 0) {
		handle_error_exit("Failed opening /dev/urandom for read");
	}

	// read len_of_read bytes from urandom
	char* read_buf = calloc(MESSAGE_BLOCK, sizeof(char));

	if (read_buf == NULL) {
		handle_error_exit("Failed to allocate memory for read buffer");
	}

	// writing to server

	// sending size of message first
	int32_t out_val = htonl(len_to_read);
	char* out_stream = (char*)&out_val;
	int size_of_val = sizeof(out_val);
	int wrote_bytes = 0;
	int bytes_wrote = 0;
	while (wrote_bytes < size_of_val) {
		bytes_wrote = write(sockfd, out_stream + wrote_bytes, size_of_val - wrote_bytes);
		if (bytes_wrote == -1) {
			handle_error_exit("Failed to write to server");
		}
		wrote_bytes += bytes_wrote;
	}

	// send message
	int bytes_read = 0;
	unsigned int left_to_read = len_to_read;
	// get number of iterations
	int num_of_iter = my_ceil( ((float)len_to_read) / MESSAGE_BLOCK);
	for (int i = 0; i < num_of_iter; ++i) {
		// read a block from urandom
		if (left_to_read > MESSAGE_BLOCK) {
			bytes_read = read(urand_fd, read_buf, MESSAGE_BLOCK);
			left_to_read -= MESSAGE_BLOCK;
		} else {
			bytes_read = read(urand_fd, read_buf, left_to_read);  // last read
		}
		if (bytes_read == -1) {
			handle_error_exit("Failed reading from urandom");
		}
		read_buf[bytes_read] = '\0';

		wrote_bytes = 0;
		while (wrote_bytes < bytes_read) {
			// reading MESSAGE_BLOCK from urandom
			bytes_wrote = write(sockfd, read_buf + wrote_bytes, bytes_read - wrote_bytes);
			if (bytes_wrote == -1) {
				handle_error_exit("Failed to write to server");
			}
			wrote_bytes += bytes_wrote;
		}
		// clear buffer
		memset(read_buf, 0, MESSAGE_BLOCK * sizeof(char));
	}

	// read from server

	int32_t in_val;
	char* in_stream = (char*)&in_val;
	size_of_val = sizeof(in_val);
	int read_bytes = 0;
	int done_read = 0;
	while (read_bytes < size_of_val) {
		done_read = read(sockfd, in_stream + read_bytes, size_of_val - read_bytes);
		if (done_read == -1) {
			handle_error_exit("Failed to read from server");
		}
		read_bytes += done_read;
	}

	// convert input to unsigned int
	unsigned int C = ntohl(in_val);

	printf("# of printable characters: %u\n", C);


	free(read_buf);
	close(sockfd);
	return EXIT_SUCCESS;
}
