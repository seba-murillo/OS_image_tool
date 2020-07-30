/*
 ============================================================================
 Name        : server_main.c
 Author      : Seba Murillo
 Version     : 1.0
 Copyright   : GPL
 Description : main service
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <global.h>
#include <message.h>
#include <signal.h>

#define MAX_ADDRESS_LENGTH 22 ///< maximum IP address length
#define MAX_CONNECTIONS 1 ///< maximum amount of simultaneous connections
#define SHOW_HELP 2 ///< special return code for process_command()

#define verbose ///< verbose mode

int FD_socket;
int FD_comms_socket;
unsigned short USER_LOGGED_IN = FALSE;
unsigned short CONNECTIONS = 0;

int process_command(char* command);
char* recv_client(char* buffer);
void send_client(const char* buffer);
void setup_client_connection(int argc, char* argv[]);
void SIGKILL_handler();
void close_FDs();
void setup_message_queue();
void delete_message_queue();

/// main server program entrypoint
/// @returns 1 on error, 0 on success
int main(int argc, char* argv[]){
	printf("> Launching [SERVER_MAIN]\n\n");
	// kill signal registration
	signal(SIGINT, SIGKILL_handler); // removes persistent queue
	// exit handler registration
	if((atexit(close_FDs)) != 0){
		fprintf(stderr, "ERROR: registering exit handler (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if((atexit(delete_message_queue)) != 0){
		fprintf(stderr, "ERROR: registering exit handler (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	// create systemV message queue
	setup_message_queue();
	// setup socket
	setup_client_connection(argc, argv);
	// setup client_address and start listening
	struct sockaddr_in client_address;
	int client_length = sizeof(client_address);
	listen(FD_socket, MAX_CONNECTIONS); // MAX_CONNECTIONS = 1
	char command[BUFFER_SIZE];
	while(TRUE){
		FD_comms_socket = accept(FD_socket, (struct sockaddr*) &client_address, (socklen_t*) &client_length);
		send_client(OK); // confirm connection to client
		printf("[SERVER_MAIN]: new connection ACCEPTED\n");
		while(TRUE){
			// get message
			recv_client(command);
			int result = process_command(command);
			if(result == FALSE){
				printf("[SERVER_MAIN]: client disconnected\n");
				send_client(command); // responds to client
				break; // break inner loop and wait for new client
			}else if(result == SHOW_HELP){
				sprintf(command, "> available commands:\n");
				strcat(command, TAB "clear\n");
				strcat(command, TAB "login <user> <pass>\n");
				strcat(command, TAB "user ls\n");
				strcat(command, TAB "user <pass>\n");
				strcat(command, TAB "file ls\n");
				strcat(command, TAB "file down <image_ID> <target>\n");
				strcat(command, TAB "exit\n\n");
			}
			send_client(command); // responds to client
		}
		CONNECTIONS--; // client left
		if(close(FD_comms_socket) < 0){ // close socket and wait for new client
			fprintf(stderr, "ERROR: closing FD_comms_socket (%s)\n", strerror(errno));
			continue;
		}
	}
	printf("> Closing [SERVER_MAIN]\n\n");
	return EXIT_SUCCESS;
}

/// handles connection between server and client
void setup_client_connection(int argc, char* argv[]){
	int port;
	if(argc > 2){
		fprintf(stderr, "> use: %s [port]\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	if(argc == 2){
		if((port = (int) strtol(argv[1], NULL, 10)) == 0){
		//if((port = atoi(argv[1]) == 0)){
			fprintf(stderr, "ERROR: invalid port (%d)\n", port);
			exit(EXIT_FAILURE);
		}
	}else{ // argc = 1
		printf("[SERVER_MAIN]: WARNING -> invalid port, using default SERVER_MAIN_PORT (%d)\n", SERVER_MAIN_PORT);
		port = SERVER_MAIN_PORT;
	}
	// create socket
	FD_socket = socket(AF_INET, SOCK_STREAM, 0); // TCP
	if(FD_socket == INEX){
		fprintf(stderr, "ERROR: creating FD_socket (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	// porque sockopt:
	// https://stackoverflow.com/questions/5106674/error-address-already-in-use-while-binding-socket-with-address-but-the-port-num
	// if server gets SIGKILL this makes the socket reusable instantly
	if(setsockopt(FD_socket, SOL_SOCKET, SO_REUSEADDR, &(int) {1}, sizeof(int)) < 0){
		fprintf(stderr, "ERROR: setsockopt() in socket FD_client (%s)\n", strerror(errno));
	}
	// setup server_address
	struct sockaddr_in server_address;
	memset((char*) &server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = INADDR_ANY;
	server_address.sin_port = htons((uint16_t) port);
	// bind to socket
	if(bind(FD_socket, (struct sockaddr*) &server_address, sizeof(server_address)) < 0){
		fprintf(stderr, "ERROR: binding socket (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
#ifdef verbose
	printf("> verbose defined:\n");
	printf(TAB "server process ID:    %d\n", getpid());
	printf(TAB "serverport:          %d\n\n", ntohs(server_address.sin_port));
	// htonl, htons, ntohl, ntohs - convert values between host and network byte orde
#endif
}

/// sends a message to the connected client
///
/// sends a message to the connected client using TCP socket
/// @param buffer the message to be sent
void send_client(const char* buffer){
	ssize_t io_count = send(FD_comms_socket, buffer, strlen(buffer), 0); // responds to client
	if(io_count < 0){
		fprintf(stderr, "ERROR: writing FD_comms_socket (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
}

/// receive a message from the connected client
///
/// receive a message from the connected client using TCP socket
/// @param buffer the buffer in which to store the message, **the buffer must be at least BUFFER_SIZE bytes long**
/// @returns a pointer to the provided buffer
char* recv_client(char* buffer){
	memset(buffer, 0, BUFFER_SIZE);
	ssize_t io_count = recv(FD_comms_socket, buffer, BUFFER_SIZE - 1, 0); // read command
	if(io_count < 0){
		fprintf(stderr, "ERROR: reading FD_comms_socket (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	return buffer;
}

/*
 help
 login <user> <password>
 user ls
 user passwd <new_pass>
 file ls
 file down <image_name> [opt1] [opt2] [opt3]
 exit
 */

/// processes a command
///
/// processes a command obtained from the client through recv_client() and stores the response in the same buffer (command)
/// @param command the command to be processed
/// @returns 1 if client is allowed another command, 0 if the client disconnects or is no longer allowed to type commands, 2 if client needs to be shown command help
int process_command(char* command){
	static unsigned short login_strikes = 0;
	if(command == NULL || strcmp(command, "") == 0){
		USER_LOGGED_IN = FALSE;
		login_strikes = 0;
		return FALSE; // break inner loop and wait for new client
	}
	printf("[SERVER_MAIN]: processing command [%s]\n", command);
	if(strcmp("clear", command) == 0){
		sprintf(command, "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
		return TRUE;
	}else if(strcmp("exit", command) == 0){
		USER_LOGGED_IN = FALSE;
		login_strikes = 0;
		return FALSE; // break inner loop and wait for new client
	}
	char message[MESSAGE_SIZE];
	char* arg = strtok(command, " ");
	if(USER_LOGGED_IN == FALSE){
		if(strcmp("login", arg) != 0){
			sprintf(command, "[SERVER_MAIN]: use login <user> <pass> before other commands\n");
			return TRUE;
		}
		char* user = strtok(NULL, " ");
		char* pass = strtok(NULL, " ");
		if(user == NULL || pass == NULL){
			sprintf(command, "[SERVER_AUTH]: empty user and/or password, try again\n");
			return TRUE;
		}
		printf("[SERVER_MAIN]: delegating login to [SERVER_AUTH]\n");
		sprintf(message, "AUTH LOG %s %s", user, pass); // ask AUTH to login user
		send_msg(SERVER_AUTH_MSG_TYPE, message); // send the querry to AUTH
		get_msg(SERVER_MAIN_MSG_TYPE, message); // get AUTH response
		// "[SERVER_AUTH]: incorrect user and/or password, try again\n"
		if(strncmp("[SERVER_AUTH]: incorrect", message, strlen("[SERVER_AUTH]: incorrect")) == 0){
			if(++login_strikes > 2){
				sprintf(command, "[SERVER_MAIN]: incorrent AGAIN, you are now BANNED (not really)\n");
				return FALSE;
			}
		}else{ // successful login
			USER_LOGGED_IN = TRUE;
			login_strikes = 0;
		}
		strcpy(command, message); // copy response to buffer, which will be sent to client
		return TRUE; // return to loop where client will be answered
	}else{ // user is logged in
		if(strcmp("login", arg) == 0){
			sprintf(command, "[SERVER_MAIN]: you are already logged in\n");
			return TRUE;
		}
		if(strcmp("help", arg) == 0){
			return SHOW_HELP;
		}else if(strcmp("user", arg) == 0){
			arg = strtok(NULL, " ");
			if(arg == NULL){
				return SHOW_HELP;
			}
			if(strcmp("ls", arg) == 0){
				sprintf(message, "AUTH LS"); // ask auth server for LS
				send_msg(SERVER_AUTH_MSG_TYPE, message); // ask AUTH to list users
				get_msg(SERVER_MAIN_MSG_TYPE, message); // get AUTH response
				strcpy(command, message); // copy response to buffer, which will be sent to client
				return TRUE; // return to loop where client will be answered
			}else if(strcmp("passwd", arg) == 0){
				arg = strtok(NULL, " ");
				if(arg == NULL){
					return SHOW_HELP;
				}
				sprintf(message, "AUTH PASS %s", arg); // ask AUTH for password change
				send_msg(SERVER_AUTH_MSG_TYPE, message); // send the querry to AUTH
				get_msg(SERVER_MAIN_MSG_TYPE, message); // get AUTH response
				strcpy(command, message);  // copy response to buffer, which will be sent to client
				return TRUE; // return to loop where client will be answered
			}
		}else if(strcmp("file", arg) == 0){
			arg = strtok(NULL, " ");
			if(arg == NULL){
				return SHOW_HELP;
			}
			if(strcmp("ls", arg) == 0){
				sprintf(message, "FILE LS"); // ask FILE to list files
				send_msg(SERVER_FILE_MSG_TYPE, message); // send the querry to FILE
				get_msg(SERVER_MAIN_MSG_TYPE, message); // get FILE response
				strcpy(command, message); // copy response to buffer, which will be sent to client
				return TRUE; // return to loop where client will be answered
			}else if(strcmp("down", arg) == 0){
				sprintf(message, "FILE DOWN "); // ask FILE for FILE TRANSFER
				strcat(message, command + strlen("FILE DOWN ")); // transfer args to SERVER_FILE
				send_msg(SERVER_FILE_MSG_TYPE, message); // send the querry to FILE
				get_msg(SERVER_MAIN_MSG_TYPE, message); // get FILE response
				strcpy(command, message);  // copy response to buffer, which will be sent to client
				return TRUE; // return to loop where client will be answered
			}
		}
		sprintf(command, "[SERVER_MAIN]: command does not exist, use 'help' to see available commands\n");
		return TRUE;
	}
	return TRUE;
}

/// handles the ctrl+C signal, closing the program properly
void SIGKILL_handler(){
	exit(EXIT_SUCCESS);
}
/// closes open file descriptors
void close_FDs(){
	printf("[SERVER_MAIN]: closing file descriptors...\n");
	// ASK: necesario ver errores de esto?
	close(FD_socket);
	close(FD_comms_socket);
	/*
	 if(close(FD_socket) < 0){
	 fprintf(stderr, "ERROR: closing socket FD_socket (%s)\n", strerror(errno));
	 exit(EXIT_FAILURE);
	 }
	 if(close(FD_comms_socket) < 0){
	 fprintf(stderr, "ERROR: closing socket FD_comms_socket (%s)\n", strerror(errno));
	 exit(EXIT_FAILURE);
	 }
	 */
}
/// starts a systemV message queue for communication with SERVER_AUTH and SERVER_FILE
void setup_message_queue(){
	key_t key;
	printf("[SERVER_MAIN]: launching message queue...\n");
	if((key = ftok(MESSAGE_QUEUE_KEY, 66)) < 0){
		fprintf(stderr, "ERROR: getting systemV queue key (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	Q_ID = msgget(key, 0666 | IPC_CREAT);
}

/// removes the systemV message queue for communication with SERVER_AUTH and SERVER_FILE
/// @note this will terminate both SERVER_AUTH and SERVER_FILE
void delete_message_queue(){
	printf("\n[SERVER_MAIN]: removing message queue...\n");
	if(msgctl(Q_ID, IPC_RMID, NULL) < 0){
		fprintf(stderr, "ERROR: deleting systemV queue (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
}
