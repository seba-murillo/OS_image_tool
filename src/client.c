/*
 ============================================================================
 Name        : client.c
 Author      : Seba Murillo
 Version     : 1.0
 Copyright   : Your copyright notice
 Description : client
 ============================================================================
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <ipcheck.h>
#include <global.h>
#include <MBR.h>
#include <openssl/md5.h>
#include <signal.h>
#include <stdint.h>

//#define debug
#define verbose ///< verbose mode
//#define FORK_MODE ///< creates child process for file transfer **DO NOT USE**
#define MAX_CONNECTION_ATTEMPTS 3 ///< maximum amount of connection attempts

char* get_MD5(const char* target);
void SIGKILL_handler();
void setup_server_connection(int argc, char* argv[]);
void setup_file_download();
void close_FDs();

int FD_socket; ///< main sever socket file descriptor
char server_IP[SIZE_IP]; ///< main sever IP

/// client program entrypoint
///
/// client program entrypoint, the correct use is ./client <IP:port>
/// example: ./client 192.80.80.80:1234
/// @returns 1 on error, 0 on success
int main(int argc, char** argv){
	printf("> Launching [CLIENT]\n\n");
	// kill signal registration
	signal(SIGINT, SIGKILL_handler); // removes persistent queue
	// exit handler registration
	if((atexit(close_FDs)) != 0){
		fprintf(stderr, "ERROR: registering exit handler (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	setup_server_connection(argc, argv);
	char buffer[BUFFER_SIZE];
	memset(buffer, 0, BUFFER_SIZE);
	recv(FD_socket, buffer, BUFFER_SIZE, 0); // wait for server confirmation "OK"
	if(strcmp(buffer, OK) != 0){
		printf("[SERVER_MAIN]: connection refused\n");
		exit(EXIT_SUCCESS);
	}
	// connected
	// get commands and send to server:
	printf("> connection established, use 'login <user> <password>' to login\n");
	while(TRUE){
		do{
			// get command
			printf("> ");
			memset(buffer, 0, BUFFER_SIZE);
			if(fgets(buffer, BUFFER_SIZE - 1, stdin) == NULL){
				fprintf(stderr, "ERROR: reading command, try again (%s)\n", strerror(errno));
				continue;
			}
			buffer[strlen(buffer) - 1] = '\0';
		}while(strcmp(buffer, "") == 0);
		ssize_t io_count = (int) send(FD_socket, buffer, strlen(buffer), 0); // send to server
		if(io_count < 0){
			fprintf(stderr, "ERROR: writing socket (%s)\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		if(strcmp("exit", buffer) == 0){
			printf("[CLIENT]: exiting...\n");
			exit(EXIT_SUCCESS);
		}
		// recieve response
		memset(buffer, 0, BUFFER_SIZE);
		io_count = recv(FD_socket, buffer, BUFFER_SIZE - 1, 0); // read response from server
		if(io_count < 0){
			fprintf(stderr, "ERROR: reading socket (%s)\n", strerror(errno));
			exit(EXIT_FAILURE);
		}else if(io_count == 0){
			fprintf(stderr, "ERROR: [SERVER_MAIN] is offline\n");
			exit(EXIT_FAILURE);
		}
		if(strcmp(START_FILE_TRANSFER_MSG, buffer) == 0){
			setup_file_download();
			continue;
		}
		printf("%s", buffer); // printf server response to client
	}
	return (EXIT_SUCCESS);
}

/// handles connection between client and server
void setup_server_connection(int argc, char* argv[]){
	// get args
	char input[SIZE_IP + 5]; // solo IPv4
#ifdef debug
	strcpy(input, "127.0.0.1");
#else
	if(argc != 2){
		fprintf(stderr, "ERROR: incorrect syntax, use %s <IP:port>\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	strncpy(input, argv[1], sizeof(input));
#endif
//-------------------------------------
	// check IP
	strcpy(server_IP, strtok(input, ":"));
	char* port_ptr = strtok(NULL, ":");
	if(validate_ip(server_IP) == FALSE){
		fprintf(stderr, "ERROR: invalid IP (%s)", server_IP);
		exit(EXIT_FAILURE);
	}
	// IP is valid
	uint16_t port;
	if(port_ptr == NULL){
		printf("[CLIENT] WARNING: invalid port, using default SERVER_MAIN_PORT (%d)\n", SERVER_MAIN_PORT);
		port = SERVER_MAIN_PORT;
	}else{
		port = (uint16_t) strtol(port_ptr, NULL, 10);
		if(port < 1){
			fprintf(stderr, "ERROR: invalid port (%d)\n", port);
			exit(EXIT_FAILURE);
		}
	}
	// port is valid (or relaced)
#ifdef verbose
	printf("\n> verbose defined:\n");
	printf("    IP:   %s -> valid: %d\n", server_IP, validate_ip(server_IP));
	printf("    port: %d\n\n", port);
#endif
	// create socket for connection:
	FD_socket = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in server_address;
	struct hostent* server = gethostbyname(server_IP);
	// server exists?
	if(server == NULL){
		fprintf(stderr, "ERROR: server not found (%s)\n", server_IP);
		exit(EXIT_FAILURE);
	}
	// server_address setup:
	memset((char*) &server_address, '0', sizeof(server_address));
	server_address.sin_family = AF_INET;
	// typedef uint16_t in_port_t;
	server_address.sin_port = htons(port);
	// wtf is this??
	bcopy((char*) server->h_addr, (char *) &server_address.sin_addr.s_addr, (size_t) server->h_length);
	// connect:
	int connection;
	int attempts = 0;
	do{
		connection = connect(FD_socket, (struct sockaddr*) &server_address, sizeof(server_address));
		if(connection < 0){
			if(++attempts > MAX_CONNECTION_ATTEMPTS){
				fprintf(stderr, "[CLIENT]: SERVER_MAIN is not responding, exiting...\n");
				exit(EXIT_FAILURE);
			}
			printf("[CLIENT]: failed to connect to SERVER_MAIN, retrying in 3...");
			fflush(stdout);
			sleep(1);
			printf("2...");
			fflush(stdout);
			sleep(1);
			printf("1...");
			fflush(stdout);
			sleep(1);
			printf("\n");
			continue;
		}
		break; // connected
	}while(TRUE);
}

/// closes open file descriptors
void close_FDs(){
	printf("[CLIENT]: closing file descriptors...\n");
	if(close(FD_socket) < 0){
		fprintf(stderr, "ERROR: closing socket FD_socket (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
}

/// prepares the client for file transfer
///
/// handles the connection between the client and the file server, also writes the file to the selected device
void setup_file_download(){
#ifdef FORK_MODE
	// fork
	pid_t pid;
	pid = fork();
	if(pid < 0){
		fprintf(stderr, "ERROR: forking process (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if(pid > 0){ // parent process returns
		sleep(5);
		return;
	}
	// child process -> create socket for connection:
	if(close(FD_socket) < 0){ // close SERVER_MAIN socket
		fprintf(stderr, "ERROR: closing socket for file transfer (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
#endif
	int FD_SERVER_FILE = socket(AF_INET, SOCK_STREAM, 0);
	if(FD_SERVER_FILE == INEX){
		fprintf(stderr, "ERROR: creating transfer socket (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	struct sockaddr_in server_address;
	if(validate_ip(server_IP) == FALSE){
		fprintf(stderr, "ERROR: invalid SERVER_FILE IP (%s)", server_IP);
		exit(EXIT_FAILURE);
	}
	struct hostent* server = gethostbyname(server_IP); // server already exists
	// server_address setup:
	memset((char*) &server_address, '0', sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(SERVER_FILE_PORT); // SERVER_FILE_PORT = 37778
	bcopy((char*) server->h_addr, (char *) &server_address.sin_addr.s_addr, (size_t) server->h_length);
	// connect:
	printf("[CLIENT]: connecting to SERVER_FILE...");
	fflush(stdout);
	int attempts = 0;
	do{
		if(connect(FD_SERVER_FILE, (struct sockaddr*) &server_address, sizeof(server_address)) < 0){
			if(++attempts > MAX_CONNECTION_ATTEMPTS){
				fprintf(stderr, "[CLIENT]: SERVER_FILE is not responding, exiting...\n");
				exit(EXIT_FAILURE);
			}
			printf("[CLIENT]: failed to connect to SERVER_FILE, retrying in 3...");
			fflush(stdout);
			sleep(1);
			printf("2...");
			fflush(stdout);
			sleep(1);
			printf("1...");
			fflush(stdout);
			sleep(1);
			printf("\n");
			continue;
		}
		break; // connected
	}while(TRUE);
	// connected
	printf("done\n");
/*
	// get filename
	char filename[MAX_FILENAME_SIZE];
	memset(filename, '\0', MAX_FILENAME_SIZE);
#ifdef verbose
	printf("[CLIENT]: receiving filename\n");
#endif
	if(recv(FD_SERVER_FILE, filename, MAX_FILENAME_SIZE, 0) < 0){
		fprintf(stderr, "ERROR: getting file transfer filename (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
#ifdef verbose
	printf("[CLIENT]: filename is: [%s]\n", filename);
#endif
*/
#ifdef verbose
	printf("[CLIENT]: receiving target\n");
#endif
	// get device
	char target[MAX_FILENAME_SIZE];
	memset(target, '\0', MAX_FILENAME_SIZE);
	if(recv(FD_SERVER_FILE, target, sizeof(target), 0) < 0){
		fprintf(stderr, "ERROR: getting file transfer target (%s)\n", strerror(errno));
		//exit(EXIT_FAILURE);
		return;
	}
#ifdef verbose
	printf("[CLIENT]: target is: [%s]\n", target);
#endif
	// test if client has write permissions
#ifdef verbose
	printf("[CLIENT]: opening target [%s]\n", target);
#endif
	FILE* file_output;
	if((file_output = fopen(target, "wb")) == NULL){
		// let SERVER_FILE know the transfer is cancelled
		if(send(FD_SERVER_FILE, NO, strlen(NO), 0) < 0){ // SEND device
			fprintf(stderr, "ERROR: sending cancellation to [SERVER_FILE] (%s)\n", strerror(errno));
			return;
		}
		switch(errno){
			case EACCES: {/* Permission denied */
				fprintf(stderr, "ERROR: no permission on client to write on %s, restart with sudo\n", target);
				break;
			}
			case EISDIR: {/* Is a directory */
				fprintf(stderr, "ERROR: no filename specified\n");
				fprintf(stderr, TAB TAB "example: file down 3 /home/user/Desktop/test_file");
				fprintf(stderr, TAB TAB "example: file down 1 /dev/sdc");
				break;
			}
			case ENOENT: {/* No such file or directory */
				fprintf(stderr, "ERROR: invalid file ID, use 'file ls' to get a list of files and their IDs\n");
				break;
			}
			default: {
				fprintf(stderr, "ERROR: unable to open %s (%s)\n", target, strerror(errno));
				break;
			}
		}
		return;
	}
	// send OK to start transfer
	if(send(FD_SERVER_FILE, OK, strlen(OK), 0) < 0){ // SEND OK
		fprintf(stderr, "ERROR: sending OK to [SERVER_FILE] (%s)\n", strerror(errno));
		fclose(file_output);
		return;
	}
#ifdef verbose
	printf("[CLIENT]: starting transfer...\n");
#endif
	long read;
	unsigned long total = 0;
	char buffer[BUFFER_SIZE];
	while((read = recv(FD_SERVER_FILE, buffer, sizeof(buffer), 0)) > 0){
		if(read < 0){
			fprintf(stderr, "ERROR: transfering (%s)\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		fwrite(buffer, 1, (unsigned long) read, file_output);
		//printf("[CLIENT] read %ld bytes\n", read);
		total += (unsigned) read;
		memset(buffer, '0', sizeof(buffer));
	}
	printf("[CLIENT]: transfer complete, total: [%ld] bytes\n", total);
	fclose(file_output);
	// get MD5
	printf("[CLIENT]: MD5 is [%s]\n", get_MD5(target));
	// print partitions
	print_partition(target);
}

/// calculates MD5 hash for *target* file
/// @param target the file or device to be hashed
/// @returns a pointer to the MD5 hash
char* get_MD5(const char* target){
	int FD = open(target, O_RDONLY);
	if(FD < 0){
		fprintf(stderr, "ERROR: opening %s for MD5 hash (%s)\n", target, strerror(errno));
		exit(EXIT_FAILURE);
	}
	// get size
	struct stat stat_struct;
	if(stat(target, &stat_struct) != 0){
		fprintf(stderr, "ERROR: reading file size (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	// init MD5
	unsigned char MD5[MD5_DIGEST_LENGTH];
	char buffer[512];
	MD5_CTX CTX;
	long int reads = stat_struct.st_size / 512;
	long int R;
	MD5_Init(&CTX);
	// calculate MD5
	for(int i = 0;i < reads;i++){
		R = read(FD, buffer, sizeof(buffer));
		if(R < 0){
			fprintf(stderr, "ERROR: reading %s for MD5 hash (%s)\n", target, strerror(errno));
			return NULL;
		}
		MD5_Update(&CTX, buffer, (unsigned long) R);
	}
	if(stat_struct.st_size % 512 != 0){
		R = read(FD, buffer, (long unsigned) (stat_struct.st_size % 512));
		if(R < 0){
			fprintf(stderr, "ERROR: reading %s for MD5 hash (%s)\n", target, strerror(errno));
			return NULL;
		}
		MD5_Update(&CTX, buffer, (unsigned long) R);
	}
	MD5_Final(MD5, &CTX);
	close(FD);
	// transform to string
	char* result = calloc(2 * MD5_DIGEST_LENGTH + 1, sizeof(char));
	char tmp[10];
	for(int i = 0;i < MD5_DIGEST_LENGTH;i++){
		sprintf(tmp, "%02x", MD5[i]);
		strcat(result, tmp);
	}
	return result;
}

/// handles the ctrl+C signal, closing the program properly
void SIGKILL_handler(){
	printf("\n[CLIENT]: exiting...\n");
	send(FD_socket, "exit", strlen("exit"), 0); // si tira error, importa?
	exit(EXIT_SUCCESS);
}
