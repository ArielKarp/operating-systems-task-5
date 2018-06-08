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
		if (inet_pton(AF_INET, server_host, &(serv_addr.sin_addr)) != 1) { //; // TODO: check error
			handle_error_exit("Error in inet_pton");
		}
	} else {
		// this is a host name
		// make this pretty
		//struct hostent *he;
		//char host_name[HOST_NAME_MAX];
//		if ( gethostname(&serv_addr.sin_addr, HOST_NAME_MAX) == -1 ) {
//			handle_error_exit("Hostname is invalid");
//		}
		// copy address to server's struct
		//memcpy(&serv_addr.sin_addr, he->h_addr_list[0], he->h_length);
		struct hostent *he;
		if ( (he = gethostbyname(server_host)) == NULL) {
			handle_error_exit("Hostname is invalid");
		}
		//copy address to server's struct
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

	// open /dev/urandom
	int urand_fd = open("/dev/urandom", O_RDONLY);
	if (urand_fd < 0) {
		handle_error_exit("Failed opening /dev/urandom for read");
	}

	// read len_of_read bytes from urandom
	char* read_buf = calloc((len_to_read + 1), sizeof(char));

	if (read_buf == NULL) {
		handle_error_exit("Failed to allocate memory for read buffer");
	}

	// do a read
	// TODO: assumre this read is good
	int bytes_read = read(urand_fd, read_buf, len_to_read);
	if (bytes_read == -1) {
		handle_error_exit("Failed reading from urandom");
	}
	read_buf[len_to_read] = '\0';


	// write to server
	int wrote_bytes = 0;
	while (wrote_bytes < len_to_read) {
		int bytes_wrote = write(sockfd, read_buf + wrote_bytes, len_to_read - wrote_bytes);
		if (bytes_wrote == -1) {
			handle_error_exit("Failed to write to server");
		}
		wrote_bytes += bytes_wrote;

	}

	// read from server

	//************* Deprecated***************************************
//	int read_bytes = 0;
//	char read_server_buf[sizeof(unsigned int) + 1];
//	memset(read_server_buf, 0, sizeof(read_server_buf));
//	while (read_bytes < sizeof(unsigned int)) {
//		int done_read = read(sockfd, read_server_buf + read_bytes, sizeof(unsigned int) - read_bytes);
//		if (done_read == -1) {
//			handle_error_exit("Failed to read from server");
//		}
//		read_bytes += done_read;
//	}
//	read_server_buf[sizeof(unsigned int)] = '\0';

	// convert read from server to unsinged int
	//rc = 0;
	//unsigned int C = convertCharArrayToUsingedInt(read_server_buf, sizeof(read_server_buf), &rc);

	int32_t in_val;
	char* in_stream = (char*)&in_val;
	int size_of_val = sizeof(in_val);
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
	return EXIT_SUCCESS;
}
